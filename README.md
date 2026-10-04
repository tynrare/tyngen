<!--
File version: 4.5.0
Last updated: 2026-10-04
Purpose: Build, run, configure, and use the Aigen Workbench.
Flow index: APP-01, APP-02, APP-03, APP-04, and APP-05 owner lib.c; lib.h indexes them; MCP bridge run.sh -> app --mcp -> loopback ENet -> shared dispatcher.
Shared invariant: reference trash moves require explicit confirmation; all enabled text references and images enter generation payloads.
-->
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document settings toggle behavior | v 4.5.0 | cdd66d -->
# Aigen Workbench

A small desktop raylib UI for managing references, following generation and pixel-processing runs, and inspecting emitted outputs.

## Build and run

```sh
./build.sh
./run.sh [workspace]
```

The default workspace is `assets`. Workspaces store image references and optional prompt files directly in the flat `refs/` folder, profiles in `profiles.ini`, and immutable per-run output and logs in `runs/`. The app does not create a replacement prompt when none remain. A first launch creates the directories and an aigen1-based **Default** profile. Generation needs the installed `replicate-gen`, a valid `REPLICATE_API_TOKEN`, Python 3 with the processor dependencies, and the installed `process_ai_pixel_2.py` beside `replicate-gen`.

Profiles use INI sections named `[profile:Name]`. The UI can add, duplicate, select, and edit profiles; model-specific options use repeatable `set=key=value` entries and extra prompt fragments use ordered `prompt=value` entries. Processor fields include layout, pitch, alignment, magenta, alpha, rearrangement, padding, and tile sizes.

## Controls

The generation grid is the root screen. Each card represents one run and shows its current status, parsed stage, elapsed time, and available image preview. Every new run folder also gets an app-written `metadata.txt` with its ID, time, profile, model, prompt, references, and image path. During pixel processing, the card switches from the original generated image to the main pixel output as soon as it is written. Runs with any readable image artifact count as complete even if a process reports an error. Use a card's trash icon to move that run and its files into `trash/` after confirmation. Click **+** to open Library. Use the single view toggle beside the static AIGEN label to switch between the Home grid and Library; from generation details it returns to the grid. The toggle is highlighted in normal views and returns to the previously selected view when clicked from Settings. Library holds image references and `.txt` prompts as asset cards and has a **Generate** button. Use its **+** card to import an image/text file or create a blank prompt; clicking outside its menu or right-clicking dismisses it. Cards show filenames, and prompt cards include a text preview. Select a prompt to edit its full contents. Click in the text to place the caret and drag to select; use arrows, Home/End, Shift selection, Ctrl+A/C/X/V, Backspace, and Delete to edit. Text wraps and scrolls to keep the caret visible. **Save** or Ctrl+Enter saves changes. Closing an unsaved prompt opens a dialog with **Save and close**, **Discard**, and **Cancel** options. The frame shows saved, unsaved, or failed status. The Library and generation image views share a viewer below the app header. The filename sits above the image frame; its footer holds dimensions, zoom/pan tips, **Open image**, and, for generations, output version controls. Output versions include the original generated image and pixel outputs; right-click closes the current image. In the Library image viewer, the filename field edits only the basename, with no extension or path. It supports caret placement, selection, clipboard, and horizontal scrolling; press Enter to rename while preserving the original extension, or Escape to cancel. Every enabled text reference is sent as a prompt file; every enabled image is sent as an image file. The corner exclude control skips that item, and the trash control moves either kind to `trash/` after confirmation. The selected text file is only the one open for editing. Click a generation card for details; running generations show parsed progress, logs, stages, and cancellation controls, and completed generations show their output images. Open **Settings** with the square sliders button beside the view toggle to edit profiles, prompt additions, model options, pixel processing, and UI fonts. The raylib built-in font is the default. Import a TTF/OTF from any folder, or use the arrows to select a font stored in `assets/fonts`; the choice persists per workspace. Settings fields stay visible; click a field, edit it, and press Enter to save. Reference images can also be dropped into the window. Left-click a reference to open it and right-click to close; middle mouse pans the full view and wheel or `+`/`-` changes scale. Excluded references stay visible but are skipped during generation. References are copied into the workspace before use.

Generation and pixel processing run as separate children. The app labels each stdout/stderr line with time and process, follows output while at the log tail, and writes the underlying stream to `runs/<run>/process.log`. Clearing the visible log does not erase that file. `process_ai_pixel_2.py --emit all` runs place diagnostic stages and manifests in that run's `extra/` folder; final output variants are listed separately from stages.

## Codex MCP and ENet

Build and launch the GUI with the workspace you intend to control. The project MCP config starts the app binary in stdio bridge mode and connects to the GUI over ENet on `127.0.0.1:27881` (override both processes with `AIGEN_ENET_PORT`). MCP offers `ui_view` for home, library, or generation details (optionally selecting a run), plus status, prompt read/save, profile list/select/edit, reference list/import/manage, generation preview/confirm/cancel, run inspection, log clearing, pixel rerun, and run notes. Status includes the original generated image before pixel outputs. `output_select` accepts `original` or a pixel output index. `generation_preview` only prepares a request; call `generation_confirm` separately to start it. Reference trash moves and run cancellation require a separate `app_confirm`; `app_cancel` dismisses a pending action.

The project MCP configuration fragment is in [`codex-mcp.toml`](codex-mcp.toml). Merge it into `.codex/config.toml` in a trusted checkout, then restart Codex. This workspace mounts `.codex/` read-only, so the fragment is provided without changing that protected configuration.

## Implementation flow

See APP-01 steps 1-7, APP-02, APP-03, APP-04, and APP-05 in the `lib.c` header for the canonical UI, run, image, and MCP flow.

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document per-run metadata file | v 4.2.0 | 875390 -->
