<!--
File version: 1.0.0
Last updated: 2026-10-06
Purpose: Plan automatic grid detection improvements for separated sprite blobs.
Flow GRID-DETECT-01: canonical proposed detection flow; implementation owner is pixelprocess/stage_ownership.cpp, PIXEL-01 step 2.
Scope: ownership determines cells before extraction and assembly; downstream stages preserve row/column indices and empty slots.
-->
<!-- agent: gpt-6 | 2026-10-06 | Plan row local spritesheet grid detection | v 1.0.0 | 1318bb -->

# Spritesheet detection improvement plan

## Confirmed failure

Run `run_20261006_110113` contains 16 large sprite bodies arranged in four rows and four columns. Ownership metadata records 105 connected components, including detached decorations, but only 12 slots: three columns by four rows. Both grid settings were automatic (`0`).

`lane_edges()` in `pixelprocess/stage_ownership.cpp` requires a foreground-free strip spanning the entire image and at least two pixels wide. The middle column gap varies by row:

| Source row band | Empty middle gap, half-open x bounds |
| --- | --- |
| 0–272 | 489–527 |
| 272–516 | 511–526 |
| 516–767 | 489–539 |
| 767–1024 | 473–512 |

The common gap is `[511, 512)`, only one pixel wide. The detector drops this separator, yielding x edges `[0, 257, 770, 1024]`. Each middle cell then owns two large bodies. Extraction and assembly inherit the incorrect grid. No warning is emitted because the existing warning only covers a single inferred cell containing multiple components.

## Proposed implementation

Keep the change centered on ownership detection. Preserve explicit grid settings and downstream metadata compatibility. Do not use component count as sprite count: small detached sparkles belong with their nearby sprite.

Canonical proposed flow **GRID-DETECT-01**:

1. Ownership detector → collect foreground components and global gap candidates → initial row/column evidence. Explicit dimensions remain authoritative.
2. Ownership detector → project foreground within each reliable row band → local column gaps; also evaluate the transposed approach using reliable column bands when row evidence is weak.
3. Ownership detector → match ordered separator candidates across bands → a consistent grid count and separator confidence. Require repeated gap evidence and compatible body placement; do not force four columns or use a fixed component-area threshold. Preserve supported empty slots and avoid treating internal sprite holes as cell boundaries.
4. Ownership detector → assign complete components using the accepted local separators → stable logical row/column owners, retaining small decorations. Source separators may shift between bands; no component is cut merely to fit a shared straight boundary. Apply explicit ownership overrides after automatic assignment.
5. Ownership detector → publish grid evidence and validate occupied cells → normal completion when supported, or a warning when separated substantial bodies remain grouped or candidate layouts conflict. Extraction and assembly consume the stable indices as in PIXEL-01.

For step 3, score candidate layouts using support across bands, repeated body alignment, spacing consistency, and whether separators intersect substantial components. Use spacing as supporting evidence rather than assuming equal source cell widths. Retain the existing global-gap result when local evidence does not justify refinement.

For step 4, retain shared `x_edges` and `y_edges` for compatibility, and add optional local cell bounds or separator evidence for shifted boundaries. Document shared edges as nominal layout bounds if they no longer delimit every owner. Keep `source_bounds` as the union of owned components; review debug overlays so the displayed cells explain actual ownership.

For step 5, extend warnings beyond the 1×1 case. Multiple components alone are normal; warn when a cell contains substantial, spatially separated bodies supported as different sprites by neighboring bands. Record the inference method, supporting bands, and ambiguous cells in metadata.

## Implementation sequence

1. Add row-local gap inference and separator matching to ownership, referencing GRID-DETECT-01 steps 1–3 in function documentation.
2. Update component assignment, optional metadata, and debug overlays for GRID-DETECT-01 step 4. Confirm extraction uses component ownership and assembly keeps the inferred grid dimensions.
3. Add ambiguity reporting for GRID-DETECT-01 step 5 and perform the validation below before integrating the implementation into master.

Reuse projection scratch buffers between bands. Keep helpers small, avoid changes to background classification, and preserve all component records.

## Validation and acceptance

- Reprocess this run into separate artifacts: expect four rows, four columns, 16 occupied slots, one large body per slot, and retained decorations.
- Confirm shifted separators do not split or duplicate foreground components; all components have exactly one owner.
- Check aligned grids, one sprite with detached pieces, internal transparent gaps, empty slots, and sparse or conflicting band evidence.
- Check both axis orders, explicit grid dimensions, and ownership overrides.
- Confirm extraction previews and final assembly show the same logical grid and preserve empty slots.
- Ambiguous layouts must expose a warning and supporting evidence rather than silently report confident completion.

The exact matching tolerance and confidence threshold remain implementation choices to calibrate against these cases. The measured gap evidence establishes the failure and supports local inference; it does not establish a general sprite classifier.

## Temporary workaround

For this run, set `grid_cols=4` and `grid_rows=4`, then reprocess. This bypasses automatic lane inference. The settings and source artifacts have not been changed as part of this plan.

<!-- agent: gpt-6 | 2026-10-06 | Plan row local spritesheet grid detection | v 1.0.0 | 1318bb -->
