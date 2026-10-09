<!-- File version: 1.0.0 | Updated: 2026-10-09 | Purpose: Record background probe review, regressions, and validation limits. -->
<!-- agent: gpt-6 | 2026-10-09 | Record probe validation and confirmed fixes | v 1.0.0 | 59c766 -->

Validated against commit `77c9b08` plus the probe fixes, using an isolated source snapshot while separator and timing changes were being edited concurrently.

| Check | Result and fix |
| --- | --- |
| Exterior probes and enclosed holes | Automatic probes now select exterior-connected regions; enclosed holes remain until manually probed. |
| Transparent source with dark opaque sprite | Preserved. Existing transparency disables inferred mass/border color removal. |
| Manual add/remove and suppression | Region fill, automatic suppression, and saved settings round trips pass. |
| Probe cache invalidation | Moving a probe with the same ID invalidates background cache; repeating unchanged settings reuses it. Map fields were previously absent from fingerprints. |
| Settings and overlay parsing | Empty arrays, whitespace, long region records, and following collections pass. Parsers are bounded and ownership corrections reload. |
| Apply and successful publication | Existing settings and ownership edits survive Apply; publication reloads resolved blob IDs and clears the draft. |
| Ownership relabel, repeated rerun, split, merge | Unique pixel overlap transfers corrections; split/merge matches drop them. Resolved IDs replace saved overrides. |
| Empty quantized sheet | A valid fully transparent result publishes successfully when reconstruction excludes every sprite. |

C++ regression suites: `pixelprocess-fringe`, `pixelprocess-probes`, and `pixelprocess-pipeline` all passed. UI action/persistence checks passed via `bash tests/run_probe_tests.sh /tmp/aigen-probe-review-build/runner`. C compilation passed with `-Wall -Wextra`; no window was opened for the UI checks.

The full `run_20261009_075809` pipeline completed in a temporary run directory: 4×4 grid, 16 probes, 757,139 background pixels (72.2064%), and 16 exported sprites. Background and ownership completed without warnings. Reconstruction reported `cells_below_foreground_coverage_threshold`.

Example artifacts: `/tmp/probe-example-full-doj1mz1r/run/`, with execution output in `/tmp/probe-example-full-doj1mz1r/pipeline.log`. The source run was read without modifying its published artifacts.

Interactive mouse rendering and the concurrent separator changes were not covered by this isolated validation. Draft dots update immediately; the background mask changes after Apply.
<!-- agent: gpt-6 | 2026-10-09 | Record probe validation and confirmed fixes | v 1.0.0 | 59c766 -->
