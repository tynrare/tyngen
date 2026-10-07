#!/usr/bin/env bash
# Aigen Workbench v2.9.0 | Updated 2026-10-07 | Build PixelGenie and pixelprocess stages.
# agent: gpt-6 | 2026-10-07 | Build processor stages with app | v 2.9.0 | 5f1d63
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
RAYLIB_DIR="${RAYLIB_DIR:-$HOME/code/raylib}"
BUILD_DIR="$ROOT/.cache"
cmake -S "$ROOT/pixelprocess" -B "$BUILD_DIR/pixelprocess" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build "$BUILD_DIR/pixelprocess" -j"${BUILD_JOBS:-4}"
if [[ ! -f "$RAYLIB_DIR/CMakeLists.txt" ]]; then
  printf 'raylib source not found: %s (set RAYLIB_DIR)\n' "$RAYLIB_DIR" >&2
  exit 1
fi
cmake -S "$RAYLIB_DIR" -B "$BUILD_DIR/raylib" -DCMAKE_BUILD_TYPE=Release -DOPENGL_VERSION=4.3 -DBUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_C_FLAGS=-DSUPPORT_FILEFORMAT_JPG=1
# agent: gpt-6.1-sol | 2026-10-06 | Enable JPEG in raylib build | v 2.8.0 | bc4162
cmake --build "$BUILD_DIR/raylib" --target raylib -j"${BUILD_JOBS:-4}"
gcc -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -DGRAPHICS_API_OPENGL_43 -I"$RAYLIB_DIR/src" \
  "$ROOT/main.c" "$ROOT/lib.c" -L"$BUILD_DIR/raylib/raylib" -Wl,-Bstatic -lraylib -Wl,-Bdynamic \
  -lGL -lm -lpthread -ldl -lrt -lX11 -o "$ROOT/pixelgenie"
printf 'Built %s\n' "$ROOT/pixelgenie"
# agent: gpt-6.1-sol | 2026-10-05 | Limit parallel shell build jobs | v 2.7.0 | 51b93f
