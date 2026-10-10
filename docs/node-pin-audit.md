<!-- Version: 1.0.0 | Last updated: 2026-10-10 | Purpose: Audit pin, manual save, branching, and revision behavior. -->
<!-- agent: GPT-6 | 2026-10-10 | Audit node pins saves and graph ownership | v 1.0.0 | c2ac54 -->
# Pin and save audit

| Feature / condition | Before | Implemented |
| --- | --- | --- |
| Pin a stage | Snapshotted whole branch; primary had no branch record and could fail. | Node ID identifies a settings snapshot; primary and every processing stage can pin. |
| Save manual settings | Updated a pin only when branch was already pinned. | Always pins the edited node; keeps branch identity. |
| Save as | Created a pinned fork; cleanup/outline could write the wrong pin ID. | Pins the edited node in the new fork using its actual ID. |
| Color, cleanup, outline saves | UI offered them; runner accepted only background/grid/lattice. | All pipeline stage targets accepted. |
| Rerun | Branch pin suppressed resetting all branch settings. | Node overlays preserve owned settings; other settings follow rerun input/defaults. Outputs recompute. |
| Upstream change | Parent settings + explicit child overrides; affected pinned descendants replay. | Same dependency flow with node overlays after inheritance. |
| Multiple pins | Whole branch snapshot. | Final snapshot first, then stage order; later stage wins shared keys. |
| Unpin | Released whole branch. | Releases one node; last pin releases branch retention. Required providers can remain hidden. |
| Failure, cancellation, concurrent/stale write | Private snapshot, run lock, expected revision, atomic current pointer. | Same protections cover node pins and saves. |
| Source / final | Source immutable; finals used branch pin. | Source can pin (no owned settings); final pins all stage settings. |
| Sprite selections / drawing | Selection recipe always pinned, refreshed by stable sprite IDs; missing IDs remain partial/unavailable. Draw edits are durable derived images. | Their existing persistence remains separate from processing settings pins. |

Branch = stable fork identity, ancestry, fork stage, explicit settings overrides and owned stages. Node = branch/step identity with artifacts, cache signature, consumed inputs/settings. Revision = immutable operation snapshot; reruns replace current outputs under stable node IDs. History is not a branch.

Save as keeps the primary settings pointer with the primary graph, preventing a child draft becoming the primary baseline. Alternate generation is skipped during copy creation so the selected copy cannot accidentally become the last generated alternate.

## Remaining weak spots and refinements

- Stage ownership is duplicated across runner key lists, cache inputs, and UI reset code. Replace with one shared stage policy table; validate shared-key precedence (grid/separator, preview/cleanup, color/palette).
- `Branch.pinned` remains an internal retention flag backed by aggregate snapshots. Derive retention directly from node pins and consumer ancestry; expose only node pin state to users.
- JSON settings merging scans text rather than parsing top-level properties. Optional missing keys now insert correctly; replace the scanner with structured JSON merging.
- Derived drawing/selection nodes use separate persistence and selection nodes cannot unpin through this toggle. Unify their lifecycle with node metadata before offering the same toggle for these nodes.
- Full replay relies on stored branch order; explicitly sort ancestors before descendants and validate ancestry before execution.
- Publication of selection copies happens after the current pointer switch; an export refresh failure can report failure after graph commit. Move derived export refresh before publication.
- Cached/replayed processing nodes are preserved by settings, not frozen image bytes; input changes can change a pinned node's output.

## Contact points

- `lib.c`: `grid_edit_save`, `request_branch_rerun`, `node_pin_path`, `toggle_node_pin`, viewer Pin button; Settings drafts and confirmations.
- `pixelprocess/runner.cpp`: NODE-PIN-01; `stage_setting_overrides`, `set_node_pin`, `apply_node_pins`; branch inheritance/replay; `write_manifest`; PIXEL-02 transaction publication.
- `pixelprocess/tests/pipeline_test.cpp`: save/copy, ancestry, immutable revisions, stale updates, and node unpin/repin regression checks.

Validation: runner build, UI syntax check, targeted save/copy/preservation/unpin/stale-write/color/cleanup/stage/final checks. Full pipeline results are reported in the task response.

<!-- agent: GPT-6 | 2026-10-10 | Audit node pins saves and graph ownership | v 1.0.0 | c2ac54 -->
