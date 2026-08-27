# Canonical room source brief

Use this exact section order.

## Identity

- Source format and project
- Room ID and display name
- One-paragraph canonical description
- Source roots and extraction date

## Exits and adjacency

List direction, destination, condition, and presentation consequence.
Separate outgoing routes from incoming routes. A route that is gated in only
one direction must remain asymmetric in the brief.

## Canonical inventory

Use a table with:

| ID | Name | Parent/location | Visibility | Interactions | Required states | Scale/relationship facts | Sources |
|---|---|---|---|---|---|---|---|

`Visibility` distinguishes room-description landmarks, discoverable objects,
containers, scenery, and conditional objects. `Required states` names every
visually distinct state needed by the adaptation.

After the table, list `Prose-only scenery` and `Documented intent without an
executable object` separately. Neither category is an interactive canonical
object unless the adaptation deliberately promotes it and records that choice.

## Story beats to support

List actions or revelations that require readable staging. Include the actor,
target, precondition, result, and any object relocation.

## Global visual facts

Record only sourced spatial, scale, material, time, weather, atmosphere, or
lighting facts. Preserve comparative language when exact dimensions do not
exist.

## State matrix

Use one row per adaptation state:

| State | Trigger | Visible changes | Object locations | Exit changes | Sources |
|---|---|---|---|---|---|

Include the initial state and every state that changes room geometry or a
story-critical prop.

## Conflicts and unknowns

Record source disagreements, underspecified placement, and facts that require
an art-direction decision. Never hide uncertainty inside declarative prose.

## Source coverage

List every ZIL and Markdown file inspected, plus relevant line ranges. End with
a checklist confirming room form, direct and nested objects, incoming and
outgoing exits, action routines, dynamic descriptions, object relocation,
companion/story scenes, prose, map, puzzle notes, intended-object registries,
QA/tests, and state docs were checked.
