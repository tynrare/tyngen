#!/usr/bin/env bash
# File version: 1.0.0 | Updated: 2026-10-04 | Purpose: Run processor and MCP metadata tests.
# agent: gpt-6 | 2026-10-04 | Run processor and MCP metadata tests | v 1.0.0 | 7770b9
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROCESSOR_ROOT="${PIXEL_PROCESSOR_ROOT:-/home/x/bin/imagegen}"
RAYLIB_DIR="${RAYLIB_DIR:-$HOME/code/raylib}"
"$PROCESSOR_ROOT/tests/run_metadata_tests.sh"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT
gcc -std=c11 -O1 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -DGRAPHICS_API_OPENGL_43 \
  -I"$RAYLIB_DIR/src" "$ROOT/tests/test_screenshot_mcp.c" "$ROOT/lib.c" \
  -L"$ROOT/.build/raylib/raylib" -Wl,-Bstatic -lraylib -Wl,-Bdynamic \
  -lGL -lm -lpthread -ldl -lrt -lX11 -o "$TMP_DIR/test_screenshot_mcp"
"$TMP_DIR/test_screenshot_mcp"
printf 'MCP screenshot checks passed\n'
# agent: gpt-6 | 2026-10-04 | Run processor and MCP metadata tests | v 1.0.0 | 7770b9
