#!/usr/bin/env bash
# File version: 1.3.0 | Updated: 2026-10-05 | Purpose: Run decoupled processor and MCP checks.
# agent: gpt-6.1-sol | 2026-10-05 | Use processor build from its directory | v 1.3.0 | e82be2
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RAYLIB_DIR="${RAYLIB_DIR:-$HOME/code/raylib}"
PIXELPROCESS_BUILD_DIR="${PIXELPROCESS_BUILD_DIR:-$ROOT/.build/pixelprocess}"
export PIXELPROCESS_BUILD_DIR
"$ROOT/pixelprocess/build.sh"
ctest --test-dir "$PIXELPROCESS_BUILD_DIR" --output-on-failure
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT
gcc -std=c11 -O1 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -DGRAPHICS_API_OPENGL_43 \
  -I"$RAYLIB_DIR/src" "$ROOT/tests/test_screenshot_mcp.c" "$ROOT/lib.c" \
  -L"$ROOT/.build/raylib/raylib" -Wl,-Bstatic -lraylib -Wl,-Bdynamic \
  -lGL -lm -lpthread -ldl -lrt -lX11 -o "$TMP_DIR/test_screenshot_mcp"
"$TMP_DIR/test_screenshot_mcp"
printf 'MCP screenshot checks passed\n'
# agent: gpt-6.1-sol | 2026-10-05 | Use processor build from its directory | v 1.3.0 | e82be2
