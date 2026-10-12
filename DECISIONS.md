# Design decisions

Running log of judgment calls made while building hearth. Each entry: the
question, the choice, and why.

## Build & tooling

- **Presets require CMake 3.21; the project itself requires 3.16.**
  `CMakePresets.json` schema v3 (needed for `testPresets` with environment and
  `noTestsAction`) needs CMake 3.21. A plain `cmake -S . -B build` still works
  on 3.16.
- **Ninja is the preset generator.** It is fast and parallel by default and the
  same on Linux and macOS. Makefiles would need a per-machine `-j`.
- **GoogleTest v1.17.0 comes from a release tarball pinned by SHA-256**, not a
  git clone. That is faster, reproducible, and easy to cache in CI.
- **Sanitizer flags are applied globally** (`add_compile_options`) so GoogleTest
  is instrumented too. Mixing instrumented and uninstrumented code produces
  false positives under TSan. Warning flags (`-Werror`) apply only to
  first-party targets through the `hearth_options` interface library.
- **`-Wshadow` added** on top of the required `-Wall -Wextra -Wpedantic`.
  It is cheap and catches real bugs in constructors.
- **Developer tools installed with pip on the build machine** (`clang-format`,
  `clang-tidy`, `ninja`, `actionlint-py`) because there was no root access. CI
  pins the same clang-format version through pip so formatting is identical.

## Git

- `origin` fetches over HTTPS (`https://github.com/depuc8/hearth.git`) as
  specified, but has an SSH **push** URL, because the build machine had SSH
  keys and no HTTPS credentials.

## Networking

- **poll(), not epoll.** poll() is POSIX, so one code path works on Linux and
  macOS. Its O(n) cost per wakeup only matters with many thousands of idle
  connections, far past this project's scale. An epoll/kqueue backend is on the
  roadmap.
- **getaddrinfo for binding**, so `--host` accepts IPv4, IPv6 or a hostname.
  Default host is `0.0.0.0` (IPv4 any).
- **SIGPIPE**: `MSG_NOSIGNAL` on Linux, `SO_NOSIGPIPE` on macOS, and the binary
  also ignores SIGPIPE process-wide as a backstop.
- **EMFILE/ENFILE on accept** leaves the connection in the backlog and retries
  on the next loop iteration instead of throwing.

## HTTP parsing

- **Bare LF line endings are accepted** (RFC 9112 §2.2 allows it). A bare CR
  anywhere else in a line is rejected with 400.
- **Leading empty lines before the request line are ignored** (RFC 9112 §2.2).
  They still count toward the header-size limit, so a CRLF flood is bounded.
- **The header limit covers the request line plus all header lines.** If the
  limit is hit while still in the request line the answer is 414; otherwise
  431. The limit is checked as bytes arrive, so a header with no newline cannot
  grow without bound.
- **Only HTTP/1.0 and HTTP/1.1 are accepted.** A well-formed but different
  version (`HTTP/2.0`, `HTTP/1.2`) gets 505; a malformed one gets 400.
- **Host is required on HTTP/1.1, and more than one Host is a 400** (RFC 9112
  §3.2).
- **Transfer-Encoding is not supported**: 501. Transfer-Encoding together with
  Content-Length is a 400 (request-smuggling defence).
- **Duplicate Content-Length** is accepted only if every value is identical.
  Comma-separated lists like `5, 5` are rejected.
- **The path is percent-decoded once, in the parser.** The router and the
  static handler see the decoded path; `%00` is rejected. The query string
  stays raw.
- **Absolute-form targets** (`GET http://host/x`) are reduced to their path;
  the authority is ignored in favour of Host.
- **`#` in the target is rejected**: clients must not send fragments.
- **Responses always say `HTTP/1.1`**, even to 1.0 clients (RFC 9110 §6.2).

## Routing

- **Precedence: exact > longest prefix > first registered.** Prefixes match on
  segment boundaries (`/api` matches `/api/x`, not `/apix`).
- **Dispatch falls through by method.** If the most specific route doesn't
  accept the method, a less specific one that does gets the request. Only
  when no path-matching route accepts it is the answer 405, and `Allow` is the
  union of methods from every path-matching route.
- **GET routes also answer HEAD.** The connection layer drops the body but
  keeps Content-Length.
- **Methods are case-sensitive** (RFC 9110 §9.1), so `get` is a 405, not GET.

## Static files

- **Any `..` segment is a 403, even one that would stay inside the root.**
  Rejecting is simpler and safer than normalizing. Canonicalization plus a
  component-wise containment check is the second layer and also catches
  symlinks that escape the root.
- **Directories serve `index.html`, with or without a trailing slash.** There
  is no redirect to the slash form, so relative links inside `/docs` (no
  slash) resolve against `/`. Kept simple; noted under limitations.
- **Files are read fully into memory.** That's fine for a demo site and
  bounded by what's on disk. `sendfile()`/streaming is on the roadmap.

## Concurrency model

- **Event loop + worker pool, not "one connection per pooled task".** Giving
  each connection a worker for its whole life means N idle keep-alive clients
  pin N workers, so with `--threads 8` the 9th client waits up to the idle
  timeout. Instead one loop thread poll()s the listener and all *idle*
  connections. A readable connection is removed from the poll set and handed
  to the pool; the worker parses and answers what's buffered and gives it
  back through a mutex-protected queue plus a self-pipe wakeup. Each
  Connection is touched by one thread at a time, and the handoffs go through
  mutexes, so there's no data race on connection state.
- **Connections are `shared_ptr`** because `std::function` needs copyable
  callables. A unique_ptr capture would need a hand-rolled move-only function
  type.
- **Fairness**: a worker does at most 16 reads (16 KiB each) per wakeup and
  then yields the connection back. poll() is level-triggered, so leftover
  data brings it straight back.
- **Writes block the worker** (poll for POLLOUT with a 5 s write timeout). An
  output buffer in the event loop would avoid that. Responses here are small,
  so this is accepted and documented.
- **The idle timeout resets on every received byte.** A trickling (slowloris)
  client can stay connected; a per-request header deadline is on the roadmap.
- **Error responses use a lingering close**: send the error, `shutdown(WR)`,
  then discard input for up to 200 ms or 256 KiB. Closing with unread data
  makes the kernel send an RST that can destroy the 413/431 before the client
  reads it.
- **Default per-connection request cap is 1000** (same as nginx's
  `keepalive_requests`).
- **Graceful shutdown**: SIGINT/SIGTERM are blocked in every thread and
  received with `sigwait()` on a dedicated thread, which calls
  `Server::stop()`. That avoids async-signal-safety problems entirely. stop()
  closes the listener and idle connections; in-flight requests finish and
  are answered with `Connection: close`.
- **Sanitizer presets use Clang.** The dev machine had compiler-rt but not
  GCC's libasan/libtsan, and Clang's TSan is the reference implementation.
