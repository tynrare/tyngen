#!/usr/bin/env bash

# agent: gpt-6 | 2026-10-09 | Run focused probe application checks | v 1.0.0 | a06e7e
# File version: 1.0.0 | Updated: 2026-10-09 | Purpose: Run probe UI persistence checks without a display.
set -euo pipefail
PROBE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROBE_RAYLIB="${RAYLIB_DIR:-$HOME/code/raylib}"
PROBE_TEMP="$(mktemp -d)"
trap 'rm -rf "$PROBE_TEMP"' EXIT
gcc -std=c11 -O1 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -DGRAPHICS_API_OPENGL_43 \
  -I"$PROBE_RAYLIB/src" "$PROBE_ROOT/tests/probe_test.c" \
  -L"$PROBE_ROOT/.cache/raylib/raylib" -Wl,-Bstatic -lraylib -Wl,-Bdynamic \
  -lGL -lm -lpthread -ldl -lrt -lX11 -o "$PROBE_TEMP/probe_test"
"$PROBE_TEMP/probe_test" "${1:-$PROBE_ROOT/pixelprocess/runner}"
# agent: gpt-6 | 2026-10-09 | Run focused probe application checks | v 1.0.0 | a06e7e
