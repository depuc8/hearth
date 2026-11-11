#!/usr/bin/env bash
# Formatting and static analysis, shared by CI and local development.
#
#   scripts/lint.sh format        rewrite files with clang-format
#   scripts/lint.sh format-check  fail if any file is not formatted
#   scripts/lint.sh tidy [dir]    run clang-tidy using dir/compile_commands.json
#                                 (default: build/debug)
set -euo pipefail
cd "$(dirname "$0")/.."

mapfile -t files < <(git ls-files '*.cpp' '*.hpp')

case "${1:-}" in
  format)
    clang-format -i "${files[@]}"
    ;;
  format-check)
    clang-format --dry-run --Werror "${files[@]}"
    echo "clang-format: ${#files[@]} files clean"
    ;;
  tidy)
    build_dir="${2:-build/debug}"
    if [[ ! -f "$build_dir/compile_commands.json" ]]; then
      echo "error: $build_dir/compile_commands.json missing; configure first" >&2
      exit 1
    fi
    runner="$(command -v run-clang-tidy || command -v run-clang-tidy.py || true)"
    if [[ -z "$runner" ]]; then
      echo "error: run-clang-tidy not found" >&2
      exit 1
    fi
    "$runner" -quiet -p "$build_dir" -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu)" \
      "$PWD/(src|tests)/.*\.cpp$"
    echo "clang-tidy: clean"
    ;;
  *)
    echo "usage: $0 {format|format-check|tidy [build-dir]}" >&2
    exit 2
    ;;
esac
