<!-- Version: 1.1.0 | Last updated: 2026-10-05 | Purpose: Pixelprocess implementation infrastructure. -->
<!-- agent: GPT-6 | 2026-10-05 | Clarify runner state and artifact contracts | v 1.1.0 | 1174ee -->
# Pixelprocess implementation infrastructure

Implement the infrastructure described in `01_PROMPT.md` to replace `process-ai-pixel` in the aigen UI shell. Keep changes small, dependencies minimal, and UI wiring limited to supporting the new processing units. Pixel processing algorithms belong in `03_PROMPT.md`; do not implement them here.

## Build and layout

- Give each stage its own source file and executable. Share only common CLI, image I/O, metadata, and error-handling helpers.
- Use C where practical. Identify the smallest C++ boundary required by the chosen OpenCV version and keep it isolated. Document the build command and required dependencies.
- Establish a runnable pass-through stage to exercise the infrastructure before adding algorithms.

## Stage contract

- Define one consistent CLI for image inputs, text inputs, output locations, stage settings, and optional debug artifacts. Treat piping as executable composition through artifact paths; specify any stream transport explicitly.
- The first image is the main processing unit; the second, when emitted, is the debug view. Give additional artifacts explicit roles and stable identifiers.
- Define exit codes, readable errors, and input validation. Keep machine-readable output separate from diagnostic logs.
- Use a versioned text format for detailed metadata and mutable run state. Specify required fields, artifact references, sprite identifiers, and settings. The runner alone owns shared run state; stages emit result metadata for it to merge. Isolate sprite state and outputs. Define schema versions, units, coordinate spaces, and source/crop/logical/placement transforms; use half-open bounds and explicit pixel-boundary coordinates.

## Execution and artifacts

Infrastructure flow INFRA-01 (owner: this prompt; replace with a reference to the owning implementation header when implemented):

1. UI → submit inputs and run/stage/sprite settings → runner creates an isolated run directory and manifest.
2. Runner → invoke configured stage executables in dependency order → validated artifacts and metadata become inputs to later stages.
3. Runner → schedule sprite work after sprite identities are available → each sprite receives isolated settings, state, and outputs.
4. Runner → collect results and status → UI displays outputs, debug artifacts, and actionable failures.

Define cancellation and failure behavior, including partial-result retention. Support rerunning an individual sprite or stage, invalidate dependent outputs, and reuse artifacts only when input content, upstream artifact identity, effective settings, and executable version match. Publish completed artifacts atomically; never expose incomplete output as successful. Record executable versions, effective settings, and enough invocation data to reproduce a run. Publish best-effort results with structured uncertainty flags and reasons; distinguish completed-with-warnings from failed or unavailable output.

## UI integration and completion

- Inspect the existing shell integration, replace its dependency on `process-ai-pixel`, and avoid preserving legacy APIs solely for compatibility.
- Provide progress, errors, stage/sprite settings, debug-image selection, and a documented overlay-data contract. Defer algorithm-specific controls and overlay generation to `03_PROMPT.md`.
- Deliver build instructions, stage and metadata contracts, the pass-through executable, runner wiring, and minimal UI integration. Keep canonical execution playbooks in their owning source headers as implementation develops.
<!-- agent: GPT-6 | 2026-10-05 | Clarify runner state and artifact contracts | v 1.1.0 | 1174ee -->
