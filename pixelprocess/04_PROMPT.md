<!-- Version: 1.0.0 | Last updated: 2026-10-05 | Purpose: Pixel workbench and shell integration. -->
<!-- agent: GPT-6 | 2026-10-05 | Define pixel workbench shell integration | v 1.0.0 | deed31 -->
<!-- Flow UI-PIXEL-01 owner: this prompt until implementation assigns source owners. Scope: UI-PIXEL-01 captures edits and presents results; INFRA-01 owns execution/artifacts; PIXEL-01 owns algorithms and processing order. Shared invariants: immutable source, stable sprite IDs, explicit settings scope, and revision-matched evidence. -->
# Pixel workbench and shell integration

Update the aigen application shell to convert AI-generated or imported art into clean pixel art using `01_PROMPT.md`–`03_PROMPT.md`. Provide individual tweaking and debugging of every processing stage and sprite. Prefer a clean, complete workflow over accumulated legacy controls or unfinished drafts. Treat `04_METADATA_PLAN.md` only as general inspiration; do not adopt its schemas, architecture, stage matrix, or compatibility requirements.

## Canonical interaction flow UI-PIXEL-01

1. User → choose **Convert to pixel art** on a generated or imported image → workbench opens with an immutable source and explicit run settings. Generation may also hand its result directly to this workflow.
2. User → run conversion → INFRA-01 executes PIXEL-01; workbench displays progress, available artifacts, and stage states. Keep source and completed results available during processing.
3. User → select a stage and sheet or sprite scope → viewer and sidebar show matching inputs, output/debug views, effective settings, measurements, and warnings. Keep sprite selection stable across stages through recorded coordinate mappings; mark a selection unavailable when a stage has no matching sprite.
4. User → edit settings or ownership corrections → pending changes show their scope and affected downstream stages. **Apply** records the settings and reruns the affected stage and dependents, reusing valid artifacts. Retain the previous result for comparison. A sprite-only change still updates final sheet assembly; ownership/layout changes invalidate dependent sprite identities and require explicit reassociation of overrides.
5. User → inspect and refine the result → repeat steps 3–4, then export the rebuilt sheet or individual sprites. Persist settings, results, and processing state so reopening the workbench restores the session.

## Layout and controls

Use one workbench with an ordered stage list, central image viewer, sprite selector, and settings/details sidebar. Follow PIXEL-01 execution order with descriptive names and visible pending/running/completed/skipped/failed states. Keep all stages selectable even when an image is unavailable. Reuse existing zoom, pan, reset, and image-loading helpers where suitable.

Expose controls only for implemented behavior: background tolerance and cleanup mode; grid/cell settings and blob ownership corrections; extraction padding; automatic or explicit per-sprite pitch and X/Y phase; foreground coverage threshold; sheet anchor and padding. Sheet-wide stages use run scope; sprite stages support inherited defaults and explicit sprite overrides. Show effective values, pending edits, and reset-to-default actions. Saving profile defaults is a separate explicit action; workbench edits must not silently alter the global profile.

## Debugging and evidence

Provide stage input/output/debug selection, nearest-neighbor display, transparency checkerboard, and before/after comparison with synchronized source/logical coordinate mapping. Toggle background masks, component ownership, crop bounds, exact lattice lines, uncertain cells, removed pixels, and final placement with readable legends. Geometric overlays follow image zoom, pan, and clipping; detailed text stays in the sidebar. Avoid duplicating annotations already baked into debug images.

Click a sprite or cell to inspect its recorded measurements, candidate scores, effective thresholds, selected result, and explanation. Bind every view and annotation to the actual artifact revision and coordinate space. Consume the new versioned contracts from `02_PROMPT.md` directly; expose full metadata and logs without requiring raw JSON reading for ordinary use. Missing or malformed evidence must leave images usable and clearly identify unavailable details.

## Shell changes and completion

Replace legacy Python discovery, filename-based stage semantics, hardcoded numeric stage switches, and profile-cycling tweak actions with the new runner and explicit stage records. Remove obsolete processing options. Parse and cache metadata outside drawing; load images asynchronously, reuse unchanged textures, and keep interaction responsive.

Support cancellation, actionable stage failures, retained partial outputs, stale-result indicators, and recovery after restart. Keep historical runs viewable as images/logs without an extensive compatibility layer. Finish the complete convert → inspect → tweak → apply → export workflow before adding optional features; move canonical playbooks to owning source headers as implementation develops.
<!-- agent: GPT-6 | 2026-10-05 | Define pixel workbench shell integration | v 1.0.0 | deed31 -->
