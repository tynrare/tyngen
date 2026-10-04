<!--
File version: 1.0.0
Last updated: 2026-10-04
Purpose: Inspect processor artifacts and plan complete decision metadata and image annotations.
Flow ID META-01 (proposed; implementation owner will be processor header)
1. App → capture effective run inputs and start processor → immutable provenance links generation to processing.
2. Processor → record operation and decision events before publishing artifacts → durable journal explains every stage and skipped branch.
3. Processor → atomically publish final or partial manifest → indexed summary references authoritative journal and available artifacts.
4. App worker → load events, manifest and legacy evidence → cached stage metadata drives Controls and image overlays.
5. User → select/hover stage, zoom or inspect a cell → matching annotations expose what, when, how and why.
Scope index: META-01 proposed processor owner; app integration references APP-04 and APP-08 in lib.c. Journal sequence defines chronology; numeric stage order defines presentation only. Failure preserves valid event prefix; unknown legacy facts remain unknown.
-->
<!-- agent: gpt-6 | 2026-10-04 | Plan processor decision metadata and annotations | v 1.0.0 | b6183c -->
# Processor metadata and image annotations plan

Scope: inspection and implementation plan only. No processor or application behavior changed.

## Inspection findings

- Installed entry chain: `/home/x/.local/bin/process-ai-pixel` → `/home/x/bin/imagegen/process-ai-pixel` → `process_ai_pixel.sh` → `.venv/bin/python process_ai_pixel_2.py`.
- Application `start_pixel` in `lib.c` invokes the Python processor directly with an immutable profile snapshot, separate output directory, and `--emit all`.
- Current `ProcessResult` records source, pixel plan, layout plan, stage paths, variants, sprite pitches, and manifest path. It lacks schema/processor versions, stage timing, explicit stage states, decision history, reasons for every branch, overlay geometry, and legends. Manifest is written at successful completion, so failures can leave images and logs without a manifest.
- Processor prints useful decisions: layout fallbacks, alignment acceptance/decline, pitch candidates, per-sprite phase/shift/confidence, ownership, alpha counts, magenta support, boundary recovery, and output choices. Some branches are silent (for example alignment off). Human-readable prints are not a complete durable decision contract.
- `scan_artifacts` discovers image files by directory and filename; it does not parse manifests. Controls has Process log, Gen record, and Resources tabs, but no processor metadata tab. `stage_description` provides generic explanations below the Controls preview. The generation image viewer does not display stage-specific metadata atop its image.
- `add_log` persists receive-time local HH:MM:SS plus process/stream, rather than full producer timestamps or stage duration. Visible history is bounded. Long-line handling and historical loading must be reviewed before claiming complete logs.
- Actual files inspected: `/home/x/Pictures/aigen/extra/gen_20261003_075759.pixel.35x37.manifest.json` and `/home/x/Pictures/aigen/dump/validation/gen_20261003_054814/{run.log,extra/}`. The first has pitch 6×6, confidence ≈0.096, alternatives and output variants, but an empty stage map. The second has 16 stages and older `sprite_pixels` metadata. Its images use older stage 30 `sprite-scale-adapt` and stage 41 `primary-output` names. These are historical evidence, not proof that every current branch produces the same artifacts.
- Visually inspected all 16 historical stage images in a contact sheet: source, grids, phase views, ownership rectangles, cleanup masks, reconstruction, and primary output. Grid/phase images need legends to distinguish meanings; cleanup masks need labels explaining classifications. Stage 35 resembles layout and must explicitly say diagnostics only / fitting disabled.
- No new generation or processor execution performed. Current weighted-alpha, accepted alignment, and boundary recovery images were not visually inspected in this sample; review those fixtures during implementation.

## Data contract

Improve the existing manifest and add a streaming decision journal. Do not build another unrelated run record.

| Artifact | Responsibility |
| --- | --- |
| `metadata.txt`, `generation.record.txt` | Existing generation inputs and provenance; retain as separate records linked by run ID. |
| `process.log` | Complete original stdout/stderr, including warnings and errors. |
| `extra/processing.events.jsonl` | Append-only authoritative lifecycle, decision, warning, artifact, and failure events, available while running. |
| Existing `*.manifest.json` | Versioned final snapshot referencing journal, stage records, artifacts, and decisions; publish atomically. |

Event fields: schema version, run/process/source ID, increasing sequence, UTC timestamp, monotonic elapsed time, event kind, operation ID, related stage IDs, severity, and optional sprite/blob/variant ID. Stage IDs remain stable; event sequence records execution order independently of the UI's numeric grouping.

Each decision records **what** action was considered and selected; **when** it occurred; **how** method, effective parameters, measured values, units, candidates/scores, and threshold; **why** a reason code and readable explanation. Include rejected candidates, fallbacks, user-forced choices, disabled/skipped branches, diagnostic-only operations, and failures. Shared decisions explicitly reference every dependent stage. Do not attach a line to a stage merely because it preceded that image write.

Separate computation operations from diagnostic image emission: stages 33/32/37/39 may share reconstruction work. Record operation start/end and artifact emission time; do not invent independent computation durations for each image.

Stage records: stable ID/name, operation references, input/output artifact IDs, state (pending/running/completed/skipped/failed), start/end/elapsed, dimensions, coordinate space, decision/warning references, purpose, skip/failure reason, and annotation descriptions. Image availability is independent from operation success and run exit status.

Annotation records: semantic kind, geometry or mask reference, coordinate space and image transform, units, color/legend meaning, related decision IDs, confidence, and sprite/blob/cell identity. Preserve source-to-aligned-to-logical-to-repacked transforms. Record actual intermediate layout/lattice state rather than using final layout for earlier images.

Run provenance: effective CLI/options including defaults, processor version and source hash, source hash/dimensions, software environment, parent rerun ID, output role/mode, tile size, pitch, confidence, and member cells. Never record API tokens or secret environment values.

Journal writes must be ordered and flushed at operation boundaries, on decisions, and on failure. Handle recording failure explicitly; surface it and stop processing if authoritative recording cannot continue. Finalize partial failure state when possible; abrupt termination retains the valid journal prefix. Ignore an incomplete trailing JSONL line until completed. Journal is authoritative; manifest is its summary, not a second independently authored account.

## Stage annotation coverage

All available source/stage/final views show a compact translucent card atop the image: identity and purpose; time/duration; method and effective parameters; selected result and reason; confidence/warnings; legend. Expand for full decisions and per-object details. Unknown historical fields say “not recorded”.

| Stage | Meaningful annotations and detailed metadata |
| --- | --- |
| Source / 40 | Input identity, dimensions, hash, capture/load time, generation or rerun provenance. |
| 36 layout | Cell boundaries and row/column IDs; detected method, requested grid, confidence, regularity, fallback candidates/reasons. |
| 35 blob-grid-fit | Diagnostics-only label; fitting disabled reason; inherited layout boundaries and legend. |
| 34 align-blobs | Original/target bounds and movement arrows; moved/skipped IDs/counts; acceptance threshold, confidence, reason, clear color. |
| 33 pixel-lattice | Pitch, phase, lattice lines; forced/automatic choice, candidate scores, weak-evidence warning. |
| 32 sprite-grid-fit | Nominal/fitted grid; cell IDs, target dimensions, weak cells, fitting method and dependencies. |
| 37 lattice-phase-fit | Per-cell nominal/fitted phase, confidence on each axis, local/shared fallback reason and grid legend. |
| 39 lattice-phase-shift | Per-cell translation arrows and before/after phase; shift units, method, diagnostic versus applied status. |
| 31 sprite-reconstruct | Logical/source dimensions and expansion mapping; ownership/spill preservation and per-sprite reconstruction decisions. |
| 30 pixel-pitch | Shared pitch, measured sprite pitches/confidence and occupied cells; variant grouping and choice rationale where applicable. |
| 28 alpha candidates | Candidate mask and eligibility criteria; counts, weighted mode and neighborhood/threshold definitions. |
| 27 alpha support | Support mask/scale legend; source support values, thresholds and per-pixel inspection. |
| 26 alpha edits | Before/after alpha and edited/retained masks; counts, edit reasons and magenta-skip requirement. |
| 29 uniform-grid | Logical grid and source mapping; regularization method, dimensions and reconstruction/alpha dependencies. |
| 25 magenta outside mask | Removed versus preserved classification; flood/ownership rules, source counts and explicit legend. |
| 24 magenta source support | Source-to-logical support mapping; support values, mask domain and protected regions. |
| 22 magenta support clean | Removed/retained colors, bounded support rule and before/after counts. |
| 20 magenta clean | Effective outside/all mode, removed/preserved counts and protection reasons; skip is visible in metadata even without an image. |
| 38 blob ownership | Component IDs, owner cells, bounds, spill handling and ownership assignment/fallback reasons. |
| 09 boundary recovery | Recovered/unresolved locations, counts, source band width and unique-ownership rule. |
| 10 rearrange bounds | Source/crop/target rectangles, trim margins, padding and sprite placement mapping. |
| Final variants / legacy 41 | Primary/forced/pitch role, dimensions, tile size, pitch, member cells, render/resample mode, choices and warnings. |

Dense per-cell/per-pixel data appears through zoom-aware labels and pointer inspection. Provide overlay layer toggles and expanded details so all meaningful data is accessible without covering the artwork. Existing PNGs already bake some geometry into pixels: describe it with legends and add only missing layers; avoid duplicate grids. Never overwrite source or diagnostic PNGs with UI annotations.

## Application design

Add **Metadata** as the fourth Controls tab. Show Summary, stage timeline, Decisions, Warnings, Outputs, and raw manifest/journal within that tab; support scrolling, stage/severity filtering, search, copy, and open-file actions. Selection synchronizes with the image viewer; selecting a skipped stage shows its record without pretending an image exists.

Resolve metadata for the artifact actually displayed, including temporary hover previews and comparison panes. Hover leave restores selected-artifact metadata with the image. Anchor the explanation card to the image frame; geometric labels follow the same image transform and clipping as zoom/pan. Keep dot rail, footer, and painting input unobstructed; overlay controls must not trigger image painting.

Load and parse metadata with existing background work patterns; cache by run/file identity and revision. Allocate on load, reuse scratch and annotation buffers during drawing, and cull labels outside the view. Do not read/parse JSON on every draw. Support large journals with an index and bounded visible cache, keeping the full journal on disk.

Legacy adapter: accept old/current manifest filenames and fields (`sprite_pixels` / `sprite_pitches`), use manifest roles before filename guesses, and preserve unknown stages. Resolve relative paths against the recorded root with safe run-local fallback. Label legacy log-derived fields as such; retain raw evidence. Filesystem mtime is artifact modification time, never decision time. Missing manifest and malformed metadata leave images and raw logs usable.

Cleaning stages preserves metadata and decision history, marks image files unavailable, and prevents dangling file actions. Reruns retain parent linkage and create new artifacts/metadata. Expose stage metadata and journal filters through existing run inspection/MCP actions after the same UI model is implemented.

## Delivery order and review criteria

1. Audit current processor branches and drawing helpers; map every meaningful processing choice to a stable operation/reason code, stage dependencies, and annotation definition. Record current fixtures and missing branch coverage.
2. Add the shared processor event recorder, then instrument layout, alignment, lattice/reconstruction, alpha, cleanup, rearrangement, and variant packing. Keep human logs and versioned manifest serialization fed by the same decisions.
3. Add app parsing/cache and legacy adapter in `lib.c` / `lib.h`; classify actual artifacts using manifest roles, retain image scanning fallback, and extend run inspection.
4. Add the Metadata Controls tab, then overlay summary/legends and geometry, cell/pixel inspection, layer controls, and hover/compare synchronization.
5. When implementing, verify automatic/forced pitch/layout, alignment accepted/declined/off, legacy/weighted alpha, all/outside/skip magenta, both rearrangement ownership paths, pitch groups/forced tiles, skipped branches, failures/cancellation, incomplete journals, legacy/missing manifests, cleaned artifacts, and restart recovery. Check annotation coordinates under zoom/pan and ensure each decision is linked to its evidence and dependent stages.

Completion means every semantic processing branch has a recorded result/reason; every available artifact has its own evidence-backed what/when/how/why display; skipped stages remain inspectable in Metadata; logs and journal survive UI clearing/restart; full details remain reachable without obscuring images. Exact historical timing and unrecorded reasons cannot be recovered retroactively.

Processor changes belong to `/home/x/bin/imagegen`, outside this repository; application changes belong here. Coordinate the contract across both before UI implementation. This plan records decisions for that future work and does not claim the external processor has been modified.

<!-- agent: gpt-6 | 2026-10-04 | Plan processor decision metadata and annotations | v 1.0.0 | b6183c -->
