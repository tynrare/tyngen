#!/usr/bin/env bash
# Tyngen Workbench v2.7.0 | Updated 2026-10-09 | Build and launch the shell independently.
# agent: gpt-6.1-sol | 2026-10-09 | Launch Tyngen executable | v 2.7.0 | f88608
# agent: gpt-6.1-sol | 2026-10-05 | Launch without building processor | v 2.6.0 | baaac4
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
BIN="$ROOT/tyngen"
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
exec "$ROOT/tyngen" "$@"
# agent: gpt-6.1-sol | 2026-10-05 | Launch without building processor | v 2.6.0 | baaac4
