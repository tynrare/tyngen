<!-- File version: 2.5.0 | Updated: 2026-10-09 | Purpose: Build and use the graph-based pixel processor. -->
<!-- Flow PIXEL-02: execution runner.cpp; graph contract common.hpp; UI integration lib.c; setup contract SETUP-SAVE-01 in docs/lattice-setup-branch-settings-plan.md. -->
<!-- agent: gpt-6 | 2026-10-07 | Document branch replacement and pin semantics | v 1.9.0 | 045a0e -->
<!-- agent: gpt-6 | 2026-10-07 | Document schema four transactions | v 2.1.0 | fab233 -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor test executable location | v 1.5.0 | 313fd0 -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor build and discovery | v 1.1.0 | ce7b77 -->

<!-- agent: gpt-6.1-sol | 2026-10-09 | Document Tyngen processor settings | v 2.2.0 | ecca26 -->
<!-- agent: gpt-6.1-sol | 2026-10-09 | Document OpenCV five requirement | v 2.4.0 | cf1bd8 -->
<!-- agent: gpt-6.1-sol | 2026-10-09 | Document cached OpenCV source build | v 2.3.0 | 28b661 -->
<!-- agent: gpt-6.1-sol | 2026-10-09 | Document final quantization stage | v 2.5.0 | 1bb7b4 -->
# Pixelprocess

<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor test executable location | v 1.5.0 | 313fd0 -->
Pixelprocess converts a generated or imported image into an isolated sprite sheet and individual sprite PNGs, then quantizes the final sheet and exports matching sprite crops. It uses OpenCV 5 (`core`, `imgproc`, and `imgcodecs`) and builds independently:

```sh
./pixelprocess/build.sh
```

The script asks CMake to find OpenCV automatically. Install the OpenCV 5 development package first. If CMake cannot find it, locate `OpenCVConfig.cmake` and set `OpenCV_DIR` to its containing directory. You can also point `OpenCV_DIR` at an OpenCV source checkout; the script builds the required modules into `.cache/` and reuses that build. For example:

```sh
find /usr /opt -name OpenCVConfig.cmake 2>/dev/null
OpenCV_DIR=/opt/opencv/lib/cmake/opencv5 ./pixelprocess/build.sh
OpenCV_DIR=/home/x/code/opencv ./pixelprocess/build.sh
```

<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor test executable location | v 1.5.0 | 313fd0 -->
You can also add a nonstandard install prefix with `CMAKE_PREFIX_PATH=/opt/opencv`. The executable files are named after their `.cpp` sources and are written directly into `pixelprocess/`; `tests/pipeline_test.cpp` builds as `tests/pipeline_test`. CMake intermediates go in `.cache/pixelprocess/`. The app invokes `runner --input source.png --run-dir workspace/runs/run-id --settings settings.json`. To use a processor in another directory, set `TYNGEN_PIXELPROCESS_DIR` or `TYNGEN_PIXELPROCESS_RUNNER`; set `TYNGEN_DISABLE_PIXELPROCESS=1` to disable it.
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor test executable location | v 1.5.0 | 313fd0 -->

```sh
./pixelprocess/runner --input source.png --run-dir workspace/runs/run-id --settings settings.json
```

<!-- agent: gpt-6.1-sol | 2026-10-06 | Document current settings sibling | v 1.7.0 | 8eb3f0 -->
<!-- agent: gpt-6 | 2026-10-07 | Document branch replacement and pin semantics | v 1.9.0 | 045a0e -->
Each run retains an immutable source, node images and metadata, `extra/pixelprocess.manifest.json`, an append-only event journal, artifact-specific sprite evidence, final alternatives, and sprite crops. Graph manifest schema 4 records ordered processing nodes, alternatives as branch lanes, and immutable prior graph revisions under `extra/pixelprocess/history/`. History is read-only and is not branch ancestry. Each node names its main/debug artifacts and sprite crop directory; sprite evidence is keyed by artifact path and sprite ID. Branch settings are recorded by branch ID; copies inherit current parent settings while retaining their owned overrides. The pin file at `extra/pixelprocess/pins/<branch-id>.json` marks a branch for preservation. Rerun advances the selected branch revision from the chosen stage, retaining its identity and prior results as history. The runner stages each operation in a private run snapshot and atomically switches `extra/pixelprocess/current` after all required branches complete. Concurrent writers are serialized, stale saves are rejected, and failed operations leave the prior graph selected. Events use schema 2 and carry operation, step, node, branch, and progress IDs.

Run `tests/run_metadata_tests.sh` for graph, conversion/cache, sprite-evidence, cleanup, and MCP screenshot checks.

<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor build and discovery | v 1.1.0 | ce7b77 -->
<!-- agent: gpt-6.1-sol | 2026-10-06 | Document current settings sibling | v 1.7.0 | 8eb3f0 -->
<!-- agent: gpt-6 | 2026-10-07 | Document branch replacement and pin semantics | v 1.9.0 | 045a0e -->
<!-- agent: gpt-6 | 2026-10-07 | Document schema four transactions | v 2.1.0 | fab233 -->

<!-- agent: gpt-6.1-sol | 2026-10-09 | Document cached OpenCV source build | v 2.3.0 | 28b661 -->
<!-- agent: gpt-6.1-sol | 2026-10-09 | Document OpenCV five requirement | v 2.4.0 | cf1bd8 -->
<!-- agent: gpt-6.1-sol | 2026-10-09 | Document final quantization stage | v 2.5.0 | 1bb7b4 -->
