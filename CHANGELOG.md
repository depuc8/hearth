# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.1.0] - 2026-11-11

### Added
- HTTP/1.1 server on raw POSIX sockets: a poll() event loop plus a fixed-size
  worker thread pool.
- Incremental request parser (byte-at-a-time safe) with header (8 KiB) and
  body limits, answering 400/413/414/431/501/505 as appropriate.
- Method + path router with exact and prefix routes, 404 vs 405 with `Allow`.
- Static file handler with MIME types and layered path-traversal protection.
- Keep-alive with `Connection: close`, HTTP/1.0 keep-alive, idle timeout
  (5 s default) and a per-connection request cap.
- `GET /stats`: atomic connection and request counters as JSON.
- Graceful shutdown on SIGINT/SIGTERM.
- CLI: `--port`, `--host`, `--threads`, `--root`, `--idle-timeout-ms`,
  `--max-requests`, `--max-body`.
- 151 GoogleTest tests (unit, integration over real sockets, concurrency).
- CMake presets for debug, release, asan, tsan and coverage; clang-format and
  clang-tidy configs; GitHub Actions CI and release workflows; Dockerfile.

[Unreleased]: https://github.com/depuc8/hearth/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/depuc8/hearth/releases/tag/v0.1.0
