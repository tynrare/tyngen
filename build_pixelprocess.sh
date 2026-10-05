#!/usr/bin/env bash
# File version: 1.0.0 | Updated: 2026-10-05 | Purpose: Build pixelprocess independently.
# agent: gpt-6.1-sol | 2026-10-05 | Build processor independently from shell | v 1.0.0 | 48155d
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PIXELPROCESS_BUILD_DIR:-$ROOT/.build/pixelprocess}"
CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release)
if [[ -n "${OpenCV_DIR:-}" ]]; then CMAKE_ARGS+=("-DOpenCV_DIR=$OpenCV_DIR"); fi
cmake -S "$ROOT/pixelprocess" -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" -j"$(getconf _NPROCESSORS_ONLN)"
printf 'Built pixelprocess in %s\n' "$BUILD_DIR"
# agent: gpt-6.1-sol | 2026-10-05 | Build processor independently from shell | v 1.0.0 | 48155d
