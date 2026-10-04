<!--
File version: 1.0.0
Last updated: 2026-10-03
Purpose: Follow-up implementation prompt for the desktop image-generation app.
Flow ID: APP-01
1. User → launches app with optional workspace → app opens that folder, or `assets` by default.
2. User → edits prompt and manages references → app keeps external prompt files and reference files in sync safely.
3. User → confirms generation or an agent sends a command → app runs the same action path and exposes progress and debug steps.
4. Generator → produces output and diagnostics → app shows them in separate controls and updates the output view.
5. User → confirms or cancels with mouse or keyboard → app performs the matching action consistently.
Invariant: ENet and MCP agent actions must use the same command handlers and control state as the UI.
-->
<!-- agent: gpt-6.1-sol | 2026-10-03 | Define desktop app implementation requirements | v 1.0.0 | 6fb378 -->

# 02 — Implement the desktop image-generation app

Use [`01_PROMPT.md`](01_PROMPT.md) as the source requirements. Inspect the existing project and the referenced files under `/home/x/Pictures/aigen`, especially `aigen0.sh` and `aigen1.sh`, before choosing behavior. Implement the app in this repository; preserve existing user files and keep the interface minimal.

## Application

- Build a desktop-only raylib application. Use the raylib checkout at `~/code/raylib` and ENet already present in the project where practical.
- Keep application source in `main.c`, `lib.c`, and `lib.h`; put shaders in `.vs` and `.fs` files. Maintain `build.sh` and `run.sh`. A clean build must work in this folder; build cache may live in a subfolder.
- Accept an optional active workspace folder as the launch argument. Use `assets` when no folder is supplied. Resolve prompt, reference, output, and generation-related files against that workspace unless the existing scripts establish a different location.
- Read the existing scripts to preserve their generation behavior and command-line options. Do not invent or silently drop script options; report any dependency or behavior that cannot be supported.

## Interface and input

Provide three compact tabs:

1. **Prompt:** read and edit an external prompt file, and save edits back to it.
2. **References:** open or drag and drop images, show thumbnails with adjustable size, and support hiding, excluding, deleting, and full-size viewing. Full-size view must support scaling and panning.
3. **Output:** show generated images and expose each generation/debug/extra step as its own subcontrol.

Use the simplest controls that make those actions usable. A right click confirms and a left click cancels; provide matching keyboard controls. Apply the same mappings to generation, reference actions, and other confirmation/cancellation interactions, and make the active mapping visible in the interface.

## Agent commands

- Add a small ENet bind/fetch loop for receiving commands periodically. Accept commands through the same handlers used by raylib keyboard input and UI actions.
- Add MCP configuration so a Codex agent can access the app. Mirror every user-visible control/action into agent access, including prompt edits, reference operations, generation, output/debug inspection, and confirm/cancel.
- Inspect the project's available MCP/agent conventions and the existing scripts before selecting command names, transport details, or configuration format. Document the command interface and any operational assumptions in the project.
- Keep network and agent commands bounded and validate paths and arguments against the active workspace. A remote command must not bypass the same confirmation/cancellation behavior as its UI equivalent.

## Completion criteria

- `build.sh` produces a clean build using the local raylib checkout; `run.sh` launches it and forwards the optional workspace argument.
- The app opens the default or supplied workspace, and the three tabs perform the listed prompt, reference, and output tasks.
- Mouse and keyboard actions behave consistently, including right-click confirm and left-click cancel.
- ENet and MCP commands invoke the same actions as the UI and expose the same available controls and results.
- Include concise build/run and agent-command instructions. Update headers and change stamps as required by `AGENTS.md`.

<!-- agent: gpt-6.1-sol | 2026-10-03 | Define desktop app implementation requirements | v 1.0.0 | 6fb378 -->
