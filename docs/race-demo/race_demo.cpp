// Demonstrates why hearth's counters are std::atomic.
//
//   race_demo plain    increment a plain int from 8 threads (data race, UB)
//   race_demo atomic   increment a std::atomic<int> from 8 threads
//
// Build it with -DHEARTH_BUILD_RACE_DEMO=ON, ideally in the tsan preset, and
// see docs/atomic-counter-race.md for real output and an explanation.
#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace {

constexpr int kThreads = 8;
constexpr int kIncrements = 100000;

int plain_counter = 0;              // NOLINT: deliberately racy global
std::atomic<int> atomic_counter{0}; // NOLINT

template <typename Fn> void run_threads(Fn fn) {
  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back(fn);
  }
  for (auto &t : threads) {
    t.join();
  }
}

} // namespace

int main(int argc, char **argv) {
  const bool plain = argc > 1 && std::strcmp(argv[1], "plain") == 0;
  const int expected = kThreads * kIncrements;
  int actual = 0;
  if (plain) {
    run_threads([] {
      for (int i = 0; i < kIncrements; ++i) {
        ++plain_counter; // load, add, store: three steps another thread can interleave with
      }
    });
    actual = plain_counter;
  } else {
    run_threads([] {
      for (int i = 0; i < kIncrements; ++i) {
        atomic_counter.fetch_add(1, std::memory_order_relaxed);
      }
    });
    actual = atomic_counter.load();
  }
  std::printf("%s counter: expected %d, got %d (%d lost)\n", plain ? "plain int" : "std::atomic",
              expected, actual, expected - actual);
  return actual == expected ? 0 : 1;
}
