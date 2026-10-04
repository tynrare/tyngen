<!--
File version: 2.9.0
Last updated: 2026-10-04
Purpose: Build, run, configure, and use the Aigen Workbench.
Flow index: APP-01 and APP-02 owner lib.c; lib.h indexes both; MCP bridge run.sh -> app --mcp -> loopback ENet -> shared dispatcher.
Shared invariant: reference trash moves require explicit confirmation; all enabled text references and images enter generation payloads.
-->
<!-- agent: gpt-6.1-sol | 2026-10-04 | Document shared reference renaming | v 2.9.0 | 9c55b0 -->
# Aigen Workbench

A small desktop raylib UI for preparing image-generation requests, managing references, following generation and pixel-processing runs, and inspecting emitted outputs.

## Build and run

```sh
./build.sh
./run.sh [workspace]
```

The default workspace is `assets`. Workspaces store image references and optional prompt files in `refs/`, profiles in `profiles.ini`, and immutable per-run output and logs in `runs/`. The app does not create a replacement prompt when none remain. A first launch creates the directories and an aigen1-based **Default** profile. Generation needs the installed `replicate-gen`, a valid `REPLICATE_API_TOKEN`, Python 3 with the processor dependencies, and the installed `process_ai_pixel_2.py` beside `replicate-gen`.

Profiles use INI sections named `[profile:Name]`. The UI can add, duplicate, select, and edit profiles; model-specific options use repeatable `set=key=value` entries and extra prompt fragments use ordered `prompt=value` entries. Processor fields include layout, pitch, alignment, magenta, alpha, rearrangement, padding, and tile sizes.

## Controls

References, Progress, and Output are the three primary pages. References includes image references and `.txt` prompts. Use the **+** thumbnail to import an image/text file or create a blank prompt; clicking outside its menu or right-clicking dismisses it. Cards show filenames, and prompt cards include a text preview. Select a prompt to edit its full contents; Ctrl+Enter or closing saves edits. In either image or text viewer, click the filename, edit it, and press Enter to rename while keeping its file extension; Escape cancels the rename. Every enabled text reference is sent as a prompt file; every enabled image is sent as an image file. The corner exclude control skips that item, and the trash control moves either kind to `trash/` after confirmation. The selected text file is only the one open for editing. Open **Details** for reference actions, generation, run logs and stages, and output file access. **Settings** appears inside Details for profiles, prompt additions, model options, pixel processing, and UI fonts. The raylib built-in font is the default. Import a TTF/OTF from any folder, or use the arrows to select a font stored in `assets/fonts`; the choice persists per workspace. Settings fields stay visible; click a field, edit it, and press Enter to save. Reference images can also be dropped into the window. Left-click a reference to open it and right-click to close; middle mouse pans the full view and wheel or `+`/`-` changes scale. Excluded references stay visible but are skipped during generation. References are copied into the workspace before use.

Generation and pixel processing run as separate children. The app labels each stdout/stderr line with time and process, follows output while at the log tail, and writes the underlying stream to `runs/<run>/process.log`. Clearing the visible log does not erase that file. `process_ai_pixel_2.py --emit all` runs place diagnostic stages and manifests in that run's `extra/` folder; final output variants are listed separately from stages.

## Codex MCP and ENet

Build and launch the GUI with the workspace you intend to control. The project MCP config starts the app binary in stdio bridge mode and connects to the GUI over ENet on `127.0.0.1:27881` (override both processes with `AIGEN_ENET_PORT`). MCP offers status, prompt read/save, profile list/select/edit, reference list/import/manage, generation preview/confirm/cancel, run inspection, log clearing, pixel rerun, and run notes. `generation_preview` only prepares a request; call `generation_confirm` separately to start it. Reference trash moves and run cancellation require a separate `app_confirm`; `app_cancel` dismisses a pending action.

The project MCP configuration fragment is in [`codex-mcp.toml`](codex-mcp.toml). Merge it into `.codex/config.toml` in a trusted checkout, then restart Codex. This workspace mounts `.codex/` read-only, so the fragment is provided without changing that protected configuration.

## Implementation flow

See APP-01 steps 1-7 and APP-02 in the `lib.c` header for the canonical UI, run, and MCP flow.

<!-- agent: gpt-6.1-sol | 2026-10-04 | Document shared reference renaming | v 2.9.0 | 9c55b0 -->
