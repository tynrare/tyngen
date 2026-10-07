<!--
File version: 1.0.0
Last updated: 2026-10-07
Purpose: Plan shared setup editing and durable branch settings across the pixel pipeline.
Flow SETUP-SAVE-01: canonical proposed contract, pending confirmation of temporary Save semantics.
1. User → open grid or lattice Setup → app pins branch ID, branch revision, inputs, baseline settings, and isolated draft.
2. User → edit, undo, or redo → app validates draft and requests a versioned preview; only the latest successful preview enables saving.
3. User → Cancel or Escape → app discards draft and preview; branch settings and artifacts remain intact.
4. User → Save → runner stages a replacement for the selected branch using temporary overrides; success replaces its current result and keeps its rerun baseline unchanged.
5. User → Save as copy → runner creates a distinct branch with a complete durable settings snapshot and pinned input prefix; success publishes the new branch and selects it.
6. Runner → fail, cancel, or detect changed base revision → keep published branch results/settings intact and retain draft for retry.
7. User → rerun a branch → runner resolves that branch's durable baseline; temporary overrides from step 4 are discarded, copied settings from step 5 remain effective.
8. Runner → update any other branch → preserve copied branch settings, input prefix, artifacts, and identity.
Invariants: preview never publishes run state; branch ancestry is provenance, not a live settings dependency; replacement keeps branch identity; copy creates identity; settings and artifacts commit together.
Scope index: this file owns proposed SETUP-SAVE-01; lib.c owns editor UI and workers (MANUAL-GRID-01); runner.cpp owns execution/publication (PIXEL-02); stage_lattice.cpp owns fitting. Implementation should reference these steps and update existing owning flows without duplicating this playbook.
-->
<!-- agent: gpt-6 | 2026-10-07 | Plan lattice setup and branch persistence | v 1.0.0 | 0eca6f -->

# Lattice setup and branch settings plan

## Requested behavior

Port the grid Setup experience to lattice: editable options, preview, undo/redo, Cancel, Save, and Save as copy. Use one persistence contract for both editors and future pipeline features.

Working interpretation: Save replaces the selected branch's current result, but its new overrides are temporary and disappear on rerun. Save as copy creates a branch whose saved settings survive reruns, application restarts, and updates to any other branch. Confirming this distinction is the only open persistence decision; if both actions should persist, step 4 also replaces the durable baseline.

## Findings from the current working tree

| Area | Existing behavior | Required change |
| --- | --- | --- |
| Grid editor (`lib.c`) | `GridEditState` pins a branch; versioned async preview, count history, Undo/Redo, Cancel exist. Footer says “Rerun branch.” | Shared setup session; explicit Save and Save as copy for both editors. |
| Grid save (`grid_edit_save`) | Mutates run settings before confirmation, clears save mode, requests a child revision. | Keep draft isolated; pass explicit save intent; preserve selected branch identity on replacement. |
| Settings (`write_pixel_settings`, `load_run_pixel_settings`) | One mutable run JSON, loaded once per run; sprite overrides are run scoped. | Resolve settings per branch/revision and sprite scope; atomic request snapshots. |
| Runner save modes | `replace`/`copy` are restricted to ownership; replacement rejects non-primary branches; copies are named as grid edits. | Generic stage setup request and arbitrary branch replacement/copy. |
| Manifest (`Branch`, read/write helpers) | Branches store ancestry, nodes, final, and optional pitch; one global `settings_path`. | Every branch needs a durable baseline and result settings provenance. |
| Reuse and ancestry | Missing branch stages are resolved from its parent's current graph; automatic alternatives apply `--sheet-pitch`. | Pin the input prefix; persist resolved alternative pitch so reruns reproduce that branch. |
| Evidence publication | Retained branch sprite records are rebuilt using the current operation's ownership metadata. | Resolve ownership and coordinates from each branch's own pinned inputs. |

Revision directories already snapshot effective settings. These are useful immutable execution records, but a branch currently has no authoritative reference to its own settings. The global settings path can describe the latest operation rather than primary or the selected branch.

Existing documentation in `dump/docs/manual-pipeline-tweaking-plan.md` describes Save/Copy semantics that the current grid UI does not implement. `pixelprocess/README.md` describes sibling retirement; the inspected runner loads and carries prior branches. Reconcile these descriptions against the final contract during implementation.

## Shared settings model

Extend the graph schema with a uniform branch record, including primary: stable ID, current revision, durable `baseline_settings_path`, `result_settings_path`, persistence category, and pinned input node references. Settings snapshots contain all pipeline fields, sprite overrides, and ownership overrides. Reuse the existing immutable revision storage; avoid a second settings database.

`baseline_settings_path` determines a rerun. `result_settings_path` records exactly what produced the displayed result. Save changes the result reference; Save as copy initializes both references on its new branch. This distinction makes temporary Save explicit and prevents a later feature from accidentally promoting overrides to persistent defaults.

Copy snapshots the effective draft of the selected branch, including earlier temporary changes visible when the editor opened. Store a complete effective configuration so later profile/default/parent edits cannot change the copy. Preserve auto values when the user selected auto; persist a concrete pitch for a branch created to represent a ranked fixed pitch.

Retain ancestry for provenance, but store the full resolved stage prefix or immutable node references at fork time. A changed parent grid must never redirect a child's extraction inputs or sprite identities. Branch selection reloads settings by branch ID and revision rather than a run-wide loaded flag.

For existing schema-2 runs, prefer settings adjacent to that branch's revision artifacts; use the global legacy settings only when provenance is unambiguous. A ranked pitch branch needs its stored pitch applied. If settings provenance cannot be recovered, mark it unresolved and require explicit settings before rerun; do not silently borrow another branch's settings. Preserve legacy artifacts for viewing.

## Lattice controls and preview

| Control | Canonical setting / behavior |
| --- | --- |
| Fit / pitch | Auto fitting or manual fractional `pitch`; existing algorithm remains authoritative. |
| Offset | `phase_x`, `phase_y`: auto or manual, in source pixels, matching existing metadata. |
| Search range | `min_pitch`, `max_pitch`, `pitch_precision`; use existing stage validation. |
| Logical size | Show resolved width × height for each sprite; `min_sprite_cells` controls the existing minimum constraint. |
| Shared fit tolerance | `sprite_pitch_tolerance` and `alternate_pitch_max_percent`. |
| Scope | Sheet defaults or selected sprite; current sprite overrides support pitch and phase only. |

The current fitter uses a single square-cell pitch, not independent X/Y pitches or an exact width/height constraint. Initially expose calculated logical dimensions and existing size controls. If exact dimensions or rectangular lattice cells are desired, that requires an additional fitting specification and processor work.

Open Setup on lattice main/debug artifacts by stable step identity. Pin source, mask, extraction metadata, sprite crops, and branch settings. Preview runs fitting only into an isolated temporary directory. Share the runner's automatic shared-pitch selection with preview, so preview and save produce the same fit; directly invoking the fitter alone would miss the runner's second fitting pass. Display confidence and resolved pitch/dimensions alongside the overlay.

Extract the existing grid session mechanics into a small shared session with editor-specific controls and preview adapters. Reuse version tracking, history, dirty checking, cancellation, and save completion. History stores draft settings changes; applying a new change after Undo clears Redo. Debounce rapid edits, keep one preview active, and enable Save only when the successful preview version matches the current draft. Neither preview nor editor opening writes run settings.

## Implementation order

1. **Settings contract and migration:** define graph schema, branch baseline/result references, pinned prefixes, request fields, and rerun resolution. Update PIXEL-02 and schema documentation first.
2. **Runner persistence:** generalize save mode beyond ownership; implement replacement of primary and non-primary branches plus durable copy creation. Stage work privately, then commit branch settings and completed graph together. Detect stale base revisions. Avoid global alias writes during copy or failed replacement.
3. **Grid adoption:** move grid drafts out of run state; wire explicit Save/Copy and branch settings reload. Establish the shared behavior before introducing lattice controls.
4. **Lattice preview adapter:** share fitting orchestration between full processing and preview, including auto shared pitch, sprite overrides, and quality errors.
5. **Lattice UI:** reuse Setup entry, history, preview, save/copy/cancel, and disabled states. Add validated controls from the table and select the resulting branch on success.
6. **Pipeline audit:** carry settings through fresh/full reruns, selected-stage reruns, child revisions, auto alternatives, sprite overrides, cleanup, and startup reload. Preserve each branch's evidence and prevent stage cleanup from deleting referenced settings or prefix artifacts. Update flow headers and user documentation.

Make each phase a small commit. Preserve the current uncommitted source changes; integrate only completed task work into master. Planning does not change runtime behavior.

## Validation planned for implementation

- Grid and lattice: open/edit/Undo/Redo/new-edit-after-Undo/Cancel; drafts never alter branch settings.
- Latest preview only: rapid edits, invalid values, failed previews, cancel during work, and save readiness after a stale worker finishes.
- Save on primary and non-primary: branch ID remains stable; displayed result updates; rerun discards temporary overrides under the proposed semantics.
- Copy from primary, non-primary, and a temporary edited result: unique branch, full settings snapshot, unchanged parent, correct selected result.
- Rerun a copied branch after restart, parent grid changes, sibling settings changes, and full primary rerun: copied settings and input provenance remain intact.
- Inject failure/cancel before final commit: earlier branch settings and artifacts remain usable; draft stays available for retry.
- Auto/manual pitch and phase, sprite override isolation, alternative fixed-pitch rerun, grid changes affecting sprite IDs, and branch-specific sprite coordinates.
- Settings changes invalidate the earliest affected stage; unrelated earlier stages reuse pinned inputs. Copied branches remain independent of global aliases.
- Legacy migration with and without recoverable branch settings; cleanup retains every live settings/prefix reference.

No tests were run for this planning-only change.

<!-- agent: gpt-6 | 2026-10-07 | Plan lattice setup and branch persistence | v 1.0.0 | 0eca6f -->
