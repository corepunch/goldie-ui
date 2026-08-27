---
name: extract-zil-room-brief
description: Extract a canonical, source-cited room brief from a ZIL adventure and its supporting Markdown. Use when a scene, illustration, map, adaptation, or environment design must be grounded in ZIL room definitions, objects, exits, containment, interactions, puzzle states, prose, or story documentation before visual interpretation begins.
---

# Extract ZIL Room Brief

Produce source facts, not visual invention. Keep canon separate from later art direction.

## Workflow

1. Read [references/source-brief-schema.md](references/source-brief-schema.md).
2. Locate the room form, all object forms whose containment resolves to that room, and both outgoing and incoming connected exits.
3. Run `scripts/extract_zil_room.py ADVENTURE_ROOT ROOM-ID` for a deterministic first pass.
4. Inspect every cited ZIL form and the relevant room sections in prose, object registry, map, puzzle, story-state, transcript, design, synopsis, QA, and test Markdown. Prefer current project-owned specifications over stale audits, and record provenance when a secondary document conflicts.
5. Search action and companion routines for each canonical object and room ID. Record state changes, conditional visibility, movement, access, and puzzle dependencies.
6. Resolve containment recursively. An object inside a container or surface in the room still belongs to the room inventory, but preserve its parent relationship.
7. Record contradictions instead of silently choosing a source. Treat executable ZIL as behavioral authority; use explicit project documentation for intended presentation, and label any divergence.
8. Classify descriptive details that lack executable object forms as `prose-only scenery`; include them as sourced visual facts without presenting them as interactive canonical objects.
9. Treat the script output as an evidence index, not a finished brief. Inspect dynamic descriptions, action handlers, companion routines, object relocation, scoring flags, and conditional exits manually.
10. Write the source brief using the schema. Cite every nontrivial fact with `path:line`.
11. Audit the brief against the room description, direct and nested object forms, incoming and outgoing exits, and every state-changing routine before handing it off.

## Boundaries

- Include only facts supported by ZIL or authoritative project Markdown.
- Do not add plausible furniture, architecture, clutter, palette, lighting, or camera choices.
- Do not turn decorative reference-image content into canon.
- Distinguish always-present objects from conditional states and temporarily relocated objects.
- Distinguish executable objects, prose-only scenery, and documented-but-unimplemented intended objects.
- Preserve scale language such as `tiny`, `enormous`, `above`, `beneath`, and `near the edge`; these become constraints for later design.
- Report missing or ambiguous facts explicitly.

## Completion standard

Deliver a brief only when it includes room identity, description, bidirectional adjacency, canonical inventory, containment, prose-only scenery, interactions, required visual states, story beats, scale facts, unresolved conflicts, and citations. The brief is the factual input to room art direction; it is not an implementation plan.
