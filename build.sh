#!/usr/bin/env bash
# Aigen Workbench v2.2.0 | Updated 2026-10-04 | Build the raylib workbench as pixelgenie.
# agent: gpt-6.1-sol | 2026-10-04 | Name image workbench PixelGenie binary | v 2.2.0 | af3484
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
RAYLIB_DIR="${RAYLIB_DIR:-$HOME/code/raylib}"
BUILD_DIR="$ROOT/.build"
if [[ ! -f "$RAYLIB_DIR/CMakeLists.txt" ]]; then
  printf 'raylib source not found: %s (set RAYLIB_DIR)\n' "$RAYLIB_DIR" >&2
  exit 1
fi
cmake -S "$RAYLIB_DIR" -B "$BUILD_DIR/raylib" -DOPENGL_VERSION=4.3 -DBUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build "$BUILD_DIR/raylib" --target raylib -j"$(getconf _NPROCESSORS_ONLN)"
gcc -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -DGRAPHICS_API_OPENGL_43 -I"$RAYLIB_DIR/src" \
  "$ROOT/main.c" "$ROOT/lib.c" -L"$BUILD_DIR/raylib/raylib" -Wl,-Bstatic -lraylib -Wl,-Bdynamic \
  -lGL -lm -lpthread -ldl -lrt -lX11 -o "$ROOT/pixelgenie"
printf 'Built %s\n' "$ROOT/pixelgenie"
# agent: gpt-6.1-sol | 2026-10-04 | Name image workbench PixelGenie binary | v 2.2.0 | af3484
