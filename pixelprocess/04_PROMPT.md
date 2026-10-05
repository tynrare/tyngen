<!-- Version: 1.1.0 | Last updated: 2026-10-05 | Purpose: Adapt existing shell controls to pixelprocess. -->
<!-- agent: GPT-6 | 2026-10-05 | Adapt existing shell processing controls | v 1.1.0 | 172e06 -->
<!-- Flow UI-PIXEL-01 owner: this prompt until implementation assigns source owners. Scope: UI-PIXEL-01 captures edits and presents results; INFRA-01 owns execution/artifacts; PIXEL-01 owns algorithms and processing order. Shared invariants: immutable source, stable sprite IDs, explicit settings scope, and revision-matched evidence. -->
# Existing shell integration

Update the aigen application shell to convert AI-generated or imported art into clean pixel art using `01_PROMPT.md`–`03_PROMPT.md`. Preserve the existing controls, their shapes, layout, and interaction conventions; adapt their usage to the new processor. Provide individual tweaking and debugging of every processing stage and sprite through the current application. Treat `04_METADATA_PLAN.md` only as general inspiration, without adopting its schemas, architecture, stage matrix, or compatibility requirements.

## Integration approach

First inspect the current UI and map each relevant control to its new purpose: Library/Grid navigation, generation flow, artifact thumbnails/dot navigation, image viewer, Controls tabs, inspector, profile settings, rerun actions, and export/open-image actions. Preserve useful functionality and presentation. Add small controls only where stage or sprite editing has a demonstrated gap; avoid introducing a separate workbench or redesign. Simplify obsolete processor behavior behind existing controls rather than replacing the controls wholesale.

## Canonical interaction flow UI-PIXEL-01

1. User → generate art or select an imported image → existing generation/details or image controls offer pixel conversion with an immutable source and explicit run settings. Preserve generation-to-processing handoff and add an imported-image action where needed.
2. User → start conversion → INFRA-01 executes PIXEL-01; existing progress, log, artifact, and cancellation controls display the run. Keep source and completed results available during processing.
3. User → select a stage through existing artifact navigation and a sprite through the inspector → image viewer and Controls show matching artifacts, effective settings, measurements, and warnings. Preserve sprite selection across stages using recorded mappings; indicate when no matching sprite exists.
4. User → edit stage settings or ownership corrections in Controls → pending edits show scope and affected downstream stages. **Apply** records settings and reruns that stage and its dependents, reusing valid artifacts and retaining the previous result for comparison. Sprite changes also update sheet assembly; ownership/layout changes invalidate affected sprite identities and require reassociation of their overrides.
5. User → inspect, refine, and export → repeat steps 3–4, then save the rebuilt sheet or individual sprites through existing output actions, extending them as needed. Persist settings, results, and processing state for reopening the run.

## Stage and sprite controls

Populate artifact navigation and Controls from explicit stage records in PIXEL-01 order, with descriptive names and pending/running/completed/skipped/failed states. Main and debug images belong to the same stage. Keep stages inspectable in Controls when no image exists. Preserve Process log, Gen record, Resources, and Metadata purposes; integrate editable processing settings into the existing Controls area.

The selected stage determines available settings; sheet or selected-sprite scope determines their target. Expose implemented background tolerance/cleanup mode, grid/cell settings and blob ownership corrections, extraction padding, automatic or explicit pitch and X/Y phase, foreground coverage threshold, and sheet anchor/padding. Sheet stages use run scope; sprite stages support inherited defaults and explicit overrides. Show effective values, pending edits, and reset actions. Replace profile-cycling tweak behavior with actual editable values and **Apply**. Keep the existing profile editor for defaults; saving defaults is explicit and separate from editing a run.

## Visual debugging and evidence

Extend the current viewer and inspector with stage input/output/debug selection and before/after comparison using recorded coordinate mappings. Preserve zoom, pan, reset, nearest-neighbor display, and existing image tools. Add toggleable background masks, ownership, crop bounds, lattice, uncertainty, removed pixels, and placement with readable legends. Overlays follow image transforms and clipping; explanations use existing details areas without obscuring artwork or intercepting unrelated controls. Avoid duplicating baked debug annotations.

Click sprites or cells to inspect recorded measurements, candidates, thresholds, selected results, and reasons. Bind images, settings, and evidence to the displayed revision and coordinate space. Consume `02_PROMPT.md` contracts directly; keep full metadata and logs reachable. Missing evidence must leave images usable and identify unavailable details.

## Processor wiring and completion

Replace legacy Python discovery, filename-derived stage meaning, numeric stage switches, and obsolete options with the new runner and records. Reuse existing background loading, logging, cancellation, and persistence; parse metadata outside drawing, cache records, and reuse unchanged textures. Support actionable failures, partial outputs, stale-result indicators, and restart recovery. Keep old runs viewable as images/logs without extensive compatibility machinery. Complete conversion, stage/sprite editing, debugging, reruns, and export within the existing shell; move canonical playbooks to owning source headers as implementation develops.
<!-- agent: GPT-6 | 2026-10-05 | Adapt existing shell processing controls | v 1.1.0 | 172e06 -->
