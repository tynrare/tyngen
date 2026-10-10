<!-- Version: 1.1.0 | Last updated: 2026-10-10 | Purpose: Audit pin, manual save, branching, and revision behavior. -->
<!-- agent: GPT-6 | 2026-10-10 | Audit node pins saves and graph ownership | v 1.1.0 | c2ac54 -->
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
| Sprite selections / drawing | Selection recipe always pinned, refreshed by stable sprite IDs; missing IDs remain partial/unavailable. Draw edits are durable derived images. | Rerun prompts warn when temporary drafts or unpinned draw copies may become outdated; temporary draw and sprite export drafts are discarded only after confirmation. Selections resolve from the committed graph snapshot. |

Branch = stable fork identity, ancestry, fork stage, explicit settings overrides and owned stages. Node = branch/step identity with artifacts, cache signature, consumed inputs/settings. Revision = immutable operation snapshot; reruns replace current outputs under stable node IDs. History is not a branch.

Save as keeps the primary settings pointer with the primary graph, preventing a child draft becoming the primary baseline. Alternate generation is skipped during copy creation so the selected copy cannot accidentally become the last generated alternate.

## Remaining weak spots and refinements

- Stage ownership and cache-consumed settings now come from a shared policy table. Keep precedence for shared keys explicit and review table updates with each stage change.
- `Branch.pinned` remains an internal retention flag backed by aggregate snapshots. Derive retention directly from node pins and consumer ancestry; expose only node pin state to users.
- JSON settings merging scans text rather than parsing top-level properties. Optional missing keys now insert correctly; replace the scanner with structured JSON merging.
- Derived drawing/selection nodes use separate persistence and selection nodes cannot unpin through this toggle. Unify their lifecycle with node metadata before offering the same toggle for these nodes.
- Full and scoped replay validate ancestry and order ancestors before descendants, with ID tie-breaking. Required unpinned providers replay from their saved settings.
- Selection images and recipes are part of the revision snapshot and resolve through the committed graph pointer. Post-commit stale-file pruning reports a maintenance warning without misreporting a committed revision as failed.
- Cached/replayed processing nodes are preserved by settings, not frozen image bytes; input changes can change a pinned node's output.

## Contact points

- `lib.c`: `grid_edit_save`, `request_branch_rerun`, `node_pin_path`, `toggle_node_pin`, viewer Pin button; Settings drafts and confirmations.
- `pixelprocess/runner.cpp`: NODE-PIN-01; `stage_setting_overrides`, `set_node_pin`, `apply_node_pins`; branch inheritance/replay; `write_manifest`; PIXEL-02 transaction publication.
- `pixelprocess/tests/pipeline_test.cpp`: save/copy, ancestry, immutable revisions, stale updates, and node unpin/repin regression checks.

Rerun confirmation blocks while a draw save is in progress. Saved draw copies remain durable and are marked as potentially outdated when reruns affect them.

<!-- agent: GPT-6 | 2026-10-10 | Audit node pins saves and graph ownership | v 1.0.0 | c2ac54 -->
