<!--
File version: 1.2.0
Last updated: 2026-10-07
Purpose: Define shared settings tweaking and branching behavior for grid and lattice.
Flow SETUP-SAVE-01: committed settings, dependency, and save contract.
1. User → open Setup → editor captures selected branch ID/revision and current effective settings in an isolated draft.
2. User → tweak settings or undo/redo → editor previews the latest draft without changing published settings or pins.
3. User → Cancel or reset draft → editor discards changes or restores its opening baseline; branch remains unchanged.
4. User → Save → processor replaces the selected branch under the same ID, updates its stage-owned overrides, and rebuilds affected pinned consumers in one transaction.
5. User → Save as copy → processor creates and selects a pinned fork at the edited stage; it inherits upstream settings and records only the edited stage overrides.
6. User → rerun → processor compares consumed inputs/settings, rebuilds changed consumers, and advances the selected branch revision.
7. User → full rerun → processor schedules pinned consumers once and switches the complete graph only after success.
8. Processor → fail, cancel, or detect stale base revision → keep published settings/results intact and retain the draft for retry or reapplication.
Invariants: Save preserves pin state and identity; a pinned branch uses its current effective settings; copies inherit outside their explicit overrides; ancestry records fork provenance while consumed inputs drive invalidation; one current pointer publishes graph, settings, pins, and artifacts together.
Scope index: this file owns SETUP-SAVE-01; lib.c owns editor requests (MANUAL-GRID-01); pixelprocess/runner.cpp owns execution/publication (PIXEL-02). The plan in dump is historical and superseded.
-->
<!-- agent: gpt-6 | 2026-10-07 | Document transactional settings behavior | v 1.2.0 | 918426 -->

# Settings tweaking and branching

## Scope

Use the same contract for spritesheet grid, lattice, and future manual settings editors. UI, CLI, and other entry points must request the same operations. Grid and lattice differ only in their controls, validation, preview, and affected processing stages.

## Settings ownership

Keep three distinct settings sources:

- **Current result settings:** the complete configuration that produced the displayed branch result. Setup opens with these settings.
- **Branch settings:** inherited upstream values plus the branch’s explicit stage-owned overrides; the resolved configuration is stored with the result.
- **Rerun defaults:** the explicit run/profile configuration used to regenerate unpinned primary. Manual Save must not rewrite it.

Each branch has a stable ID, original fork point, current result revision, settings provider, pin flag, and stage-owned overrides. A copy inherits current upstream values outside its overrides. Switching branches reloads the resolved settings recorded for that result.

## Action contract

| Action | Current branch | Pin and future rerun |
| --- | --- | --- |
| Save | Replace its settings/result; preserve ID. | Preserve the pin flag; if pinned, the saved settings become the rerun settings. |
| Save as copy | Keep original unchanged; create and select a pinned fork at the edited stage. | Inherit upstream settings outside stage-owned overrides. |
| Pin | Keep current result. | Preserve the branch across reruns; commit the pin in a new graph revision. |
| Unpin | Keep current result visible. | Clear preservation; affected extra branch retires on the next successful rerun. |
| Rerun selected branch | Replace selected ID; create no implicit copy. | Pinned branch uses its current saved settings; unpinned settings in scope reset to explicit rerun defaults. |
| Full/fresh rerun | Rebuild primary and pinned branches; preserve pinned IDs. | Retire affected unpinned extras only after success. Fresh forces recomputation. |

**Save on an already pinned branch:** keep the pin flag and replace the settings used by future reruns with the saved draft. Pinning means “preserve this branch across rerun”; it does not freeze an older settings snapshot.

A targeted rerun forces the selected node, then schedules dependent branch consumers in parent-before-child order. Each node reuses its result when its consumed artifact contents, relevant settings, and executable match. A selected save updates the stage-owned settings and applies the same dependency scheduling.

## Reset behavior

- **Reset draft:** restore settings captured when Setup opened; no branch or pin change.
- **Reset settings to auto/defaults:** edit the draft, then apply ordinary Save or Save as copy. Selecting auto does not unpin.
- **Change rerun defaults:** affect future unpinned processing; pinned branches keep their current saved settings.
- **Unpin parent:** pinned descendants remain independent and rerunnable after the parent retires.

Preserve auto as an instruction to fit again. Store resolved values as result metadata. A branch explicitly representing a fixed pitch must keep that fixed value in its current saved settings.

## Stable branching rules

A copy retains its fork point and stage-owned overrides. Upstream edits flow into values it does not override. Keep original ancestry as provenance and retain an unpinned provider internally while a pinned consumer depends on it.

Repeated Save produces no new branch. Repeated rerun produces no new pinned branch. Copy allocates a unique ID, including concurrent requests; retrying the same operation must not create another copy. Branch replacement preserves its original ancestry and must never make the branch its own parent.

Validate the draft base revision under the run lock. Stage settings, pins, artifacts, and manifest privately; publish them by switching one current-revision pointer after every required branch succeeds. On failure, cancellation, or stale base, leave that pointer unchanged.

The runner walks ancestry iteratively, reuses nodes by consumed-input signatures, and processes required branches sequentially. The desktop viewer retains its existing fixed artifact capacity.

## Runtime behavior

The runner records direct inputs/settings on each node, resolves copy settings through inherited values and stage-owned overrides, and runs dependent pinned branches parent-first. Each operation uses a locked private snapshot and commits through the current-revision pointer. The app captures the draft base revision, uses operation-specific confirmation text, and selects the branch named by the committed manifest.

## Example

1. Grid Save on primary changes 6×4 to 8×3 temporarily. Rerun restores the explicit primary defaults.
2. Grid Copy creates pinned G1 with 8×3. Lattice Copy from G1 creates pinned L1 with pitch 4 and sibling L2 with pitch 5; both retain grid 8×3.
3. Save pitch 6 on pinned L1 updates its displayed result and its next rerun settings. Copy that result creates pinned L3 with pitch 6. Rerun L1 keeps pitch 6.
4. Create nested grid/lattice copies. Each fork retains its original stage and only overrides owned by its edited stage.
5. Save grid 9×3 on G1, then Copy to G2. G1 remains pinned at 8×3 until saved; G2 pins 9×3. Existing descendants keep their own configurations.
6. Unpin G1 and half the siblings; change primary rerun defaults. A failed full rerun preserves all current results. A successful retry retires unpinned extras and rebuilds every pinned descendant once under its existing ID.
7. Reset L2 draft to auto and Cancel: unchanged. Repeat and Copy: new pinned A1 uses auto fitting on future reruns. Restart and rerun preserve the same branch settings and pin flags.

## Proposed tests

| Coverage | Assertions |
| --- | --- |
| Grid/lattice × primary/extra × pinned/unpinned × Save/Copy | Stable branch identity, stage-owned overrides, inherited upstream values, and unchanged pin state where required. |
| Save on pinned branch; copy from temporary result | Save updates its settings and consumers; copy inherits the active upstream context plus edited-stage overrides. |
| Branch switching, restart, parent edits/unpinning | Correct settings reload; inherited values update while explicit overrides and original fork points persist. |
| Selected/full/fresh reruns | Correct settings source, earliest invalid stage, no implicit copies, every pin processed once. |
| Reset, auto/manual values, Undo/Redo, Cancel | Draft isolation, exact reset semantics, latest preview only. |
| Failure/cancel/stale draft/concurrent operations | Prior publication retained, explicit conflicts, unique IDs, idempotent retries. |
| Nested grid/lattice copies and a retired unpinned provider | Original fork points stay fixed; required provider settings remain available to pinned consumers. |
| Stale save, failed stage, canceled run, and competing writers | Current revision pointer stays on the prior complete graph. |

This document records the implemented contract. Schema 4 is required; prior graph schemas need a fresh run.

<!-- agent: gpt-6 | 2026-10-07 | Document transactional settings behavior | v 1.2.0 | 918426 -->
