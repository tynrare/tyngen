<!-- File version: 1.0.0 | Updated: 2026-10-05 | Purpose: Build and use the pixel conversion pipeline. -->
<!-- Flow PIXEL-01 canonical steps: common.hpp. Shell integration: build.sh and run.sh. -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document pixelprocess build and outputs | v 1.0.0 | 1e2ee1 -->

# Pixelprocess

Pixelprocess converts a generated or imported image into an isolated sprite sheet and individual sprite PNGs. It uses OpenCV 4 (`core`, `imgproc`, and `imgcodecs`) and builds with the project scripts:

```sh
./build.sh
```

The runner is available at `.build/pixelprocess/pixelprocess-run`. `run.sh` supplies that path to the app. Direct conversion uses this contract:

```sh
pixelprocess-run --input source.png --run-dir workspace/runs/run-id --settings settings.json
```

Each run retains `source.png`, stage images and debug overlays in `extra/`, a revisioned manifest under `extra/pixelprocess/`, event and sprite JSONL records, `final.pixel.png`, and `sprite-<sprite-id>.png` exports. Settings edits and ownership corrections create a new run revision after confirmation. The pipeline order and shared invariants are documented in `common.hpp` under PIXEL-01.

Run `tests/run_metadata_tests.sh` for the synthetic conversion/cache fixture and MCP screenshot checks.

<!-- agent: gpt-6.1-sol | 2026-10-05 | Document pixelprocess build and outputs | v 1.0.0 | 1e2ee1 -->
