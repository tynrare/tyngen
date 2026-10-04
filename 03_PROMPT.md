<!--
File version: 1.0.0
Last updated: 2026-10-03
Purpose: Specify active app usage, live progress, and pixel debug interactions.
Flow ID: APP-02
1. User → prepares prompt, references, and options → app previews the resolved generation request.
2. User → confirms request → app starts generation and shows live process status and output.
3. Generator → emits log lines, output files, and optional pixel stages → app updates progress and debug controls as evidence arrives.
4. User → inspects a stage and chooses an available adjustment → app previews or queues a rerun with the changed supported option.
5. User → confirms rerun or cancels it → app records the result and keeps the prior output available.
Invariant: show unknown progress as indeterminate; never imply a stage or metric exists until the process reports it.
-->
<!-- agent: gpt-6.1-sol | 2026-10-03 | Describe live generation and pixel debug interactions | v 1.0.0 | 47e8c3 -->

# 03 — Describe active application usage

Use [`01_PROMPT.md`](01_PROMPT.md) and [`02_prompt.md`](02_prompt.md) as requirements. This prompt specifies the active generation experience, progress display, and interactions for `process_ai_pixel_2.py`. Preserve the behavior and command options of `/home/x/Pictures/aigen/aigen0.sh`, `/home/x/Pictures/aigen/aigen1.sh`, and the installed `replicate-gen` and pixel processor.

## Generation prompt and request preview

- Keep the external prompt file as the editable source. Show its contents in the Prompt tab and make unsaved edits clear.
- Before starting, show a compact request preview with the prompt text and each component's source: prompt file, additional prompt text, selected references, model, generation options, and post-processing options. Preserve the scripts' order when combining prompt parts.
- Support these optional tokens in prompt text: `{workspace}`, `{reference_count}`, `{reference_files}`, `{model}`, `{active_process}`, and `{output_path}`. Resolve tokens only when explicitly present; leave ordinary braces unchanged. Show the resolved preview before generation.
- During a run, expose `{current_step}` and `{progress_percent}` as status fields in the UI, not as implicit edits to the saved prompt. Show a percent only when the process provides a measurable value; otherwise show an indeterminate activity indicator and the current named step.
- Right click or its keyboard equivalent confirms the request. Left click or its keyboard equivalent cancels the preview and returns to editing. A confirmation starts one run with an immutable snapshot of prompt, references, and options.

## Active process and progress

Add a **Progress** tab for the live output stream, or place an equivalent clearly separated panel beside the generation controls. Keep a compact running/failed/succeeded badge visible on other tabs and link it to the active run.

Show one row per active process with its name, state, start time, current step, elapsed time, and output paths as they become known. Include generation/upload/download and pixel post-processing as separate child processes when they are reported separately. Do not fabricate overall percentages from elapsed time or count unreported steps as complete.

Stream timestamped stdout and stderr lines into a scrollable log. Preserve warnings, errors, decisions, and `stage-*`, `final`, and `done` lines. Follow new lines automatically while the user is at the bottom; do not steal scroll position when the user scrolls back. Provide copy, clear-view (without deleting the underlying log), and open-output actions.

Show a confirmable cancel action while a process is active. Cancellation sends an interrupt to the child process, marks the run canceled, and retains partial logs and files. If the process cannot be interrupted cleanly, report that state and keep showing it until exit. On failure, show the failed process, exit status when available, last error lines, and any outputs already produced.

## Pixel processing debug controls

When debug output is enabled, run the pixel processor with `--emit all` and direct its artifacts and manifest to the active workspace's extra-output folder. Normal runs may retain `--emit final`; provide a visible option to rerun processing with debug stages enabled. The wrapper scripts currently pass `--emit final`, so make the debug choice explicit and forward it through the wrapper instead of assuming intermediate images exist.

Build a stage list from artifacts actually reported or listed in the manifest. Sort it in processor execution order, display the stage number and name, and omit conditional stages that were not produced. Each available stage gets a thumbnail, full-size zoom/pan view, file path, and actions to:

- open the image at full size;
- compare it with the source, previous available stage, or final output using a side-by-side or overlay view;
- inspect the stage log lines and corresponding decision/warning;
- add a note to the run without changing the image;
- rerun the processor with a supported option when applicable, keeping the current output intact.

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

The active workflow is complete when a user can preview the resolved generation request, start or cancel it, follow live logs and process state, inspect the actual pixel debug stages, make a supported adjustment, and rerun without losing the previous result. Verify with actual emitted files and logs from `process_ai_pixel_2.py`; conditional controls must appear only when their stage exists.

<!-- agent: gpt-6.1-sol | 2026-10-03 | Describe live generation and pixel debug interactions | v 1.0.0 | 47e8c3 -->
