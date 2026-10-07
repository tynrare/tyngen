<!--
File version: 1.6.0
Last updated: 2026-10-04
Purpose: Specify active app usage, live progress, and emitted pixel artifacts.
Flow ID: APP-02
1. User → prepares prompt, references, and options → app previews the resolved generation request.
2. User → confirms request → app starts generation and shows live process status and output.
3. Generator and processor → emit log lines, final outputs, and all applicable pixel stages → app updates progress and artifact controls as evidence arrives.
4. User → inspects a stage and chooses an available adjustment → app previews or queues a rerun with the changed supported option.
5. User → confirms rerun or cancels it → app records the result and keeps the prior output available.
Invariant: show unknown progress as indeterminate; never imply a stage or metric exists until the process reports it.
-->
<!-- agent: gpt-6.1-sol | 2026-10-04 | Specify numeric stage row direction | v 1.6.0 | 6c1871 -->

# 03 — Describe active application usage

Use [`01_PROMPT.md`](01_PROMPT.md) and [`02_prompt.md`](02_prompt.md) as requirements. This prompt specifies the active generation experience, progress display, and interactions for `process_ai_pixel_2.py`. Preserve the behavior and command options of `/home/x/Pictures/aigen/aigen0.sh`, `/home/x/Pictures/aigen/aigen1.sh`, and the installed `replicate-gen` and pixel processor.

## Generation prompt and request preview

- Keep text files in References as editable prompt sources and make unsaved edits clear. Include every enabled text reference as a repeated prompt input; do not select one active prompt.
- Before starting, show a compact request preview with all enabled prompt inputs, image references, model, generation options, and post-processing options. Preserve the references order.
- Support these optional tokens in prompt text: `{workspace}`, `{reference_count}`, `{reference_files}`, `{model}`, `{active_process}`, and `{output_path}`. Resolve tokens only when explicitly present; leave ordinary braces unchanged. Show the resolved preview before generation.
- During a run, expose `{current_step}` and `{progress_percent}` as status fields in the UI, not as implicit edits to the saved prompt. Show a percent only when the process provides a measurable value; otherwise show an indeterminate activity indicator and the current named step.
- Right click or its keyboard equivalent confirms the request. Left click or its keyboard equivalent cancels the preview and returns to editing. A confirmation starts one run with an immutable snapshot of prompt files, image references, and options.

## Active process and progress

Add a **Progress** tab for the live output stream, or place an equivalent clearly separated panel beside the generation controls. Keep a compact running/failed/succeeded badge visible on other tabs and link it to the active run.

Show one row per active process with its name, state, start time, current step, elapsed time, and output paths as they become known. Include generation/upload/download and pixel post-processing as separate child processes when they are reported separately. Do not fabricate overall percentages from elapsed time or count unreported steps as complete.

Stream timestamped stdout and stderr lines into a scrollable log. Preserve warnings, errors, decisions, and `stage-*`, `final`, and `done` lines. Follow new lines automatically while the user is at the bottom; do not steal scroll position when the user scrolls back. Provide copy, clear-view (without deleting the underlying log), and open-output actions.

Show a confirmable cancel action while a process is active. Cancellation sends an interrupt to the child process, marks the run canceled, and retains partial logs and files. If the process cannot be interrupted cleanly, report that state and keep showing it until exit. On failure, show the failed process, exit status when available, last error lines, and any outputs already produced.

## Pixel processing artifacts

<!-- agent: gpt-6.1-sol | 2026-10-04 | Require all emitted processor stages | v 1.4.0 | 6d4970 -->
`replicate-gen` does not accept `--emit`. After it saves the generated source, invoke `process_ai_pixel_2.py` directly with `--emit all` on every run. Save every emitted stage and the manifest in the active run's `extra/` folder. There is no stage-output toggle.

<!-- agent: gpt-6.1-sol | 2026-10-04 | Specify numeric stage row direction | v 1.6.0 | 6c1871 -->
Build a stage list from artifacts actually reported or listed in the manifest. Sort and display stages by descending numeric stage number, grouped into logical processor phases; omit conditional stages that were not produced. In the generated image viewer, place final outputs in a compact horizontal row at the top, group middle stages into compact horizontal phase rows, and keep the generated source at the bottom. Within each phase row, place higher-numbered stages farther right. The viewer places stage 38 in the early phase row with stages 40, 36, 35, and 34. Align every row to a shared fine grid, center the rows vertically, and shrink dots as needed without scrolling. Hover previews and labels a dot; place its label beside the grid so it does not cover thumbnails. Clicking selects the artifact. Each available stage gets a full-size zoom/pan view, file path, and actions to:

- open the image at full size;
- compare it with the source, previous available stage, or final output using a side-by-side or overlay view;
- inspect the stage log lines and corresponding decision/warning;
- add a note to the run without changing the image;
- rerun the processor with a supported option when applicable, keeping the current output intact.

The installed processor was inspected with `--emit all` using an existing source image. It wrote 14 stages to the manifest for that run; other stage entries below are conditional and should appear only when emitted. Keep this inventory as the reference for later stage-specific inspection work:

| Phase row | Stage | Image meaning and emission condition |
| --- | --- | --- |
| Layout | 40 `source` | Baseline input image. Always emitted with `--emit all`. |
| Layout | 36 `layout` | Detected cell boundaries, method, confidence, and grid. Always emitted. |
| Layout | 35 `blob-grid-fit` | Phase diagnostic placeholder; blob-grid-fit is disabled. Always emitted. |
| Layout | 34 `align-blobs` | Placement after accepted blob alignment; emitted only when alignment is accepted. |
| Lattice and reconstruction | 33 `pixel-lattice` | Detected or forced source-pixel pitch and lattice boundaries. Always emitted. |
| Lattice and reconstruction | 32 `sprite-grid-fit` | Fitted sprite grid after phase decisions. Always emitted. |
| Lattice and reconstruction | 37 `lattice-phase-fit` | Phase-fit diagnostic overlay. Always emitted. |
| Lattice and reconstruction | 39 `lattice-phase-shift` | Phase-shift diagnostic overlay. Always emitted. |
| Lattice and reconstruction | 31 `sprite-reconstruct` | Reconstructed sprites before final expansion. Always emitted. |
| Lattice and reconstruction | 30 `pixel-pitch` | Shared-pitch reconstruction result. Always emitted. |
| Alpha and grid | 28 `alpha-edge-candidates` | Candidate edge-alpha pixels; weighted edge-alpha mode only. |
| Alpha and grid | 27 `alpha-edge-support` | Support values for candidate edge-alpha pixels; weighted mode only. |
| Alpha and grid | 26 `alpha-edge-edits` | Proposed edge-alpha edits; weighted mode only and requires `--magenta skip`. |
| Alpha and grid | 29 `uniform-grid` | Regularized logical-pixel grid. Always emitted. |
| Magenta cleanup | 25 `magenta-outside-mask` | Outside-magenta classification mask; emitted when an outside mask exists. |
| Magenta cleanup | 24 `magenta-source-support` | Source support used for bounded cleanup; emitted when outside support exists. |
| Magenta cleanup | 22 `magenta-support-clean` | Support-scoped cleanup preview; outside mode with outside support only. |
| Magenta cleanup | 20 `magenta-clean` | Cleanup result; emitted unless magenta mode is `skip`. |
| Rearrangement | 38 `blob-ownership` | Blob ownership during rearrangement; blob-owned layout with rearrangement only. |
| Rearrangement | 09 `boundary-recovery` | Recovered/unresolved boundary pixels; non-blob rearrangement in outside mode only. |
| Rearrangement | 10 `rearrange-bounds` | Trim and repack bounds; non-blob layout with rearrangement only. |

Keep stages sorted by descending numeric stage number within their logical phase rows, regardless of conditional processor emission order. The inspected run emitted 40, 36, 35, 33, 32, 37, 39, 31, 30, 29, 25, 24, 22, and 20. It did not emit alignment, weighted-alpha, or rearrangement stages.
<!-- agent: gpt-6.1-sol | 2026-10-04 | Specify numeric stage row direction | v 1.6.0 | 6c1871 -->

Draft these stage-specific controls and explanations, showing only controls whose prerequisites and relevant artifacts exist:

| Stage | Display and interaction |
| --- | --- |
| 40 `source` | Baseline image; compare against every later stage. |
| 36 `layout` | Show detected cell boundaries, layout method, confidence, and grid. Offer a rerun with `--layout COLSxROWS` or automatic detection. |
| 35 `blob-grid-fit` | Label as phase diagnostics only; show the stage and log decision. Do not offer it as a selectable processing mode. |
| 34 `align-blobs` | When emitted, show moved/skipped counts and confidence. Offer `--align-blobs auto`, `on`, or `off` on rerun. |
| 33 `pixel-lattice` | Show detected/forced pitch and confidence. Offer automatic detection or a rerun with a chosen `--pixel-pitch`. |
| 32 `sprite-grid-fit` | Compare fitted grid to source; link to pitch and lattice decisions. |
| 37 `lattice-phase-fit` | Show phase-fit diagnostics and offer side-by-side comparison with phase-shift diagnostics. |
| 39 `lattice-phase-shift` | Show phase-shift diagnostics and compare with phase-fit and reconstructed sprites. |
| 31 `sprite-reconstruct` | Preview reconstructed sprites before expansion; offer comparison with source and final sheet. |
| 30 `pixel-pitch` | Show shared pitch result and measured pitch. Offer rerun with an explicit pitch value. |
| 28 `alpha-edge-candidates` | Only for weighted edge-alpha runs; explain candidate pixels and link to the two related alpha views. |
| 27 `alpha-edge-support` | Only for weighted edge-alpha runs; inspect support values and compare with candidate/edit images. |
| 26 `alpha-edge-edits` | Only for weighted edge-alpha runs; inspect proposed alpha edits and offer rerun with weighted or legacy mode. Respect the processor requirement that weighted mode uses `--magenta skip`. |
| 29 `uniform-grid` | Preview the regularized logical grid; compare with reconstructed sprites and final output. |
| 25 `magenta-outside-mask` | Only when outside-magenta classification ran; show removed/kept mask and offer reruns with `--magenta outside`, `all`, or `skip`. |
| 24 `magenta-source-support` | When emitted, show source support used by bounded cleanup; compare with stage 25 and 22. |
| 22 `magenta-support-clean` | When emitted, compare retained and removed pixels; offer a rerun with an alternate magenta mode. |
| 20 `magenta-clean` | When emitted, compare cleanup result with stage 29 and source; offer `outside`, `all`, or `skip` on rerun. |
| 38 `blob-ownership` | When emitted during rearrangement, show blob ownership and compare with stage 34/layout; retain the current output while trying another alignment mode. |
| 09 `boundary-recovery` | When emitted during non-blob rearrangement, show recovered and unresolved boundary pixels and the recovery counts from the log. |
| 10 `rearrange-bounds` | When emitted, show trim/repack bounds and compare with the pre-rearranged sheet. Offer a rerun with rearrangement enabled or disabled. |

For final outputs, show the primary pixel sheet, forced `--tile-size` outputs, and pixel-pitch alternatives as distinct selectable results, with dimensions and manifest details when available. Never treat diagnostic stage images as final outputs. A rerun creates a new run entry and output set; it must not overwrite the selected run's files.

## Agent access and acceptance

Expose the active run's status, current step, progress (or indeterminate state), log tail, stage list, output list, and the same confirm/cancel/rerun actions to MCP and ENet agent access. Route these through the same app actions used by the UI.

The active workflow is complete when a user can preview the resolved generation request, start or cancel it, follow live logs and process state, inspect the actual emitted pixel stages, make a supported adjustment, and rerun without losing the previous result. Conditional controls must appear only when their stage exists.

<!-- agent: gpt-6.1-sol | 2026-10-04 | Require all emitted processor stages | v 1.4.0 | 6d4970 -->
