<!-- File version: 1.8.0 | Updated: 2026-10-07 | Purpose: Build and use the graph-based pixel processor. -->
<!-- Flow PIXEL-02: execution runner.cpp; graph contract common.hpp; UI integration lib.c; setup pins SETUP-SAVE-01 in lib.c. -->
<!-- agent: gpt-6.1-sol | 2026-10-07 | Document persistent setup branch pins | v 1.8.0 | 48b23b -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor test executable location | v 1.5.0 | 313fd0 -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor build and discovery | v 1.1.0 | ce7b77 -->

# Pixelprocess

<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor test executable location | v 1.5.0 | 313fd0 -->
Pixelprocess converts a generated or imported image into an isolated sprite sheet and individual sprite PNGs. It uses OpenCV 4 (`core`, `imgproc`, and `imgcodecs`) and builds independently:

```sh
./pixelprocess/build.sh
```

The script asks CMake to find OpenCV automatically. Install the OpenCV 4 development package first; on Linux this is commonly `libopencv-dev`. If CMake cannot find it, locate `OpenCVConfig.cmake` and set `OpenCV_DIR` to its containing directory. For example:

```sh
find /usr /opt -name OpenCVConfig.cmake 2>/dev/null
OpenCV_DIR=/opt/opencv/lib/cmake/opencv4 ./pixelprocess/build.sh
```

<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor test executable location | v 1.5.0 | 313fd0 -->
You can also add a nonstandard install prefix with `CMAKE_PREFIX_PATH=/opt/opencv`. The executable files are named after their `.cpp` sources and are written directly into `pixelprocess/`; `tests/pipeline_test.cpp` builds as `tests/pipeline_test`. CMake intermediates go in `.cache/pixelprocess/`. The app invokes `runner --input source.png --run-dir workspace/runs/run-id --settings settings.json`. To use a processor in another directory, set `AIGEN_PIXELPROCESS_DIR` or `AIGEN_PIXELPROCESS_RUNNER`; set `AIGEN_DISABLE_PIXELPROCESS=1` to disable it.
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor test executable location | v 1.5.0 | 313fd0 -->

```sh
./pixelprocess/runner --input source.png --run-dir workspace/runs/run-id --settings settings.json
```

<!-- agent: gpt-6.1-sol | 2026-10-06 | Document current settings sibling | v 1.7.0 | 8eb3f0 -->
<!-- agent: gpt-6.1-sol | 2026-10-07 | Document persistent setup branch pins | v 1.8.0 | 48b23b -->
<!-- agent: gpt-6.1-sol | 2026-10-07 | Document persistent setup branch pins | v 1.8.0 | 48b23b -->
Each run retains an immutable source, node images and metadata, `extra/pixelprocess.manifest.json`, an append-only event journal, artifact-specific sprite evidence, final alternatives, and sprite crops. Schema 2 records processing progress as ordered rows and alternatives as branch lanes. Each node names its main/debug artifacts and sprite crop directory; sprite evidence is keyed by artifact path and sprite ID. Branch settings pins live in `extra/pixelprocess/pins/<branch-id>.json`. A full rerun regenerates each pinned branch with its saved settings and retires extra branches that were already unpinned when the rerun began. New branches stay visible until a later full rerun. Events use schema 2 and carry step, node, branch, and progress IDs. Older manifests remain viewable and their branches start unpinned.

Run `tests/run_metadata_tests.sh` for graph, conversion/cache, sprite-evidence, cleanup, and MCP screenshot checks.

<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor build and discovery | v 1.1.0 | ce7b77 -->
<!-- agent: gpt-6.1-sol | 2026-10-06 | Document current settings sibling | v 1.7.0 | 8eb3f0 -->
<!-- agent: gpt-6.1-sol | 2026-10-07 | Document persistent setup branch pins | v 1.8.0 | 48b23b -->
