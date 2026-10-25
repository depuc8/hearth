# Contributing to hearth

Thanks for your interest! hearth is a small, focused project. Bug reports,
tests and well-scoped improvements are very welcome.

## Getting set up

You need CMake ≥ 3.21, Ninja, and GCC ≥ 13 or Clang ≥ 18 (older versions
may work but aren't tested). The sanitizer presets use Clang.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## Before opening a pull request

```sh
ctest --preset debug                 # all tests
ctest --preset asan                  # after: cmake --preset asan && cmake --build --preset asan
ctest --preset tsan                  # after: cmake --preset tsan && cmake --build --preset tsan
scripts/lint.sh format               # clang-format (CI pins clang-format 23.1.5)
scripts/lint.sh tidy build/debug     # clang-tidy, must be clean
```

CI runs all of the above on Linux (GCC and Clang) and macOS. Formatting and
clang-tidy findings fail the build.

## Guidelines

- **C++17 and POSIX only.** No third-party runtime dependencies. GoogleTest
  is the only dependency, and only for tests.
- **Every behaviour change comes with a test.** Parser and router changes
  need unit tests; anything touching connections or threads needs an
  integration test over a real socket.
- **Keep it TSan-clean.** Shared state is either confined to one thread at a
  time (as connections are) or protected by a mutex or atomic. Explain new
  synchronization in a comment.
- **Record judgment calls** in `DECISIONS.md`.
- **Commits** follow [Conventional Commits](https://www.conventionalcommits.org/)
  (`feat:`, `fix:`, `test:`, `docs:`, `ci:` ...).
- Update `CHANGELOG.md` under *Unreleased* for user-visible changes.

## Reporting security issues

Please don't open a public issue for vulnerabilities (for example a way
around the path-traversal checks). Use GitHub's private
[security advisory](https://github.com/depuc8/hearth/security/advisories/new)
form instead.
