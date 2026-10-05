<!-- File version: 1.1.0 | Updated: 2026-10-05 | Purpose: Build and use the pixel conversion pipeline. -->
<!-- Flow PIXEL-01 canonical steps: common.hpp. Shell integration: build_pixelprocess.sh and run.sh. -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor build and discovery | v 1.1.0 | ce7b77 -->

# Pixelprocess

Pixelprocess converts a generated or imported image into an isolated sprite sheet and individual sprite PNGs. It uses OpenCV 4 (`core`, `imgproc`, and `imgcodecs`) and builds with the project scripts:

```sh
./build_pixelprocess.sh
```

The runner is available at `.build/pixelprocess/pixelprocess-run`. The shell builds independently and can run without a processor. To use a processor in another directory, set `AIGEN_PIXELPROCESS_DIR` to its build or install directory; to select one executable directly, set `AIGEN_PIXELPROCESS_RUNNER`. Set `AIGEN_DISABLE_PIXELPROCESS=1` to leave an installed processor unused.

```sh
pixelprocess-run --input source.png --run-dir workspace/runs/run-id --settings settings.json
```

Each run retains `source.png`, stage images and debug overlays in `extra/`, a revisioned manifest under `extra/pixelprocess/`, event and sprite JSONL records, `final.pixel.png`, and `sprite-<sprite-id>.png` exports. Settings edits and ownership corrections create a new run revision after confirmation. The pipeline order and shared invariants are documented in `common.hpp` under PIXEL-01.

Run `tests/run_metadata_tests.sh` for the synthetic conversion/cache fixture and MCP screenshot checks.

<!-- agent: gpt-6.1-sol | 2026-10-05 | Document processor build and discovery | v 1.1.0 | ce7b77 -->
