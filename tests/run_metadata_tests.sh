#!/usr/bin/env bash
# File version: 1.1.0 | Updated: 2026-10-05 | Purpose: Run pixelprocess and MCP action tests.
# agent: gpt-6.1-sol | 2026-10-05 | Test integrated pixelprocess and MCP | v 1.1.0 | 796298
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RAYLIB_DIR="${RAYLIB_DIR:-$HOME/code/raylib}"
cmake -S "$ROOT/pixelprocess" -B "$ROOT/.build/pixelprocess" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$ROOT/.build/pixelprocess" -j"$(getconf _NPROCESSORS_ONLN)"
ctest --test-dir "$ROOT/.build/pixelprocess" --output-on-failure
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT
gcc -std=c11 -O1 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -DGRAPHICS_API_OPENGL_43 \
  -I"$RAYLIB_DIR/src" "$ROOT/tests/test_screenshot_mcp.c" "$ROOT/lib.c" \
  -L"$ROOT/.build/raylib/raylib" -Wl,-Bstatic -lraylib -Wl,-Bdynamic \
  -lGL -lm -lpthread -ldl -lrt -lX11 -o "$TMP_DIR/test_screenshot_mcp"
"$TMP_DIR/test_screenshot_mcp"
printf 'MCP screenshot checks passed\n'
# agent: gpt-6.1-sol | 2026-10-05 | Test integrated pixelprocess and MCP | v 1.1.0 | 796298
