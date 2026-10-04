#!/usr/bin/env bash
# Aigen Workbench v2.4.0 | Updated 2026-10-04 | Build when inputs change and launch PixelGenie.
# agent: gpt-6.1-sol | 2026-10-04 | Rebuild when project inputs change | v 2.4.0 | 55040d
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
BIN="$ROOT/pixelgenie"
RAYLIB_DIR="${RAYLIB_DIR:-$HOME/code/raylib}"
needs_build=0

if [[ ! -x "$BIN" ]]; then
  needs_build=1
else
  for input in main.c lib.c lib.h enet.h shaders.vs shaders.fs build.sh; do
    [[ "$ROOT/$input" -nt "$BIN" ]] && needs_build=1
  done
  if [[ -f "$RAYLIB_DIR/CMakeLists.txt" && "$RAYLIB_DIR/CMakeLists.txt" -nt "$BIN" ]]; then
    needs_build=1
  elif [[ -d "$RAYLIB_DIR/src" ]] &&
       [[ -n "$(find "$RAYLIB_DIR/src" -type f \( -name '*.c' -o -name '*.h' \) -newer "$BIN" -print -quit)" ]]; then
    needs_build=1
  fi
fi

if (( needs_build )); then
  "$ROOT/build.sh"
fi
exec "$ROOT/pixelgenie" "$@"
# agent: gpt-6.1-sol | 2026-10-04 | Rebuild when project inputs change | v 2.4.0 | 55040d
