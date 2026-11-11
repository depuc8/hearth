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
