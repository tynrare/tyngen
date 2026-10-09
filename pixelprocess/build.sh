#!/usr/bin/env bash
# File version: 1.7.0 | Updated: 2026-10-09 | Purpose: Build pixelprocess and cached OpenCV.
# agent: gpt-6.1-sol | 2026-10-09 | Explain OpenCV cache and build steps | v 1.7.0 | 3f31c8
# agent: gpt-6.1-sol | 2026-10-05 | Limit parallel processor build jobs | v 1.3.0 | 6069c1
set -euo pipefail
PIXELPROCESS_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$PIXELPROCESS_DIR/.." && pwd)"
BUILD_DIR="${PIXELPROCESS_BUILD_DIR:-$ROOT/.cache/pixelprocess}"
JOBS="${BUILD_JOBS:-4}"
CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release "-DBUILD_TESTING=${BUILD_TESTING:-OFF}" "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$PIXELPROCESS_DIR")

if [[ -n "${OpenCV_DIR:-}" ]]; then
  OPENCV_PATH="$(cd "$OpenCV_DIR" 2>/dev/null && pwd -P)" || {
    printf 'OpenCV path not found: %s\n' "$OpenCV_DIR" >&2
    exit 1
  }
  if [[ -f "$OPENCV_PATH/OpenCVConfig.cmake" ]]; then
    printf 'Using OpenCV package: %s\n' "$OPENCV_PATH/OpenCVConfig.cmake"
    CMAKE_ARGS+=("-DOpenCV_DIR=$OPENCV_PATH")
  elif [[ -f "$OPENCV_PATH/CMakeLists.txt" && -d "$OPENCV_PATH/modules/core" ]]; then
    OPENCV_SOURCE_DIR="$OPENCV_PATH"
    OPENCV_KEY="$(printf '%s' "$OPENCV_SOURCE_DIR" | cksum | awk '{print $1}')"
    OPENCV_BUILD_DIR="$ROOT/.cache/opencv/$OPENCV_KEY"
    OPENCV_INSTALL_DIR="$ROOT/.cache/opencv-install/$OPENCV_KEY"
    OPENCV_CONFIG="$(find "$OPENCV_INSTALL_DIR" -name OpenCVConfig.cmake -print -quit 2>/dev/null || true)"
    OPENCV_CACHE_COMPLETE=1
    for module in core imgproc imgcodecs; do
      if ! find "$OPENCV_INSTALL_DIR" -type f \
        \( -name "libopencv_${module}.so*" -o -name "libopencv_${module}.a" \) \
        -print -quit 2>/dev/null | grep -q .; then
        OPENCV_CACHE_COMPLETE=0
        break
      fi
    done
    OPENCV_CACHE_MISS=0
    if [[ -z "$OPENCV_CONFIG" || "$OPENCV_CACHE_COMPLETE" -eq 0 ]]; then
      OPENCV_CACHE_MISS=1
      printf 'OpenCV 5 cache miss for source: %s\n' "$OPENCV_SOURCE_DIR"
      printf 'Building core, imgproc, and imgcodecs. The first build can take several minutes.\n'
      printf 'Build cache: %s\nInstall cache: %s\n' "$OPENCV_BUILD_DIR" "$OPENCV_INSTALL_DIR"
      OPENCV_ARGS=(
        -DCMAKE_BUILD_TYPE=Release
        "-DCMAKE_INSTALL_PREFIX=$OPENCV_INSTALL_DIR"
        -DBUILD_LIST=core,imgproc,imgcodecs
        -DBUILD_TESTS=OFF
        -DBUILD_PERF_TESTS=OFF
        -DBUILD_EXAMPLES=OFF
        -DBUILD_opencv_apps=OFF
        -DBUILD_JAVA=OFF
        -DBUILD_opencv_python3=OFF
      )
      if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then OPENCV_ARGS+=("-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"); fi
      cmake -S "$OPENCV_SOURCE_DIR" -B "$OPENCV_BUILD_DIR" "${OPENCV_ARGS[@]}"
      cmake --build "$OPENCV_BUILD_DIR" -j"$JOBS"
      cmake --install "$OPENCV_BUILD_DIR"
      OPENCV_CONFIG="$(find "$OPENCV_INSTALL_DIR" -name OpenCVConfig.cmake -print -quit)"
      OPENCV_CACHE_COMPLETE=1
      for module in core imgproc imgcodecs; do
        if ! find "$OPENCV_INSTALL_DIR" -type f \
          \( -name "libopencv_${module}.so*" -o -name "libopencv_${module}.a" \) \
          -print -quit 2>/dev/null | grep -q .; then
          OPENCV_CACHE_COMPLETE=0
          break
        fi
      done
    fi
    if [[ -z "$OPENCV_CONFIG" || "$OPENCV_CACHE_COMPLETE" -eq 0 ]]; then
      printf 'OpenCV cache build did not produce the package config and required libraries. Check the OpenCV build output above.\n' >&2
      exit 1
    fi
    if [[ "$OPENCV_CACHE_MISS" -eq 0 ]]; then
      printf 'Using cached OpenCV 5 package: %s\n' "$OPENCV_CONFIG"
    fi
    CMAKE_ARGS+=("-DOpenCV_DIR=$(dirname "$OPENCV_CONFIG")")
  else
    printf 'OpenCV_DIR must point to an OpenCV source tree or a directory containing OpenCVConfig.cmake: %s\n' "$OpenCV_DIR" >&2
    exit 1
  fi
fi
if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"); fi
if ! cmake -S "$PIXELPROCESS_DIR" -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"; then
  printf 'Pixelprocess requires OpenCV 5 (core, imgproc, imgcodecs). Set OpenCV_DIR to a package directory or source checkout.\n' >&2
  printf 'Example: OpenCV_DIR=/path/to/opencv ./pixelprocess/build.sh\n' >&2
  exit 1
fi
cmake --build "$BUILD_DIR" -j"$JOBS"
printf 'Built pixelprocess executables in %s (CMake files in %s)\n' "$PIXELPROCESS_DIR" "$BUILD_DIR"
# agent: gpt-6.1-sol | 2026-10-09 | Explain OpenCV cache and build steps | v 1.7.0 | 3f31c8
# agent: gpt-6.1-sol | 2026-10-05 | Limit parallel processor build jobs | v 1.3.0 | 6069c1
