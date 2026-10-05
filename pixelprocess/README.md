<!-- File version: 1.3.0 | Updated: 2026-10-05 | Purpose: Build and use the pixel conversion pipeline. -->
<!-- Flow PIXEL-01 canonical steps: common.hpp. Shell integration: pixelprocess/build.sh and run.sh. -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document direct processor executable output | v 1.3.0 | f527cf -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor build and discovery | v 1.1.0 | ce7b77 -->

# Pixelprocess

<!-- agent: gpt-6.1-sol | 2026-10-05 | Document direct processor executable output | v 1.3.0 | f527cf -->
Pixelprocess converts a generated or imported image into an isolated sprite sheet and individual sprite PNGs. It uses OpenCV 4 (`core`, `imgproc`, and `imgcodecs`) and builds independently:

```sh
./pixelprocess/build.sh
```

The script asks CMake to find OpenCV automatically. Install the OpenCV 4 development package first; on Linux this is commonly `libopencv-dev`. If CMake cannot find it, locate `OpenCVConfig.cmake` and set `OpenCV_DIR` to its containing directory. For example:

```sh
find /usr /opt -name OpenCVConfig.cmake 2>/dev/null
OpenCV_DIR=/opt/opencv/lib/cmake/opencv4 ./pixelprocess/build.sh
```

You can also add a nonstandard install prefix with `CMAKE_PREFIX_PATH=/opt/opencv`. The runner and stage executables are written directly into `pixelprocess/`; CMake's intermediate files go into the root `.cache/pixelprocess/` directory. The shell builds independently and can run without a processor. To use a processor in another directory, set `AIGEN_PIXELPROCESS_DIR` to its build or install directory; to select one executable directly, set `AIGEN_PIXELPROCESS_RUNNER`. Set `AIGEN_DISABLE_PIXELPROCESS=1` to leave an installed processor unused.
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document direct processor executable output | v 1.3.0 | f527cf -->

```sh
pixelprocess-run --input source.png --run-dir workspace/runs/run-id --settings settings.json
```

Each run retains `source.png`, stage images and debug overlays in `extra/`, a revisioned manifest under `extra/pixelprocess/`, event and sprite JSONL records, `final.pixel.png`, and `sprite-<sprite-id>.png` exports. Settings edits and ownership corrections create a new run revision after confirmation. The pipeline order and shared invariants are documented in `common.hpp` under PIXEL-01.

Run `tests/run_metadata_tests.sh` for the synthetic conversion/cache fixture and MCP screenshot checks.

<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor build and discovery | v 1.1.0 | ce7b77 -->
