<!--
File version: 1.0.0
Last updated: 2026-10-07
Purpose: Define shared settings tweaking and branching behavior for grid and lattice.
Flow SETUP-SAVE-01: canonical proposed settings and branching contract.
1. User → open Setup → editor captures selected branch ID/revision and current effective settings in an isolated draft.
2. User → tweak settings or undo/redo → editor previews the latest draft without changing published settings or pins.
3. User → Cancel or reset draft → editor discards changes or restores its opening baseline; branch remains unchanged.
4. User → Save → processor replaces selected branch result/settings under the same ID; pin state and pin snapshot remain unchanged.
5. User → Save as copy → processor creates and selects a new branch with a complete draft settings snapshot and a pin; original branch remains unchanged.
6. User → rerun → processor uses pinned settings for pinned branches and explicit rerun defaults for unpinned primary; temporary Save settings are overridden.
7. User → full rerun → processor rebuilds primary and each pinned branch once; successful publication retires old unpinned extra branches.
8. Processor → fail, cancel, or detect stale base revision → keep published settings/results intact and retain the draft for retry or reapplication.
Invariants: Save never changes pins; Copy creates independent settings and identity; ancestry is historical provenance, not a live settings dependency; settings/results publish together; unrelated branches remain unchanged.
Scope index: this file owns SETUP-SAVE-01; lib.c owns editor requests (MANUAL-GRID-01); pixelprocess/runner.cpp owns execution/publication (PIXEL-02). The plan in dump is historical and superseded.
-->
<!-- agent: gpt-6 | 2026-10-07 | Focus settings tweaking and branching contract | v 1.0.0 | 7d3663 -->

# Settings tweaking and branching

## Scope

Use the same contract for spritesheet grid, lattice, and future manual settings editors. UI, CLI, and other entry points must request the same operations. Grid and lattice differ only in their controls, validation, preview, and affected processing stages.

## Settings ownership

Keep three distinct settings sources:

- **Current result settings:** the complete configuration that produced the displayed branch result. Setup opens with these settings.
- **Pinned settings:** the complete configuration preserved for a branch's future reruns.
- **Rerun defaults:** the explicit run/profile configuration used to regenerate unpinned primary. Manual Save must not rewrite it.

Each branch has a stable ID, result revision, current settings reference, optional pin snapshot, and its own processing inputs. Include grid, lattice, sprite overrides, and ownership overrides in every complete snapshot. Switching branches reloads that branch's settings.

## Action contract

| Action | Current branch | Pin and future rerun |
| --- | --- | --- |
| Save | Replace its settings/result; preserve ID. | Do not create, remove, or update its pin. |
| Save as copy | Keep original unchanged; create and select a new branch. | Pin the copy's complete effective draft. |
| Pin | Keep current result. | Snapshot current result settings; repeated Pin is idempotent. |
| Unpin | Keep current result visible. | Remove persistence; extra branch retires on next successful full rerun. |
| Rerun selected branch | Replace selected ID; create no implicit copy. | Pinned branch uses its pin snapshot; unpinned branch uses rerun defaults. |
| Full/fresh rerun | Rebuild primary and pinned branches; preserve pinned IDs. | Retire old unpinned extras only after success. Fresh forces recomputation. |

**Save on an already pinned branch:** the edited result is temporary; the old pin snapshot stays authoritative for rerun. Show that consequence in the editor. Save as copy preserves the edited configuration as a new pinned branch.

A targeted rerun preserves unrelated branches and recomputes from the earliest stage invalidated by resolved settings, even when that stage precedes the selected point. An unpinned extra branch may be rerun explicitly while visible; full rerun retires it.

## Reset behavior

- **Reset draft:** restore settings captured when Setup opened; no branch or pin change.
- **Reset settings to auto/defaults:** edit the draft, then apply ordinary Save or Save as copy. Selecting auto does not unpin.
- **Change rerun defaults:** affect future unpinned processing; pinned branches keep their snapshots.
- **Unpin parent:** pinned descendants remain independent and rerunnable after the parent retires.

Preserve auto as an instruction to fit again. Store resolved values as result metadata. A branch explicitly representing a fixed pitch must snapshot that fixed value.

## Stable branching rules

Copies include earlier temporary settings visible in their source branch. Later parent or sibling edits never alter a copy's grid, lattice, sprite settings, or extraction inputs. Keep ancestry for provenance; never resolve execution settings from the parent's latest result.

Repeated Save produces no new branch. Repeated rerun produces no new pinned branch. Copy allocates a unique ID, including concurrent requests; retrying the same operation must not create another copy. Branch replacement preserves its original ancestry and must never make the branch its own parent.

Validate branch revisions before publishing a save. Reject stale drafts and allow refresh/reapplication. Stage settings and outputs privately, then publish together. A full rerun publishes only after all planned branches succeed, so a failure cannot leave half the branch set replaced.

Deep or wide branching must not depend on recursive ancestry traversal or fixed UI artifact limits. Reuse immutable processing outputs when input/settings signatures match; bound active processing work independently of branch count.

## Implementation plan

1. Define shared operation intents and uniform branch records, including primary; separate result settings, pin snapshots, and rerun defaults.
2. Add strict branch settings resolution and complete copy snapshots. Migrate recoverable legacy branch settings from their own revisions; flag unresolved settings rather than borrow another branch's configuration.
3. Centralize Save, Copy, pin/unpin, and rerun planning/publication. Preserve identity and ancestry, detect stale revisions, and publish complete operations atomically.
4. Route grid and lattice through the same draft/session and operation helpers. Reload committed branch settings and select the copy returned by publication.
5. Support deep/wide branch registries and shared immutable stage reuse; update owning flow documentation and implement the contract tests below.

## Example

1. Grid Save on primary changes 6×4 to 8×3 temporarily. Rerun restores the explicit primary defaults.
2. Grid Copy creates pinned G1 with 8×3. Lattice Copy from G1 creates pinned L1 with pitch 4 and sibling L2 with pitch 5; both retain grid 8×3.
3. Save pitch 6 on L1 changes its displayed result. Its pin remains pitch 4. Copy that result creates pinned L3 with pitch 6. Rerun L1 restores pitch 4.
4. Create 100 siblings and an 80-copy chain alternating grid/lattice tweaks. Every copy owns its settings; editing the deepest copy does not require parent traversal.
5. Save grid 9×3 on G1, then Copy to G2. G1's pin remains 8×3; G2 pins 9×3. Existing descendants keep their own configurations.
6. Unpin G1 and half the siblings; change primary rerun defaults. A failed full rerun preserves all current results. A successful retry retires unpinned extras and rebuilds every pinned descendant once under its existing ID.
7. Reset L2 draft to auto and Cancel: unchanged. Repeat and Copy: new pinned A1 uses auto fitting on future reruns. Restart and rerun preserve the same pin snapshots.

## Proposed tests

| Coverage | Assertions |
| --- | --- |
| Grid/lattice × primary/extra × pinned/unpinned × Save/Copy | Same operation semantics, stable replacement IDs, independent copy settings, unchanged defaults/pins where required. |
| Save on pinned branch; Copy from temporary result | Rerun restores old pin; copy preserves the currently edited settings. |
| Branch switching, restart, parent edits/unpinning | Correct settings reload; descendants stay independent, including sprite/extraction provenance. |
| Selected/full/fresh reruns | Correct settings source, earliest invalid stage, no implicit copies, every pin processed once. |
| Reset, auto/manual values, Undo/Redo, Cancel | Draft isolation, exact reset semantics, latest preview only. |
| Failure/cancel/stale draft/concurrent operations | Prior publication retained, explicit conflicts, unique IDs, idempotent retries. |
| 1,000 siblings, depth 200, repeated Save/Copy/unpin/rerun | No hidden/truncated branches, no ancestry recursion, bounded active work. |
| Seeded randomized action sequences | Published branch/settings/pin state matches a minimal contract model after every operation. |

This is a proposed contract and implementation plan; runtime behavior has not changed.

<!-- agent: gpt-6 | 2026-10-07 | Focus settings tweaking and branching contract | v 1.0.0 | 7d3663 -->
