#!/usr/bin/env bash
# Aigen Workbench v2.5.0 | Updated 2026-10-05 | Build when inputs change and launch PixelGenie.
# agent: gpt-6.1-sol | 2026-10-05 | Rebuild pixelprocess source changes | v 2.5.0 | 23c487
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
BIN="$ROOT/pixelgenie"
RAYLIB_DIR="${RAYLIB_DIR:-$HOME/code/raylib}"
needs_build=0

if [[ ! -x "$BIN" ]]; then
  needs_build=1
else
  for input in main.c lib.c lib.h enet.h shaders.vs shaders.fs build.sh pixelprocess/CMakeLists.txt pixelprocess/common.cpp pixelprocess/common.hpp pixelprocess/runner.cpp pixelprocess/stage_background.cpp pixelprocess/stage_ownership.cpp pixelprocess/stage_extract.cpp pixelprocess/stage_lattice.cpp pixelprocess/stage_reconstruct.cpp pixelprocess/stage_cleanup.cpp pixelprocess/stage_assemble.cpp; do
    [[ "$ROOT/$input" -nt "$BIN" ]] && needs_build=1
  done
  if [[ -f "$RAYLIB_DIR/CMakeLists.txt" && "$RAYLIB_DIR/CMakeLists.txt" -nt "$BIN" ]]; then
    needs_build=1
  elif [[ -d "$RAYLIB_DIR/src" ]] &&
       [[ -n "$(find "$RAYLIB_DIR/src" -type f \( -name '*.c' -o -name '*.h' \) -newer "$BIN" -print -quit)" ]]; then
    needs_build=1
  fi
fi
if [[ ! -x "$ROOT/.build/pixelprocess/pixelprocess-run" ]]; then needs_build=1; fi

if (( needs_build )); then
  "$ROOT/build.sh"
fi
export AIGEN_PIXELPROCESS_RUNNER="$ROOT/.build/pixelprocess/pixelprocess-run"
exec "$ROOT/pixelgenie" "$@"
# agent: gpt-6.1-sol | 2026-10-05 | Rebuild pixelprocess source changes | v 2.5.0 | 23c487
