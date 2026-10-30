# Progress

- [x] 1. Scaffold: CMake, presets, folder layout, hello-world test
- [x] 2. Socket layer + single-threaded fixed-response server (verified with curl)
- [x] 3. Incremental parser + tests
- [x] 4. Router + static files + tests
- [x] 5. Thread pool + keep-alive + timeouts + limits + integration tests
- [x] 6. Atomic metrics + /stats + concurrency tests (TSan clean)
- [x] 7. clang-format, clang-tidy clean, all sanitizer presets green
- [x] 8. CI workflows, Docker, release workflow
- [x] 9. Logo, banner, README, docs, benchmark script + real run
- [x] 10. Fresh clone → build → test from README only

## Verification log

- Fresh `git clone` from GitHub, following only the README:
  `cmake --preset release && cmake --build --preset release && ctest --preset release`
  → 0 warnings, 151/151 tests passed. The run command serves `/` and `/stats`,
  and SIGINT shuts down cleanly. The README's `tsan` one-liner gives 151/151
  with no reports. `docker build` + `docker run` (via podman) serves `/`.
  Nothing needed fixing.
- CI on GitHub: all 11 jobs green (Linux gcc/clang × Debug/Release, macOS,
  clang-format, clang-tidy, ASan+UBSan, TSan, coverage).
- Not done on the build machine: a wrk benchmark (wrk not installed; see
  `bench/results.md`), and setting repo topics/homepage (no `gh` CLI or API
  token).
