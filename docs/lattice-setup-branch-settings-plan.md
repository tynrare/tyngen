<!--
File version: 1.2.0
Last updated: 2026-10-07
Purpose: Plan shared setup editing and durable branch settings across the pixel pipeline.
Flow SETUP-SAVE-01: canonical proposed contract; pinning owns rerun persistence.
1. User → open grid or lattice Setup → app captures branch ID, branch revision, inputs, current settings, and isolated draft; opening does not pin.
2. User → edit, undo, or redo → app validates draft and requests a versioned preview; only the latest successful preview enables saving.
3. User → Cancel or Escape → app discards draft and preview; branch settings and artifacts remain intact.
4. User → Save → runner stages a replacement for the selected branch using temporary overrides; success replaces its current result/settings without creating or updating a pin snapshot.
5. User → Save as copy → runner creates a distinct branch with a complete settings snapshot and pinned=true; success publishes the new branch and selects it.
6. Runner → fail, cancel, or detect changed base revision → keep published branch results/settings intact and retain draft for retry.
7. User → unpin a copied branch → app persists pinned=false; its current result remains visible until the next rerun.
8. User → rerun the pipeline → runner rebuilds primary from rerun defaults and every pinned branch from its own pin snapshot; unpinned extra branches disappear on successful publication.
9. Runner → update any other branch → preserve pinned branch snapshots, input provenance, artifacts, and identity.
10. Runner → fail or cancel a rerun → retain the previous published graph, including branches awaiting retirement.
Invariants: preview never publishes run state; branch ancestry is provenance, not a live settings dependency; replacement keeps branch identity; copy creates identity and pin; primary always exists; unpin defers branch retirement until rerun; settings and artifacts commit together.
Scope index: this file owns proposed SETUP-SAVE-01; lib.c owns editor UI and workers (MANUAL-GRID-01); runner.cpp owns execution/publication (PIXEL-02); stage_lattice.cpp owns fitting. Implementation should reference these steps and update existing owning flows without duplicating this playbook.
-->
<!-- agent: gpt-6 | 2026-10-07 | Specify stable manual save branching contract | v 1.2.0 | 6cfdf4 -->

# Lattice setup and branch settings plan

## Requested behavior

Port the grid Setup experience to lattice: editable options, preview, undo/redo, Cancel, Save, and Save as copy. Use one persistence contract for both editors and future pipeline features.

Pinning is the persistence contract. Save replaces the current branch's result and settings for the current run, without pinning them. Save as copy creates and pins a new branch together with its settings. Each pipeline rerun regenerates all pinned branches using their respective saved settings. Unpinning preserves the visible result now and removes the extra branch on the next successful rerun. Primary is regenerated rather than removed.

Proposed edge rule: Save on an already pinned branch changes its current result but leaves the existing pin snapshot unchanged; rerun restores that snapshot. This follows the rule that Save does not persist new settings. Updating a pin snapshot in place would need an explicit future action; Save as copy already provides a way to pin the edited configuration.

## Findings from the current working tree (2026-10-07)

Grid and lattice now share Setup, Save/Copy, preview, and pin controls. The earlier implementation inventory is superseded by this audit of the current uncommitted source.

| Observed path | Issue | Proposed correction |
| --- | --- | --- |
| `lib.c:start_pixel`, `write_pixel_settings` | Every operation writes the mutable run settings file, including setup saves. Temporary edits can become the next unpinned rerun defaults. | Separate rerun defaults from immutable operation settings; setup requests never update defaults. |
| `branch_settings_path` | Missing branch settings fall back to the latest run settings; primary has no uniform branch settings record. | Resolve by branch ID and revision; reject unresolved provenance rather than borrow another branch configuration. |
| `runner.cpp:working_branch`, `manifest_snapshot` | Replacement/replay uses selected ID as both output ID and parent, then overwrites existing metadata. This can create self ancestry. | Preserve ancestry on replacement; assign parent only on copy; validate acyclic provenance. |
| `resolve_branch` | Recursive parent overlay has depth limit 32 and depends on current parent stages. | Complete branch execution records and immutable input references; iterative validation without an arbitrary depth limit. |
| Copy publication | Pin file and output aliases are published before the final manifest; copy stage records initially use primary identity. | Allocate destination identity before execution; publish settings, artifacts, pin state, and graph in one transaction. |
| Full rerun | Primary publishes first; pinned branches replay through recursive runner processes and publish individually. A late failure leaves a partially replaced graph. Final publication also uses the outer revision after child revisions advanced it. | One coordinator, frozen branch work list, one generation commit; stage every branch before publication. |
| Lattice shared fitting failures | Some failure paths write the manifest even during setup save, unlike ordinary setup failure paths. | Route every failure through one abort path with no published graph mutation. |
| Pin storage | File existence determines pin state; UI writes/deletes pins separately from manifest. | One authoritative graph pin record; immutable referenced snapshots, transactional pin actions. |
| UI capacity (`lib.h`) | 64 stage slots and 64 output slots can hide branches long before 64 complete pipelines fit. | Separate branch registry from artifact views; paged/lazy artifact loading, explicit capacity errors, no silent truncation. |

Revision snapshots and existing stage signatures are useful foundations. The contract needs to govern UI actions, CLI/MCP entry points, fresh/full reruns, selected-stage reruns, startup, cleanup, and recovery equally.

## Shared settings model

Extend the graph schema with a uniform branch record, including primary: stable ID, current revision, `pinned` boolean, optional `pinned_settings_path`, `result_settings_path`, and immutable input provenance. Settings snapshots contain all pipeline fields, sprite overrides, and ownership overrides. Reuse existing revision storage; avoid a second settings database. UI selection and preview input capture must never implicitly set `pinned`.

`result_settings_path` records exactly what produced the displayed result. Save changes that reference only. Save as copy initializes both settings references and sets `pinned=true` on its new branch. Unpin persists `pinned=false` immediately, without deleting current artifacts. A successful pipeline rerun publishes primary plus regenerated pinned branches and any new automatic alternatives; previous unpinned extra branches are retired. Automatic alternatives are unpinned unless explicitly pinned.

Primary rerun settings come from the existing explicit rerun request/profile policy, excluding temporary setup overrides. Pinned branches use their complete stored configurations, independent of those defaults. Persist pin records in the graph and carry them through startup and every publication. Full rerun schedules primary and each pinned branch once, keeping stable pinned IDs and preventing recursive copy creation. A selected-stage rerun uses the same persistence rules for its affected scope; it must never erase unrelated pinned branches.

Copy snapshots the effective draft of the selected branch, including earlier temporary changes visible when the editor opened. Store a complete effective configuration so later profile/default/parent edits cannot change the copy. Preserve auto values when the user selected auto; persist a concrete pitch for a branch created to represent a ranked fixed pitch.

Retain ancestry for provenance and immutable prefix references for the current execution. On full rerun, rebuild each pinned branch's necessary upstream stages from the immutable run source and its own complete saved configuration; reuse matching immutable inputs where valid. A changed parent grid must never redirect a child's extraction inputs or sprite identities. Pinned means preserve the configuration and branch identity, while outputs are regenerated. Branch selection reloads settings by branch ID and revision rather than a run-wide loaded flag.

Existing branches default to unpinned during migration; do not silently turn historical alternatives into permanent branches. For existing schema-2 runs, prefer settings adjacent to that branch's revision artifacts; use the global legacy settings only when provenance is unambiguous. A ranked pitch branch needs its stored pitch applied. If settings provenance cannot be recovered, mark it unresolved and require explicit settings before rerun; do not silently borrow another branch's settings. Preserve legacy artifacts for viewing.

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

Open Setup on lattice main/debug artifacts by stable step identity. Capture source, mask, extraction metadata, sprite crops, and branch settings for the edit session; this is independent of persistent pinning. Preview runs fitting only into an isolated temporary directory. Share the runner's automatic shared-pitch selection with preview, so preview and save produce the same fit; directly invoking the fitter alone would miss the runner's second fitting pass. Display confidence and resolved pitch/dimensions alongside the overlay.

Extract the existing grid session mechanics into a small shared session with editor-specific controls and preview adapters. Reuse version tracking, history, dirty checking, cancellation, and save completion. History stores draft settings changes; applying a new change after Undo clears Redo. Debounce rapid edits, keep one preview active, and enable Save only when the successful preview version matches the current draft. Neither preview nor editor opening writes run settings.

## Operation matrix and reset policy

| Operation | Identity and current result | Persistent settings | Rerun behavior |
| --- | --- | --- | --- |
| Save on unpinned branch | Replace selected ID only | Remains unpinned | Primary uses rerun defaults; extra branch retires on successful full rerun. |
| Save on pinned branch | Replace selected ID only | Keep existing pin and its old snapshot | Restore old snapshot on rerun. UI must disclose this before saving. |
| Save as copy | New ID, select it; parent unchanged | Pin complete effective draft | Rebuild same ID from that snapshot. |
| Pin | Current result unchanged | Snapshot current effective settings, including fixed alternative pitch | Rebuild from new snapshot. Already pinned is idempotent. |
| Unpin | Current result remains visible | Remove pin reference | Retire extra branch at next successful full rerun; primary survives. |
| Selected-stage rerun | Replace selected ID; no implicit child | Resolve pin or defaults, never temporary result settings | Rebuild from earliest invalid stage, even if earlier than selected stage; preserve unrelated branches. |
| Full/fresh rerun | Primary plus pinned IDs and new automatic alternatives | Pins remain unchanged | Rebuild pinned branches exactly once; retire old unpinned extras on success. Fresh forces recomputation. |
| Reset draft | Restore editor baseline | No change | No processing. |
| Reset defaults | Change explicit rerun defaults | Pins unchanged | Takes effect on next unpinned primary rerun. |
| Reset branch to auto | Use ordinary Save or Copy with auto settings | Determined by chosen save action | Auto is a setting, not an unpin action. |
| Artifact cleanup | Remove rebuildable outputs in a committed cleanup transaction | Keep defaults, pins, source, and required provenance | Pinned branches remain rebuildable. |
| Full cleanup | Clear processing graph and settings, keep source | Remove all pins explicitly | Next processing starts a new primary. |

Save on a pinned branch retains its pin, rather than implicitly unpinning it. This is the proposed interpretation of “does not pin”: Save never modifies pin state or pin settings. The distinction between current result settings and rerun settings must be visible in Setup. Selected-stage rerun currently creates children in some cases; change that behavior to the matrix above, and reserve branching for explicit Copy.

## Generic tools and transaction design

Use small shared helpers rather than more editor-specific persistence paths:

- `resolve_settings(branch, purpose) -> snapshot`: purpose is edit, rerun, or copy. Edit uses current result; pinned rerun uses pin; unpinned rerun uses explicit defaults. Copy freezes the complete effective draft.
- `plan_operation(intent, base_generation, branch_revision) -> plan`: table-driven intents, destination ID, affected stages, settings hash, source fingerprint, and expected publication generation.
- `validate_graph(graph) -> errors`: unique safe IDs, no self/cyclic ancestry, live settings/artifact references, branch-local sprite provenance, valid hashes. Removed ancestors remain historical provenance, never required execution parents.
- `stage_execution(plan) -> candidate`: immutable operation directory, complete stage records, no published aliases. Matching source/input/settings/processor signatures allow shared cached stages.
- `commit_generation(candidate, expected_generation) -> receipt`: per-run writer lock, compare generation, durable snapshots, atomic manifest replacement. Receipt includes committed generation and selected branch ID.
- `collect_unreferenced(graph, jobs) -> paths`: mark live results, pins, prefixes, active jobs, and retained recovery generations before deleting anything.

The manifest is the publication authority. Prefer artifacts addressed directly through immutable generation paths. Compatibility aliases are derived after commit and repairable on startup; they cannot define the displayed result. Pin blobs are immutable settings snapshots, not a second mutable registry. A crash before commit leaves removable staged files; a crash after commit leaves a complete authoritative graph. Concurrent requests carry unique operation IDs; retries return their existing receipt, avoiding duplicate copies. Reject stale drafts with a refresh/reapply action. Never silently overwrite a newer branch.

A full rerun freezes its work list at planning time. Each pin has an independent complete configuration; ancestry never triggers recursive copies or replay. Stage all planned branches, validate all outputs, then publish once. Failure or cancellation anywhere retains the complete prior generation. Pin changes are serialized against that transaction. Cache by content and reuse immutable stage outputs instead of copying whole trees for every branch; bounded workers and a paged UI keep branch count independent of active memory.

## Implementation order

1. Define the operation matrix, schema migration, result/pin/default separation, and reset semantics; update owning flow references to SETUP-SAVE-01.
2. Add uniform primary/branch records, stable IDs, revisions, complete execution provenance, strict settings resolution, and graph validation. Recover legacy settings from their own revisions; preserve unresolved legacy results for viewing.
3. Implement coordinator, immutable operation storage, writer lock, stale request detection, atomic generation commit, recovery, and deferred collection. Remove per-child published reruns.
4. Route both editors, pin controls, CLI/MCP, selected/full/fresh reruns, and cleanup through the shared intents. Replace global settings writes during Setup. Return a receipt so UI selects the correct result and reloads committed settings.
5. Remove fixed branch visibility limits through paged artifact views; share stage outputs by signatures; cap active workers. Keep fixed algorithmic scratch buffers where appropriate.
6. Implement the suite below, reconcile runner/UI flow headers and processor docs, then integrate small commits into master.

## Heavy branching example

Notation: P is primary; G1 is grid 8×3; L1 is G1 with pitch 4; L2 is G1 with pitch 5. D0 and D1 are explicit rerun defaults.

1. Process P with D0. Grid Save with 7×3 replaces P temporarily. Full rerun restores D0.
2. Grid Copy 8×3 from P creates pinned G1. Lattice Copy pitch 4 from G1 creates pinned L1; sibling Copy pitch 5 creates pinned L2. All three own complete settings, including the 8×3 grid.
3. Save pitch 6 on L1: L1 displays pitch 6, its pin stays pitch 4. Copy that displayed draft creates pinned L3 with pitch 6. Rerun L1 restores pitch 4 without changing L3.
4. Create 100 siblings from G1 and a chain of 80 alternating grid/lattice copies from L3. Restart: every branch and snapshot remains available beyond current UI limits; editing the deepest branch requires no ancestor traversal.
5. Save grid 9×3 on G1, then Copy it to G2. G1's pin remains 8×3; G2 pins 9×3. Existing lattice branches keep their own 8×3 extraction and sprite provenance.
6. Unpin G1 and every second sibling. Set defaults to D1. Start full rerun and inject a failure at the final pinned branch: published results remain exactly as before, including unpinned branches awaiting retirement.
7. Retry successfully: P uses D1; G1 and unpinned siblings disappear; all pinned descendants regenerate once with stable IDs and their independent snapshots. Ancestry to G1 remains historical.
8. Reset L2 draft to auto, then Cancel: no change. Reopen, reset to auto, Copy: pinned A1 uses auto fitting on future reruns. Its previously resolved pitch remains result metadata, not a substituted fixed setting.
9. Artifact cleanup and restart retain pins/defaults/source; rerun rebuilds the same pinned set. Explicit full cleanup removes every pin and result; processing creates only P and new unpinned automatic alternatives.

## Proposed test suite

| Layer | Cases | Required assertions |
| --- | --- | --- |
| Contract units | Parameterize grid/lattice × primary/extra × pinned/unpinned × Save/Copy | Correct identity, result snapshot, pin snapshot, parent, defaults, and selected result; no unrelated mutation. |
| Settings resolution | Edit versus rerun, temporary Save then Copy, auto/manual pitch, sprite overrides, fixed alternatives, missing legacy snapshots | Complete settings and branch-local provenance; no fallback to another branch; defaults never absorb temporary saves. |
| Graph units | Self/cyclic ancestry, missing parents, depth 80, duplicate/unsafe IDs, deleted ancestor | Reject corruption; historical ancestry does not prevent independent execution. |
| Coordinator integration | Full/fresh/selected-stage reruns, earliest invalid stage, removed pins, unchanged signatures | Every pinned ID runs once; unrelated branches untouched; no implicit copies; safe cache reuse. |
| Failure injection | Fail/cancel each stage, shared lattice pass, snapshot write, disk full, manifest rename, last pinned replay | Prior manifest and referenced artifact hashes unchanged before commit; draft retryable; no orphan active pins. |
| Crash recovery | Kill before/after every publication boundary, restart, replay same operation ID | Old or complete new generation, never mixed; receipt replay creates no duplicate branch. |
| Concurrency | Two copies, two Saves, Save versus rerun, pin/unpin versus rerun, cleanup versus active job | Serialized valid publication or explicit conflict; unique IDs; no lost updates or deleted live inputs. |
| UI integration | Rapid previews, Undo/Redo, new edit after Undo, stale completion, cancel, selection after receipt | Latest valid preview only; no draft leakage; correct branch settings after restart or failure. |
| Reset/cleanup | Draft reset, default reset, auto Save/Copy, artifact/full cleanup, pinned child of unpinned parent | Exact matrix behavior; referenced snapshots and source retained until explicitly cleared. |
| Scale | 1,000 branches, chain depth 200, repeated rerun/unpin/copy/reset cycles | No truncated registry; stable IDs; bounded active workers/memory; no ancestry recursion; no exponential replay. |
| Model sequences | Seeded randomized actions against a minimal in-memory contract oracle | Compare graph/pins/defaults after every commit; minimize and retain failing sequences. |

Extend `pixelprocess/tests/pipeline_test.cpp` for real processor integration and `tests/cleanup_test.c` for cleanup invariants. Use a cheap deterministic stage fixture for thousands of randomized transactions, plus a smaller real-image suite for grid/lattice provenance and preview parity. Benchmark branch scheduling separately from image processing; record metadata time, peak memory, cache hits, and work count before choosing numeric budgets.

No runtime changes or tests are included in this planning update.

<!-- agent: gpt-6 | 2026-10-07 | Specify stable manual save branching contract | v 1.2.0 | 6cfdf4 -->
