# The atomic counter race

hearth counts active connections and total requests. Those counters are
updated by the event-loop thread (connection opened) and by every worker
thread (request received, connection closed) at the same time. This note shows
what happens if they are plain `int`s, and why `std::atomic` fixes it.

All output below is real, captured on the development machine (Fedora 44,
GCC 16.2, Clang 22.1, 16 hardware threads) with
[`docs/race-demo/race_demo.cpp`](race-demo/race_demo.cpp): 8 threads, each
incrementing a shared counter 100 000 times. The correct answer is 800 000.

## What goes wrong with a plain `int`

`++counter` looks like one operation, but the CPU does three:

```
load  counter -> register     ; both threads read 41
add   register, 1             ; both compute 42
store register -> counter     ; both write 42: one increment is lost
```

Two threads can interleave between the load and the store, and one update
silently disappears. The more threads and the tighter the loop, the more is
lost.

**Debug build (`-O0`), three runs:**

```
plain int counter: expected 800000, got 144074 (655926 lost)
plain int counter: expected 800000, got 125286 (674714 lost)
plain int counter: expected 800000, got 221388 (578612 lost)
```

**Release build (`-O3`), three runs:**

```
plain int counter: expected 800000, got 800000 (0 lost)
plain int counter: expected 800000, got 800000 (0 lost)
plain int counter: expected 800000, got 700000 (100000 lost)
```

The release numbers are the more dangerous ones. The program is correct most
of the time, so testing doesn't catch it. When it fails, it loses *exactly*
100 000, one whole thread's work. The data race is undefined behaviour, so the
optimizer may assume no other thread touches `counter`. It turned each
thread's loop into a single `counter += 100000`, and two of those collided.
The bug isn't really "a few lost increments". A race makes the program's
meaning undefined, and the compiler is entitled to transform it in ways that
fail differently on every build.

## What ThreadSanitizer says

Built with `-fsanitize=thread` (the `tsan` preset), the plain version is
flagged right away, even on runs where the final number happens to be right
(stack frames trimmed):

```
==================
WARNING: ThreadSanitizer: data race (pid=30266)
  Write of size 4 at 0x00000189d7d8 by thread T2:
    #0 main::$_0::operator()() const docs/race-demo/race_demo.cpp:42
    ...
  Previous write of size 4 at 0x00000189d7d8 by thread T1:
    #0 main::$_0::operator()() const docs/race-demo/race_demo.cpp:42
    ...
  Thread T2 (tid=30269, running) created by main thread at:
    #0 pthread_create
    ...
    #6 main docs/race-demo/race_demo.cpp:40

SUMMARY: ThreadSanitizer: data race docs/race-demo/race_demo.cpp:42 in main::$_0::operator()() const
==================
plain int counter: expected 800000, got 417773 (382227 lost)
ThreadSanitizer: reported 1 warnings
```

TSan doesn't need the race to cause a wrong answer. It tracks
happens-before relationships between memory accesses. Two writes to the same
address from different threads, with no synchronization ordering them, is a
race by definition, whatever the timing on a given run.

## Why `std::atomic` fixes it

```
std::atomic counter: expected 800000, got 800000 (0 lost)
```

(and TSan reports nothing.) `counter.fetch_add(1)` compiles to a single
indivisible read-modify-write (`lock xadd` on x86-64, `ldadd` or an LL/SC loop
on ARM). No other thread can observe or modify the value between the read and
the write, so no increment is lost. It is also a synchronization operation as
far as the C++ memory model is concerned, so the program has no data race and
no undefined behaviour. The optimizer can no longer fold the loop into one
addition.

hearth uses `std::memory_order_relaxed` for its counters
([`src/server/metrics.hpp`](../src/server/metrics.hpp)). Relaxed still
guarantees atomicity and a single total order of modifications *per counter*.
What it skips is ordering against *other* memory, which a statistics counter
doesn't need: no thread reads a counter and then relies on it to access other
data. Relaxed ordering costs the same as sequentially consistent on x86 and is
cheaper on ARM.

## Alternatives considered

| Approach | Verdict |
| --- | --- |
| `int` + `std::mutex` | Correct, but a lock per request is heavier than the work being counted. |
| `volatile int` | **Wrong.** `volatile` stops the compiler from folding the loop, but the load/add/store is still three separate steps and still a data race. |
| Per-thread counters summed on read | Fastest under heavy contention (no shared cache line), but more code. A sensible next step if `/stats` counters ever show up in a profile. |
| `std::atomic` (relaxed) | Simple, correct, cheap. **Chosen.** |

## Reproduce it

```sh
# Plain build: watch increments go missing
cmake -S . -B build/race -DHEARTH_BUILD_RACE_DEMO=ON -DHEARTH_BUILD_TESTS=OFF
cmake --build build/race --target race_demo
./build/race/docs/race-demo/race_demo plain
./build/race/docs/race-demo/race_demo atomic

# Under ThreadSanitizer, with CTest checks that the plain version is reported
# and the atomic version is clean:
cmake -S . -B build/race-tsan -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
      -DHEARTH_SANITIZE=thread -DHEARTH_BUILD_RACE_DEMO=ON -DHEARTH_BUILD_TESTS=OFF
cmake --build build/race-tsan
ctest --test-dir build/race-tsan --output-on-failure
```

The demo is opt-in (`HEARTH_BUILD_RACE_DEMO=OFF` by default) so the regular
`tsan` preset keeps reporting zero races. The real counters are hammered from
16 threads in `tests/metrics_test.cpp` (`MetricsConcurrency.*`), and those
tests run clean under TSan in CI.
