<!--
File version: 4.32.0
Last updated: 2026-10-04
Purpose: Build, run, configure, and use the Aigen Workbench.
Flow index: APP-01 through APP-08 owner lib.c; lib.h indexes them; APP-08 covers queued image decoding, main-thread texture uploads, and deferred desktop log reads; MCP bridge run.sh -> app --mcp -> loopback ENet -> shared dispatcher.
Shared invariant: resource trash moves require explicit confirmation; all enabled text and image resources enter generation payloads; application Settings contains app preferences, profile Settings contains generation configuration, and one profile resource is enabled per workspace.
-->
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document explicit profile settings save | v 4.32.0 | 244597 -->
# Aigen Workbench

A small desktop raylib UI for managing resources, following generation and pixel-processing runs, and inspecting emitted outputs.

## Build and run

```sh
./build.sh
./run.sh [workspace]
```

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document queued previews and logs | v 4.27.0 | efaab8 -->
Build with `./build.sh` after changing source; `./run.sh` launches the existing binary without rebuilding. Reference and run previews decode in the background; animated markers show loading, full-size textures load when an image is opened, and old run logs load when their details are opened.

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document profile resources and migration | v 4.27.0 | 10c40d -->
The default workspace is `assets`. Workspaces store image, text, and profile resources together in the flat `resources/` folder, plus immutable per-run output and logs in `runs/`. Existing `refs/` files and `profiles.ini` sections are migrated into resources at startup. The app does not create a replacement prompt when none remain. A first launch creates an aigen1-based **Default** profile resource. Generation needs the installed `replicate-gen`, a valid `REPLICATE_API_TOKEN`, Python 3 with the processor dependencies, and the installed `process_ai_pixel_2.py` beside `replicate-gen`.

Each profile is a readable `.profile` text resource. The UI can add, duplicate, select, and edit profiles as settings or text; settings changes stay unsaved until Save or Close, and changed fields are highlighted; enabling one disables the others. Selection persists per workspace. Model-specific options use repeatable `set=key=value` entries and extra prompt fragments use ordered `prompt=value` entries. Processor fields include layout, pitch, alignment, magenta, alpha, rearrangement, padding, and tile sizes.
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document profile resources and migration | v 4.27.0 | 10c40d -->

## Controls

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document run controls and previews | v 4.26.0 | 036555 -->
The generation grid is the root screen. Each card represents one run and shows its current status, parsed stage, elapsed time, and available image preview. Every new run folder also gets an app-written `metadata.txt` with its ID, time, profile, model, prompt, resources, and image path. During pixel processing, the card switches from the original generated image to the main pixel output as soon as it is written. Runs with any readable image artifact count as complete even if a process reports an error. Use a card's trash icon to move that run and its files into `trash/` after confirmation. Click **+** to open Library. Use the single view toggle beside the static AIGEN label to switch between the Home grid and Library; from generation details it returns to the grid. When an image is open, the toggle closes its viewer and keeps the current view. Its icon reflects the current view (Library in the Library image viewer), stays highlighted except in Settings, and returns to the previously selected view when clicked from Settings. Generation artifact names appear in a popup only while hovering their dots. Generation details show a usable image as soon as it is available while the run continues; **Controls** switches to run details and **Show image** returns to the viewer. The shared image frame footer shows dimensions, zoom/pan tips, an **Open image** icon, and a square reset button that restores the image to its fitted size and clears panning. The generated image view shows source, stage, and final output thumbnails; right-click closes the current image. In the Library image viewer, the filename field edits only the basename, with no extension or path. It supports caret placement, selection, clipboard, and horizontal scrolling; press Enter to rename while preserving the original extension, or Escape to cancel. The Resources view lists image, `.txt`, and profile resources as cards and has a **Generate** button. Use its **+** card to import an image/text file, create a blank text resource, or add a profile; clicking outside its menu or right-clicking dismisses it. Cards show filenames, and prompt cards include a text preview. Select a prompt to edit its full contents. Click in the text to place the caret and drag to select; use arrows, Home/End, Shift selection, Ctrl+A/C/X/V, Backspace, and Delete to edit. Text wraps and scrolls to keep the caret visible. **Save** or Ctrl+Enter saves changes. Closing an unsaved prompt opens a dialog with **Save and close**, **Discard**, and **Cancel** options. The frame shows saved, unsaved, or failed status. Every enabled text resource is sent as a prompt file; every enabled image is sent as an image file. The corner exclude control skips that item, and the trash control moves either kind to `trash/` after confirmation. The selected text file is only the one open for editing. Click a generation card for details; running generations show parsed progress, logs, and cancellation controls; when processor stages exist, their previews share the page with the log, otherwise the log uses the full width. Completed generations show their output images. Open application **Settings** with the square sliders button beside the view toggle to choose UI fonts. Open a profile card to edit its text or switch to its settings view, where model options and pixel processing changes remain unsaved until **Save** or **Close**; modified values are highlighted yellow. The raylib built-in font is the default. Import a TTF/OTF from any folder, or use the arrows to select a font stored in `assets/fonts`; the choice persists per workspace. Profile settings fields use the same framed single-line editor as file renaming, with caret placement, selection, clipboard, and horizontal scrolling. Enter or Escape ends editing without discarding its pending change. Application font choices apply immediately. Resource images can also be dropped into the window. Left-click a resource to open it and right-click to close; middle mouse pans the full view and wheel or `+`/`-` changes scale around the pointer when it is over the image. Excluded resources stay visible but are skipped during generation. Resources are copied into the workspace before use.
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document run controls and previews | v 4.26.0 | 036555 -->

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document run controls and previews | v 4.26.0 | 036555 -->
Generation image details show final pixel versions in a compact horizontal row at the top, processor stages in compact horizontal logical phase groups through the middle, and the generated source at the bottom. Within each phase row, stages are sorted by descending numeric stage number from right to left, with earlier stages farther right; stage 40 is at the right of the early phase row, and stage 35 is at its left. Stage 38 shares that 40–35 phase row rather than forming its own rearrangement row. Stage thumbnails use twice the previous size. The dot grid sits at the viewer's right edge. Hover previews the image and opens its stage label or filename beside the grid without covering thumbnails; leaving restores the selection, and clicking selects the dot. Controls provides in-app **Process log**, **Gen record**, and **Resources** views; Resources previews the saved inputs used for that run. It also offers **Clean stages**, **Rerun pixels**, and **Rerun generation**. Clean stages removes emitted debug stage images from that run. Reruns create a new run and require confirmation. **Change profile setting** cycles and immediately saves the active profile option used by the selected stage.
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document run controls and previews | v 4.26.0 | 036555 -->

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document unconditional processor artifacts | v 4.18.0 | ec5560 -->
Generation and pixel processing run as separate children. Every pixel-processing run uses `process_ai_pixel_2.py --emit all`, placing all emitted stages and the manifest in that run's `extra/` folder; conditional stages appear when their processor conditions are met. The app labels each stdout/stderr line with time and process, follows output while at the log tail, and writes the underlying stream to `runs/<run>/process.log`. Clearing the visible log does not erase that file. Final output variants are listed separately from stages.
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document unconditional processor artifacts | v 4.18.0 | ec5560 -->

## Codex MCP and ENet

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document MCP run controls | v 4.26.0 | 9b221d -->
Build and launch the GUI with the workspace you intend to control. The project MCP config starts the app binary in stdio bridge mode and connects to the GUI over ENet on `127.0.0.1:27881` (override both processes with `AIGEN_ENET_PORT`). MCP offers `ui_view` for home, resources, or generation details (optionally selecting a run), plus status, prompt read/save, profile list/select/edit, resource list/import/manage, generation preview/confirm/cancel, run inspection, log clearing, debug stage cleanup, pixel/full reruns, and run notes. Legacy reference tool names remain aliases. Status includes the original generated image before pixel outputs. `output_select` accepts `original` or a pixel output index. `generation_preview` only prepares a request; call `generation_confirm` separately to start it. Resource trash moves and run cancellation require a separate `app_confirm`; `app_cancel` dismisses a pending action.
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document MCP run controls | v 4.26.0 | 9b221d -->

The project MCP configuration fragment is in [`codex-mcp.toml`](codex-mcp.toml). Merge it into `.codex/config.toml` in a trusted checkout, then restart Codex. This workspace mounts `.codex/` read-only, so the fragment is provided without changing that protected configuration.

## Implementation flow

Application Settings contains workspace preferences such as the UI font. Generation and processor settings are edited in a profile resource through its Settings view; switch between profile text and settings from the same right aligned toolbar. Rename a profile by editing its filename in the text view. See APP-01 through APP-08 in the `lib.c` header for the canonical UI, run, image, and MCP flows. APP-08 documents the image loading queue and texture ownership.

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document per-run metadata file | v 4.2.0 | 875390 -->

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document adaptive run log layout | v 4.13.0 | 01bcc3 -->
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document queued previews and logs | v 4.27.0 | efaab8 -->
