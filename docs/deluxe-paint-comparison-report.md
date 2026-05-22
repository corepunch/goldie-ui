# Report: Deluxe Paint Reconstruction References vs. Orion Image Editor

## Scope

This report compares classic/retro paint-editor codebases (with emphasis on **GrafX2** and Deluxe Paint-style reconstructions) against Orion’s current image editor structure, and evaluates whether Orion’s `canvas_doc_t` is unusually heavy.

## 1) Reality check on “Deluxe Paint source/reconstruction”

- Original commercial Deluxe Paint source was not officially open-sourced.
- In practice, **GrafX2** is the most relevant open implementation of the Deluxe Paint model (indexed palette workflow, classic tool behavior, legacy format handling).
- So for architectural comparison, GrafX2 is the best “Deluxe Paint reconstruction” reference.

## 2) What those projects do with core image/canvas state

### GrafX2 (closest reference)

- Uses a central document structure (`T_Document`) plus extensive global state patterns.
- Keeps substantial state in/around the document model (palette, viewport-related state, layers/visibility, undo linkage).
- Also uses modular boundaries in key areas (notably file format I/O through context-based APIs).

**Takeaway:** even the closest Deluxe Paint-style clone uses a fairly large central document state, but succeeds when subsystems have clear APIs.

### mtPaint

- Lightweight C architecture, but with globally shared editor/image state conventions.
- Pragmatic and compact, but less isolated/test-friendly in many paths.

**Takeaway:** “small C codebase” does not automatically mean strongly modular/testable boundaries.

### GIMP / ImageMagick (historical contrast)

- Both carry large internal image structures.
- Their scalability comes from stronger module boundaries and data-access APIs, not from tiny structs.

## 3) Comparison to Orion (`examples/imageeditor`)

Orion already has meaningful modular decomposition:

- `canvas/` split into focused files (`canvas_pixels.c`, `canvas_selection.c`, `canvas_layers.c`, etc.)
- `commands/` command modules
- `tools/` handler registry and per-tool modules
- `anim/` separate animation module

Relevant local structures:

- `canvas_doc_t` in `examples/imageeditor/imageeditor.h` (large, nested, multi-concern state)
- `app_state_t` in same header (application/UI orchestration state)

## 4) Is Orion’s canvas struct “too heavy”?

Short answer: **it is heavy, but not abnormal for this class of editor**.

The bigger issue is not raw field count; it is **concern mixing**:

- persistent document data mixed with transient UI/runtime concerns
- GL/render-adjacent data and interaction session state close to core model

So the best optimization target is **boundary quality**, not “smallest possible struct.”

## 5) Recommended direction (module-first, testable API)

Your target model (“modules expose testable APIs; main app communicates between modules”) is the right direction.

### A. Make a strict core model boundary

- Keep a pure document/core API layer that has no UI/window dependencies.
- Keep rendering/window/event concerns in view/controller modules.

### B. Use context-driven module APIs

Adopt explicit context parameter patterns for subsystems (I/O, draw operations, transforms), so module functions depend on passed-in data rather than globals.

### C. Move transient runtime state out of persistent doc where possible

- Keep persistent image/layer/selection data in core model.
- Move preview/session-only states to window/controller-local state.

### D. Keep main app as orchestrator

- Main/controller layer coordinates:
  - command dispatch
  - document lifecycle
  - module calls
  - UI invalidation/update decisions
- Core modules should not directly drive UI.

### E. Testability rule

- Core module headers should be includable in headless tests.
- Tests should call production module functions directly (no duplicated logic copies).

## 6) Suggested phased implementation for Orion

1. Define/strengthen a pure “core” header/API surface for document + canvas operations.
2. Audit and remove implicit dependencies from core modules to UI/global app state.
3. Isolate transient preview/tool-session state into view/controller state objects.
4. Standardize subsystem context structs for I/O and draw operations.
5. Expand headless tests against production module APIs.

## 7) Direct answer to your design question

Yes—GrafX2 and similar editors also carry substantial central state; this is common.  
What differentiates maintainable systems is **modular API boundaries** and **testable separation**, not minimal struct size alone.

---

## Local references used

- `examples/imageeditor/imageeditor.h`
- `examples/imageeditor/canvas/*.c`
- `examples/imageeditor/commands/*.c`
- `examples/imageeditor/tools/*.c`
- `examples/imageeditor/anim/*.c`
- `docs/imageeditor-mvc.md`
