#!/usr/bin/env bash
# File version: 1.1.0 | Updated: 2026-10-05 | Purpose: Build pixelprocess independently.
# agent: gpt-6.1-sol | 2026-10-05 | Build processor independently from shell | v 1.1.0 | 6d33f1
set -euo pipefail
PIXELPROCESS_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$PIXELPROCESS_DIR/.." && pwd)"
BUILD_DIR="${PIXELPROCESS_BUILD_DIR:-$ROOT/.build/pixelprocess}"
CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release)
if [[ -n "${OpenCV_DIR:-}" ]]; then CMAKE_ARGS+=("-DOpenCV_DIR=$OpenCV_DIR"); fi
if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"); fi
cmake -S "$PIXELPROCESS_DIR" -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" -j"$(getconf _NPROCESSORS_ONLN)"
printf 'Built pixelprocess in %s\n' "$BUILD_DIR"
# agent: gpt-6.1-sol | 2026-10-05 | Build processor independently from shell | v 1.1.0 | 6d33f1
