<!--
File version: 1.0.0
Last updated: 2026-10-06
Purpose: Plan manual spritesheet grid diagnosis and branch editing.
Flow MANUAL-GRID-01: canonical proposed flow; diagnosis and ownership are owned by pixelprocess; editing controls and history are owned by lib.c; branch persistence is owned by pixelprocess/runner.cpp.
1. User -> open diagnosis -> original sheet displays live grid boundaries and ownership labels.
2. User -> enter rows and columns -> lightweight ownership preview recalculates split positions and cell labels without extraction.
3. User -> edit, undo, or redo -> draft settings and preview stay in sync; cancel discards the draft.
4. User -> Save -> rerun ownership and downstream stages on the selected branch; successful completion replaces that branch result.
5. User -> Save as copy -> fork the selected branch and rerun its pipeline; original branch remains available.
6. Processor -> publish failure -> preserve prior branch artifacts and keep the edit session open.
Invariant: users choose grid counts only; split positions are detected from the source and never dragged manually. Save operations affect only the selected branch; copy creates a distinct branch.
Scope index: diagnosis is after PIXEL-01 step 2 and before step 3; execution and publication extend PIXEL-02 in runner.cpp; UI integration extends APP-04 and APP-06 in lib.c. Automatic grid inference remains owned by GRID-DETECT-01 in docs/spritesheet-detection-plan.md.
-->
<!-- agent: gpt-6 | 2026-10-06 | Plan manual spritesheet grid editing | v 1.0.0 | 591694 -->

# Manual spritesheet pipeline tweaking plan

## Goal

Let users correct inferred row and column counts before extraction, inspect the resulting split positions and blob ownership, and save the pipeline change on the selected branch or as a new branch. Automatic grid inference improvements remain a separate effort described by GRID-DETECT-01.

## Current state

- Ownership runs immediately before extraction and records `grid_rows`, `grid_cols`, cell bounds, and blob ownership. Automatic inference currently requires full-width or full-height blank gaps; this misses separators that shift between sprite rows.
- The image viewer already has Inspect and Draw controls. Draw provides a precedent for edit mode, undo/redo, cancel, save, and save-as-copy controls.
- Processing settings can request a rerun from an affected stage, and graph artifacts carry branch IDs. The current runner does not apply an edited grid to an arbitrary selected branch or create a pipeline copy from that branch.
- In `run_20261006_110113`, a 1024×1024 source contains 16 visibly separated sprite bodies, while automatic ownership produces a 3×4 grid and 12 cells.

## Proposed behavior

Canonical flow **MANUAL-GRID-01** is recorded in this file's header.

Add a dedicated spritesheet diagnosis stage after blob ownership and before sprite extraction. Show the unmodified source sheet, inferred grid split lines, per-cell sprite IDs, and the component IDs assigned to each cell. Keep the existing ownership main/debug images available as evidence, while diagnosis becomes the review surface for this handoff.

Place a settings icon beside Inspect and Draw when viewing a run's diagnosis artifact. Opening it starts an edit session with the source and selected branch pinned. In the details frame's upper-left corner, show editable row and column counts and the source pixel dimensions. Counts are positive integers; changing either count recalculates candidate split positions from the existing foreground/ownership evidence and refreshes the grid and labels immediately. Do not run extraction or later pipeline stages during preview, and do not offer direct split dragging.

Provide Save, Save as copy, Cancel, Undo, and Redo, following Draw's edit-session interaction patterns. Each committed row/column value change is one history entry; a new change after undo clears redo. Cancel or Escape discards the draft and returns to the diagnosis view. Save and Save as copy are enabled only when settings differ from the starting values. During save, disable conflicting edits and preserve the session on failure.

On Save, replace the selected branch's pipeline result only after ownership and all dependent stages complete successfully. On Save as copy, create a child branch from the selected branch and run the same stages there; preserve the source branch's artifacts and selection history. Keep stable branch and node IDs in the manifest, and never publish a partial run as a completed replacement. Reuse earlier branch results only when their settings and inputs match the new run's cache keys.

## Implementation sequence

1. Add the diagnosis preview and metadata between ownership and extraction. Expose resolved row/column counts, split positions, cell IDs, and component-to-cell ownership to the app.
2. Add a grid edit session to the viewer. Store baseline and draft counts, lightweight preview output, and undo/redo history; reuse Draw's save/cancel interaction conventions.
3. Pass the selected branch ID, requested save mode, and edited counts through the existing processing request. Extend runner graph handling to replace that branch on Save or append a distinct child branch on Save as copy, then regenerate from ownership through assembly.
4. Update the owning flow documentation and add focused processor/app coverage for preview, history, and branch persistence.

## Interface changes

- Diagnosis metadata exposes grid counts, source dimensions, resolved split positions, cell identities, and blob ownership. Its preview image is the original sheet with readable boundaries and labels.
- The edit request carries `grid_rows`, `grid_cols`, `selected_branch_id`, and a save mode distinguishing replacement from copy. Existing settings names remain the canonical row/column fields.
- The runner reports the resulting branch and graph artifacts through the current manifest/event interfaces so the app can select the saved result.

## Acceptance and validation

- Opening diagnosis shows the original source with grid boundaries and cell/blob labels after ownership and before extraction.
- Editing the failing run to four rows and four columns immediately shows 16 labeled cells and the detected separators, without starting downstream stages.
- Multiple count edits, undo, redo, redo invalidation after a new edit, Cancel, and Escape preserve the expected draft or baseline state.
- Save on a selected non-primary branch regenerates and replaces that branch; the other branches remain intact. Save as copy creates a new child branch with a completed final sheet while preserving its parent.
- A failed stage leaves the previous selected-branch result usable and keeps edits available for correction or retry.
- Existing callers that do not provide a selected branch continue to process the primary branch. Grid overrides, ownership overrides, empty cells, and detached decorations remain supported.

## Relationship to automatic detection

The editor lets a user choose row and column counts; it does not replace automatic split inference. Detection of separator positions and robust row-local gap matching are specified separately in [Spritesheet detection improvement plan](spritesheet-detection-plan.md) (GRID-DETECT-01). The diagnosis preview uses the same resolved split and ownership result as extraction so the displayed assignments explain downstream crops.

<!-- agent: gpt-6 | 2026-10-06 | Plan manual spritesheet grid editing | v 1.0.0 | 591694 -->
