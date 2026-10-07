#!/usr/bin/env bash
# File version: 1.0.0 | Updated: 2026-10-07 | Purpose: Build and run cleanup behavior checks.
# agent: gpt-6.1-sol | 2026-10-07 | Run workspace cleanup behavior checks | v 1.0.0 | b23bb3
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RAYLIB_DIR="${RAYLIB_DIR:-$HOME/code/raylib}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT
gcc -std=c11 -O1 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -DGRAPHICS_API_OPENGL_43 \
  -I"$RAYLIB_DIR/src" "$ROOT/tests/cleanup_test.c" \
  -L"$ROOT/.cache/raylib/raylib" -Wl,-Bstatic -lraylib -Wl,-Bdynamic \
  -lGL -lm -lpthread -ldl -lrt -lX11 -o "$TMP_DIR/cleanup_test"
"$TMP_DIR/cleanup_test"
