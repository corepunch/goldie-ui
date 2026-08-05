---
name: populate-simplegl-scenes
description: Create, populate, edit, compose, or validate SimpleGL XML scenes and prefab XML files. Use for room layout, walls and openings, window or door inserts, furniture placement, materials, lights, cameras, groups, primitive selection, transforms, prefab authoring, and diagnosing misplaced, floating, intersecting, incorrectly oriented, or incorrectly scaled scene objects.
---

# Populate SimpleGL XML Scenes

Build scenes in stable local coordinate frames and verify them with CLI checks and tests.

## Read the relevant references

- Read [references/scene-format.md](references/scene-format.md) for supported XML tags, attributes, defaults, rotations, modifiers, and prefabs.
- Read [references/layout-and-validation.md](references/layout-and-validation.md) whenever placing walls, openings, inserts, furniture, cameras, lights, or prefabs.
- Read [references/shot-composition-guide.md](references/shot-composition-guide.md) whenever placing or revising cameras, and use it to define each shot's story purpose, framing, continuity, negative space, and field of view.

## Workflow

1. Inspect the target scene, referenced prefabs, and existing materials before editing.
2. Establish the room coordinate system, floor height, wall centers, and local axes.
3. Create the structural shell first: floor, walls, ceiling or roof, openings, cameras, and lights. An interior room is enclosed unless the design explicitly calls for an open or roofless space.
4. Place related geometry in a shared `<group>` coordinate frame. Never duplicate a rotated parent's world-space transform by hand when a group can express it.
5. Populate large furniture before small decoration. Reuse a prefab when an object appears more than once or has a natural front direction.
6. Keep all naturally grounded objects at the documented prefab baseline. Calculate primitive centers from half-height; do not guess vertical positions.
7. Validate XML, build the project, and run the tests.
8. Load the scene through the CLI and check its declared cameras.
9. For every interior room, place at least one motivated practical or window light that casts readable shadows. Match visible lamp geometry to its light position, establish a clear key direction, and use weaker fill only where needed to keep important actions legible.
10. Correct every invariant violation found through coordinate calculations, XML validation, scene loading, or tests.

## Spatial invariants

- Treat `pos`, `rot`, and `scale` as transforms in the parent group's coordinate frame.
- Treat a wall's local X as length, local Y as height, and local Z as thickness.
- Express wall inserts in the same local frame as the wall. For an opening, calculate:
  - center X: `opening.x + opening.width / 2 - wall.length / 2`
  - center Y: `opening.sill + opening.height / 2`
  - center Z: `0`
- Align the smallest dimension of a thin insert with the wall's local Z thickness axis.
- Prefer a window or door prefab containing its own `<bool-negative-box>` so the opening and insert cannot drift apart. Match the outer frame extents to the cutter extents; inset only the pane or explicitly recessed pieces.
- Keep inserts smaller than their opening only when visible construction clearance is intentional.
- Treat a gap or penetration larger than `0.001` scene units as an error unless the design explicitly requires it.
- Never overlap coplanar visible faces. OpenGL depth settings cannot reliably order surfaces at the same depth; resize or reposition the parts so their exterior faces occupy distinct regions. Adjacent parts may meet at a shared edge.
- Prefer swapping box dimensions over adding a rotation when both describe the same axis-aligned shape in the current local frame.
- Document the default front direction in every directional prefab's leading XML comment.
- Orient prefab instances toward their intended target using the documented front direction; never infer it only from the prefab name.
- Use `attach="name:slot"` for placing objects on prefab surfaces rather than manual vertical position calculations.
- Put surface attach points at the usable surface center, not its front or side edge. Apply deliberate offsets in a shared local group, and keep each object's complete rotated footprint inside the support boundary with visible margin.
- Make lived-in prop clusters irregular but authored: vary yaw, spacing, depth, and scale slightly instead of aligning every center on one axis. Keep the variation deterministic, preserve contact, prevent intersections, and do not tilt an object away from its support unless it pivots plausibly from a contact edge.
- Use `pivotOffset` for hinged rotations (book covers, open drawers) instead of `group` nesting workarounds.
- Use the `<array>` modifier for repeating geometry (books, shelves, stairs) rather than copy-pasting shapes.
- Prefer built-in preset materials (`wall`, `floor`, `wood`, `metal`, `glass`) and backgrounds (`midnight`, `dusk`, `neutral`, `black`). Define `<material>` tags only for custom materials.
- Close interior shells with a ceiling or roof at the wall-top elevation unless an opening is intentional. Keep overhead cameras below a visible ceiling or provide a deliberate non-production plan view.
- Motivate every light with visible or implied scene geometry such as a lamp, window, fire, or doorway. Put a reusable practical's `<light>` inside its prefab so geometry and illumination share one transform.
- Mark visible bulbs, flames, and other self-luminous source geometry `unlit="1" castShadow="0"`. Place the point light inside that source volume and below any opaque shade or lamp body so the fixture does not block its own useful light.
- Give each room a dominant shadow-casting key light. Keep ambient light low enough for shape, but never leave the hero subject or interaction in featureless darkness; add a weaker motivated fill or rim when required for readability.
- Aim directional exterior light through an actual opening. Check that the ceiling and wall shell do not accidentally block the intended window-light path.

## Required CLI validation

```sh
xmllint --noout scenes/scene.xml
make
make test
./build/bin/simplegl scenes/scene.xml -list-cameras
```

Run `xmllint` on every edited scene and prefab. Use the actual target path in place of `scenes/scene.xml`. Treat parser errors, unresolved materials or prefabs, build warnings, test failures, and invalid camera declarations as failures. Do not render screenshots or perform visual inspection as part of validation.

## Prefab rules

- Keep prefab geometry centered around a useful placement origin, normally floor center.
- Keep prefab materials externally resolvable by the containing scene.
- State footprint, baseline, and front direction in the leading comment.
- Declare `<attach>` elements on prefabs that have meaningful surface reference points (tabletop center, seat surface, shelf height).
- Define a shelf attach at the center of each usable shelf surface. Treat edge anchors as separate, explicitly named slots rather than using an edge as the default surface anchor.
- Keep a practical light, its unlit emitter, and its shadow-casting shade in one prefab. Verify transformed and scaled instances keep the point light inside the emitter and on the emitting side of the shade lip.
- Put a wall insert's `<bool-negative-box>` and all visible frame geometry in the same prefab. Center the prefab on the desired opening, align its local Z with wall thickness, and make the cutter deep enough to cross the complete wall.
- Use `source=` on `<prefab>` to specify the file; `name=` only when something references this instance via `attach`.
- Verify a directional prefab's `0`, `90`, `-90`, and `180` orientation mappings numerically before using it repeatedly.
- Prefer a prefab over copied groups so later corrections propagate to every instance.

## Completion standard

Do not report a scene as complete until its XML validates, the project builds, relevant tests pass, and the scene loads through the CLI. For an interior, also verify that the shell includes its intended ceiling or roof, visible practicals own aligned prefab-local lights, emitter geometry is unlit and shadow-free, and every story camera has a readable focal subject with deliberate cast shadows. Report any limitation that CLI validation cannot establish.
