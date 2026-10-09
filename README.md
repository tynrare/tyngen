<!--
File version: 4.56.0
Last updated: 2026-10-09
Purpose: Build, run, configure, and use the Tyngen Workbench.
Flow index: APP-01 through APP-09 owner lib.c; lib.h indexes them; CLEANUP-01 and BPROBE-UI-01 owner lib.c; PIXEL-01 owner pixelprocess/common.hpp; PIXEL-02 shared-scale variants owner pixelprocess/runner.cpp; META-01 covers processor events and sprite evidence; APP-08 covers queued image decoding; MCP bridge run.sh -> app --mcp -> loopback ENet -> shared dispatcher; MCP-HIDDEN-01 owner main.c covers hidden app launch and screenshots.
Shared invariant: resource trash moves require explicit confirmation; all enabled text and image resources enter generation payloads; generation defaults belong to one enabled profile; processor overrides are run-local.
-->
<!-- agent: gpt-6.1-sol | 2026-10-09 | Require OpenCV five for processing | v 4.54.0 | 8a029f -->
<!-- agent: gpt-6 | 2026-10-09 | Document probe Settings workflow | v 4.55.0 | 4ab7db -->
<!-- agent: gpt-6 | 2026-10-09 | Document newest action right-click behavior | v 4.56.0 | 3559ab -->
<!-- agent: gpt-6.1-sol | 2026-10-06 | Index shared sheet variant flow | v 4.49.0 | f39dab -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document build jobs and processor artifact names | v 4.42.0 | f88977 -->
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document explicit profile settings save | v 4.32.0 | 244597 -->
<!-- agent: gpt-6.1-sol | 2026-10-09 | Rename project and launcher | v 4.53.0 | 57ff97 -->
# Tyngen Workbench

A small desktop raylib UI for managing resources, following generation and pixel-processing runs, and inspecting emitted outputs.
<!-- agent: gpt-6.1-sol | 2026-10-06 | Index shared sheet variant flow | v 4.49.0 | f39dab -->

## Build and run

```sh
./build.sh
./run.sh [workspace]
```

For agent control without showing a window, start `./run.sh --hidden [workspace]` and keep it running. The app uses its normal raylib renderer and binds the loopback ENet control server; the configured MCP bridge (`./run.sh --mcp [workspace]`) connects to it. Use MCP `ui_screenshot` with a workspace path such as `screenshots/agent.png`, then poll `ui_screenshot_status` until `pending` is false. Flow MCP-HIDDEN-01 in `main.c` defines the sequence. `./run.sh --screenshot /tmp/frame.png [workspace]` also captures one hidden initial frame without the MCP bridge.
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document hidden agent launch and screenshots | v 4.37.0 | 36a898 -->

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document queued previews and logs | v 4.27.0 | efaab8 -->
Build the shell with `./build.sh`; it does not build or require the pixel processor. `./run.sh` rebuilds the shell when needed and launches it with processing available when a runner is installed. Reference and run previews decode in the background; animated markers show loading, full-size textures load when an image is opened, and old run logs load when their details are opened.

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document profile resources and migration | v 4.27.0 | 10c40d -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document build jobs and processor artifact names | v 4.42.0 | f88977 -->
The default workspace is `assets`. Workspaces store image, text, and profile resources together in the flat `resources/` folder, plus immutable per-run output and logs in `runs/`. Existing `refs/` files and `profiles.ini` sections are migrated into resources at startup. The app does not create a replacement prompt when none remain. A first launch creates a Tyngen1-based **Default** profile resource. Generation needs the installed `replicate-gen` and a valid `REPLICATE_API_TOKEN`; it continues to work when no pixel processor is configured. Build the shell with `./build.sh`; its CMake artifacts live in `.cache/`. Build pixel processing separately with `./pixelprocess/build.sh`; its executables use their source names (`runner`, `stage_background`, and so on) in `pixelprocess/`, while CMake artifacts go in `.cache/pixelprocess/`. Both builds use four jobs by default; set `BUILD_JOBS` to a different count if needed. OpenCV 5 is required. CMake searches its standard locations by default; set `OpenCV_DIR` to an OpenCV package directory, or point it at an OpenCV source checkout to build and cache the required modules. For example, `OpenCV_DIR=/home/x/code/opencv ./build.sh`. Set `TYNGEN_PIXELPROCESS_DIR` to a directory containing `runner`, or set `TYNGEN_PIXELPROCESS_RUNNER` to the executable path. Set `TYNGEN_DISABLE_PIXELPROCESS=1` to leave an installed processor unused. Without a processor, image conversion and pixel-only reruns report that processing is unavailable.
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document build jobs and processor artifact names | v 4.42.0 | f88977 -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document build jobs and processor artifact names | v 4.42.0 | f88977 -->

Each profile is a readable `.profile` text resource. The UI can add, duplicate, select, and edit profiles as settings or text; settings changes stay unsaved until Save or Close, and changed fields are highlighted; enabling one disables the others. Selection persists per workspace. Model-specific options use repeatable `set=key=value` entries and extra prompt fragments use ordered `prompt=value` entries. Pixel defaults include background tolerance, grid hints, crop padding, pitch and phase, coverage, cleanup, and sheet placement. Obsolete pixel keys from existing profile text are discarded.
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document profile resources and migration | v 4.27.0 | 10c40d -->

## Controls

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document run controls and previews | v 4.26.0 | 036555 -->
<!-- agent: gpt-6 | 2026-10-09 | Document newest action right-click behavior | v 4.56.0 | 3559ab -->
The generation grid is the root screen. Each card represents one run and shows its current status, parsed stage, elapsed time, and available image preview. Every new run folder also gets an app-written `metadata.txt` with its ID, time, profile, model, prompt, resources, and image path. During pixel processing, the card switches from the original generated image to the main pixel output as soon as it is written. Runs with any readable image artifact count as complete even if a process reports an error. Use a card's trash icon to move that run and its files into `trash/` after confirmation. Click **New generation** to choose **Generate**, which opens Resources, or **Import generation**, which opens the desktop image picker, copies the selected image into a new run, starts pixel processing, and opens its details. Click the single view toggle beside the static TYNGEN label to switch between the Home grid and Library; from generation details it returns to the grid. When an image is open, the toggle closes its viewer and keeps the current view. Its icon reflects the current view (Library in the Library image viewer), stays highlighted except in Settings, and returns to the previously selected view when clicked from Settings. Generation artifact names appear in a popup only while hovering their dots. Generation details show a usable image as soon as it is available while the run continues; **Controls** switches to run details and **Show image** returns to the viewer. The shared image frame footer shows dimensions, zoom/pan tips, an **Open image** icon, and a square reset button that restores the image to its fitted size and clears panning. The generated image view shows source, stage, and final output thumbnails; right-click closes the newest open UI action, such as a menu, field editor, tool, confirmation, or viewer, and preserves its parent. In the Library image viewer, the filename field edits only the basename, with no extension or path. It supports caret placement, selection, clipboard, and horizontal scrolling; press Enter to rename while preserving the original extension, or Escape to cancel. The Resources view lists image, `.txt`, and profile resources as cards and has a **Generate** button. Use its **+** card to import an image/text file, create a blank text resource, or add a profile; clicking outside its menu closes it. Cards show filenames, and prompt cards include a text preview. Select a prompt to edit its full contents. Click in the text to place the caret and drag to select; use arrows, Home/End, Shift selection, Ctrl+A/C/X/V, Backspace, and Delete to edit. Text wraps and scrolls to keep the caret visible. **Save** or Ctrl+Enter saves changes. Closing an unsaved prompt opens a dialog with **Save and close**, **Discard**, and **Cancel** options. The frame shows saved, unsaved, or failed status. Every enabled text resource is sent as a prompt file; every enabled image is sent as an image file. The corner exclude control skips that item, and the trash control moves either kind to `trash/` after confirmation. The selected text file is only the one open for editing. Click a generation card for details; running generations show parsed progress, logs, and cancellation controls; when processor stages exist, their previews share the page with the log, otherwise the log uses the full width. Completed generations show their output images. Open application **Settings** with the square sliders button beside the view toggle to choose UI fonts. On a generation’s **Background mask** stage, use the **Settings** gear to edit probe dots with preview, Undo, Redo, Save, Save as copy, and Cancel. Open a profile card to edit its text or switch to its settings view, where model options and pixel processing changes remain unsaved until **Save** or **Close**; modified values are highlighted yellow. The raylib built-in font is the default. Import a TTF/OTF from any folder, or use the arrows to select a font stored in `assets/fonts`; the choice persists per workspace. Profile settings fields use the same framed single-line editor as file renaming, with caret placement, selection, clipboard, and horizontal scrolling. Enter or Escape ends editing without discarding its pending change. Application font choices apply immediately. Resource images can also be dropped into the window. Left-click a resource to open it; right-click closes the newest open action and preserves the parent view. Middle mouse pans the full view and wheel or `+`/`-` changes scale around the pointer when it is over the image.
<!-- agent: gpt-6 | 2026-10-09 | Document newest action right-click behavior | v 4.56.0 | 3559ab --> Excluded resources stay visible but are skipped during generation. Resources are copied into the workspace before use.
<!-- agent: gpt-6.1-sol | 2026-10-06 | Document generation imports | v 4.50.0 | 6aa6f9 -->
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document run controls and previews | v 4.26.0 | 036555 -->

<!-- agent: gpt-6.1-sol | 2026-10-05 | Restore top-left image metadata overlay | v 4.36.0 | b1e178 -->
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document run controls and previews | v 4.26.0 | 036555 -->
<!-- agent: gpt-6 | 2026-10-05 | Document sprite version inspection | v 4.45.0 | 87c00d -->
<!-- agent: gpt-6.1-sol | 2026-10-07 | Document cleanup behavior and verification | v 4.52.0 | fb362d -->
Generation image details show final pixel versions and processor stages in the artifact rail. Hover previews an artifact and clicking selects it. Controls provides **Process log**, **Gen record**, **Resources**, and **Metadata** views. With Inspect enabled, click a sprite to show 32×32 pipeline thumbnails beside its bounds; hover previews that sprite crop inside the same bounds without distortion, and click pins the version. Sprite inspection has no footer settings inputs. Every stage writes main/debug images and metadata. Runs retain a revisioned processor manifest, event journal, sprite evidence JSONL, the assembled `final.pixel.png`, and `sprite-<sprite-id>.png` exports. Use **Export** to copy a run artifact to PNG. **Cleanup** offers **Artifacts**, which keeps the source, settings, and pins, or **Full**, which keeps only the source. Removed files go to workspace backups when enabled; the backup manifest tracks retained files. Application Settings controls backup enablement and the workspace size cap. **Rerun pixels** opens a confirmation with **New run** and **Rerun here** options; **Rerun generation** remains available.
<!-- agent: gpt-6.1-sol | 2026-10-07 | Document cleanup behavior and verification | v 4.52.0 | fb362d -->
<!-- agent: gpt-6 | 2026-10-04 | Document processor metadata and screenshot actions | v 4.33.0 | ee415a -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Restore top-left image metadata overlay | v 4.36.0 | b1e178 -->
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document run controls and previews | v 4.26.0 | 036555 -->

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document unconditional processor artifacts | v 4.18.0 | ec5560 -->
Generation and pixel processing run as separate children. Pixelprocess executes the seven ordered stages in PIXEL-01 and writes its emitted artifact aliases into the run's `extra/` folder. The app labels each stdout/stderr line with time and process, follows output while at the log tail, and writes the underlying stream to `runs/<run>/process.log`. Clearing the visible log does not erase that file. Final output variants are listed separately from stages.
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document unconditional processor artifacts | v 4.18.0 | ec5560 -->

## Codex MCP and ENet

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document MCP run controls | v 4.26.0 | 9b221d -->
Build and launch the GUI with the workspace you intend to control. The project MCP config starts the app binary in stdio bridge mode and connects to the GUI over ENet on `127.0.0.1:27881` (override both processes with `TYNGEN_ENET_PORT`). MCP offers `pixel_convert`, `processing_setting_set`, `processing_scope`, `processing_apply`, `processing_owner_set`, `stage_select`, `stage_debug`, `run_controls`, `run_panel_select`, and `output_export`, along with run status, generation, resources, profiles, and logs. `processing_apply` prepares a pixel-only run and still requires `generation_confirm`. Legacy reference tool names remain aliases. Status includes the original generated image before pixel outputs. `output_select` accepts `original` or a pixel output index. Resource trash moves and run cancellation require a separate `app_confirm`; `app_cancel` dismisses a pending action.
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document MCP run controls | v 4.26.0 | 9b221d -->

The project MCP configuration fragment is in [`codex-mcp.toml`](codex-mcp.toml). Merge it into `.codex/config.toml` in a trusted checkout, then restart Codex. This workspace mounts `.codex/` read-only, so the fragment is provided without changing that protected configuration.

## Implementation flow

<!-- agent: gpt-6.1-sol | 2026-10-07 | Document cleanup behavior and verification | v 4.52.0 | fb362d -->
Application Settings contains workspace preferences such as the UI font and cleanup backup controls. Generation defaults are edited in a profile resource; run-specific pixel settings and ownership corrections are edited in the sprite inspector. Rename a profile by editing its filename in the text view. See APP-01 through APP-09 and CLEANUP-01 in `lib.c`, and PIXEL-01 in `pixelprocess/common.hpp`, for the canonical UI and processor flows.
<!-- agent: gpt-6.1-sol | 2026-10-07 | Document cleanup behavior and verification | v 4.52.0 | fb362d -->

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document per-run metadata file | v 4.2.0 | 875390 -->

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document adaptive run log layout | v 4.13.0 | 01bcc3 -->
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document queued previews and logs | v 4.27.0 | efaab8 -->
<!-- agent: gpt-6.1-sol | 2026-10-07 | Document cleanup behavior and verification | v 4.52.0 | fb362d -->
Run `tests/run_cleanup_tests.sh` for cleanup retention, backup, manifest, and pruning checks. Run `tests/run_metadata_tests.sh` for the C++ synthetic pipeline/cache test and MCP action checks. The processor contract and stage order are PIXEL-01 in `pixelprocess/common.hpp`.
<!-- agent: gpt-6.1-sol | 2026-10-07 | Document cleanup behavior and verification | v 4.52.0 | fb362d -->

<!-- agent: gpt-6.1-sol | 2026-10-05 | Document decoupled processor builds | v 4.39.0 | eb90d6 -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Document confirmed and in-place reruns | v 4.43.0 | 6bf16d -->

<!-- agent: gpt-6 | 2026-10-05 | Document sprite version inspection | v 4.45.0 | 87c00d -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Clarify in-place rerun label | v 4.46.0 | ff00bc -->
<!-- agent: gpt-6.1-sol | 2026-10-05 | Offer pixel rerun location choices | v 4.47.0 | 67cf7c -->

<!-- agent: gpt-6 | 2026-10-05 | Clarify stable sprite previews | v 4.48.0 | 9d3252 -->
<!-- agent: gpt-6.1-sol | 2026-10-06 | Index shared sheet variant flow | v 4.49.0 | f39dab -->
<!-- agent: gpt-6.1-sol | 2026-10-09 | Require OpenCV five for processing | v 4.54.0 | 8a029f -->
