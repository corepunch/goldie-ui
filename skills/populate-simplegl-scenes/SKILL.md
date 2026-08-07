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
9. When composition, lighting, or references are part of the request, render the affected cameras with `make screenshot` and `./build/bin/screenshot scenes/scene.blks -cam CameraName -d 24 -o /tmp/shot.png`, then inspect the image before accepting the edit. `-d 24` hides lamp and character editor overlays. Screenshot review complements, but does not replace, CLI validation.
10. For every interior room, place at least one motivated practical or window light that casts readable shadows. Match visible lamp geometry to its light position, establish a clear key direction, and use weaker fill only where needed to keep important actions legible.
11. Correct every invariant violation found through coordinate calculations, XML validation, scene loading, screenshot review, or tests.

Declare scene-wide ambient light and background only as attributes on the root
element: `<scene ambient="r g b" background="preset-or-rgb">`. Never emit
`<ambient>` or `<background>` child elements. The XML parser accepts those
unknown nodes but ignores them, silently falling back to its defaults.

## Color-space contract

- Author every XML value that represents a visible RGB color in sRGB space,
  using the same `0..1` values a color picker displays. This includes scene
  `ambient` and `background`, material and shape `color`, light and sun
  `color`, unlit emitters, and colored dummy/overlay geometry.
- Do not pre-linearize, gamma-correct, square, or otherwise transform authored
  color values. The renderer converts sRGB colors to linear values exactly
  once at its input boundary, performs lighting in linear space, and relies on
  the sRGB framebuffer to encode the final output for display.
- Treat light `intensity` as a linear scalar, not a color. Never apply an sRGB
  conversion to intensity or fold intensity into the XML `color` value.
  `intensity="2"` supplies twice the linear light energy of `intensity="1"`,
  although the displayed pixel value is not necessarily twice as large after
  lighting, clipping, and sRGB output encoding.
- Treat positions, directions, radius, shininess, transforms, and every other
  non-color number as linear data with no color-space conversion.
- Keep light color channels normally within `0..1`; use `intensity` for HDR
  brightness above white. Use `color="1 0.75 0.4" intensity="2"`, not
  `color="2 1.5 0.8" intensity="1"`.

See [references/scene-format.md](references/scene-format.md#color-space-and-numeric-units)
for the complete attribute classification and renderer data flow.

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
- Use `attach="name:slot"` for placing objects on prefab surfaces rather than manual vertical position calculations. The attached element inherits the instance's full world transform — its `pos`, `rot`, and `scale` are applied in the instance's local frame at the attach point. Objects placed on a rotated workbench automatically stay flat on the surface without the author needing to match rotations.
- Put surface attach points at the usable surface center, not its front or side edge. Apply deliberate offsets in a shared local group, and keep each object's complete rotated footprint inside the support boundary with visible margin.
- Make lived-in prop clusters irregular but authored: vary yaw, spacing, depth, and scale slightly instead of aligning every center on one axis. Keep the variation deterministic, preserve contact, prevent intersections, and do not tilt an object away from its support unless it pivots plausibly from a contact edge.
- Use `pivotOffset` for hinged rotations (book covers, open drawers) instead of `group` nesting workarounds.
- Use the `<array>` modifier for repeating geometry (books, shelves, stairs) rather than copy-pasting shapes.
- Prefer built-in preset materials (`wall`, `floor`, `wood`, `metal`, `glass`) and backgrounds (`midnight`, `dusk`, `neutral`, `black`). Define `<material>` tags only for custom materials.
- Close interior shells with a ceiling or roof at the wall-top elevation unless an opening is intentional. Keep overhead cameras below a visible ceiling or provide a deliberate non-production plan view.
- Motivate every light with visible or implied scene geometry such as a lamp, window, fire, or doorway. Put a reusable practical's `<light>` inside its prefab so geometry and illumination share one transform.
- Mark visible bulbs, flames, and other self-luminous source geometry `unlit="1" castShadow="0"`. Place the point light inside that source volume and below any opaque shade or lamp body so the fixture does not block its own useful light.
- Every scene object and architectural element must cast shadows (`castShadow="1"`) unless it is self-luminous emitter geometry. Ceilings, floors, walls, furniture, and props all contribute to the stencil shadow volumes. The only legitimate `castShadow="0"` exceptions are: unlit light bulbs/flames, glass panes (which are opaque in fixed-function but conceptually transparent), shadow-catcher placeholder planes, and deliberately composited scene boundaries.
- Give each room a dominant shadow-casting key light. Keep ambient light low enough for shape, but never leave the hero subject or interaction in featureless darkness; add a weaker motivated fill or rim when required for readability.
- Aim directional exterior light through an actual opening. Use a **45–60 degree downward** angle and offset it **15–45 degrees from the wall axis** so cast shadows fall diagonally rather than parallel to walls. `dir="-0.6 -1 1"` gives ~45° down and ~30° horizontal offset; `dir="0 -1.7 1"` gives ~60° down. Never set the horizontal component to zero — that produces shadows aligned to walls, which reads as flat and uninteresting. Check that the ceiling and wall shell do not accidentally block the intended window-light path.
- Treat a window as both a compositional subject and a lighting instrument, not background decoration. In at least one establishing or action camera, frame the window itself or its bright spill so the source of the key is legible. Place the window on the side of the hero work surface that the camera can plausibly see; a distant window behind the action often lights only an empty floor and contributes neither story nor depth.
- Solve daylight placement against the hero surface before committing to the room layout. For a window-center ray `p + t * dir` (where `dir.y < 0`) and a tabletop at height `h`, use `t = (h - p.y) / dir.y`, then check the resulting X/Z point lands inside the tabletop footprint. Move the opening or adjust the sun direction until it crosses a prop cluster rather than bare floor. Window frames and mullions must cast shadows so this spill creates readable, crisp patterned shapes across the table and its objects.
- When the camera must view a closed room from the outside, mark the camera-facing wall `renderable="0" castShadow="1"`. It remains invisible while preserving correct interior shadow and light-blocking behavior.
- Verify that traversal mechanisms (lifts, stairs, ladders) are positioned adjacent to their destination platform. The base should sit near the lower platform and the top should reach near the upper platform so a single shot can capture both ends of the traversal.

## Required CLI validation

```sh
xmllint --noout scenes/scene.blks
xmllint --xpath 'count(/scene/ambient | /scene/background)' scenes/scene.blks
make
make test
./build/bin/simplegl scenes/scene.blks -list-cameras
./build/bin/simplegl scenes/scene.blks -test
```

Run `xmllint` on every edited scene and prefab. Use the actual target path in place of `scenes/scene.blks`. Treat parser errors, `unsupported XML element` warnings on stderr, unresolved materials or prefabs, build warnings, test failures, and invalid camera declarations as failures. Do not render screenshots or perform visual inspection as part of validation.
The XPath count must print `0`; any other value means scene-wide settings were
written as ignored child elements instead of root attributes.

## Prefab rules

- Keep prefab geometry centered around a useful placement origin, normally floor center.
- Keep prefab materials externally resolvable by the containing scene.
- State footprint, baseline, and front direction in the leading comment.
- Declare `<attach>` elements on prefabs that have meaningful surface reference points (tabletop center, seat surface, shelf height). Name the primary work surface `top_surface`; use `under_center`, `shelf_lower`, `shelf_upper`, or `edge_n`/`edge_s` for secondary slots.
- Define a shelf attach at the center of each usable shelf surface. Name them by tier (`shelf_lower`, `shelf_upper`). Treat edge anchors as separate, explicitly named slots rather than using an edge as the default surface anchor.
- Keep a practical light, its unlit emitter, and its shadow-casting shade in one prefab. Verify transformed and scaled instances keep the point light inside the emitter and on the emitting side of the shade lip.
- Put a wall insert's `<bool-negative-box>` and all visible frame geometry in the same prefab. Center the prefab on the desired opening, align its local Z with wall thickness, and make the cutter deep enough to cross the complete wall.
- Use `source=` on `<prefab>` to specify the file; `name=` only when something references this instance via `attach`.
- Verify a directional prefab's `0`, `90`, `-90`, and `180` orientation mappings numerically before using it repeatedly.
- Prefer a prefab over copied groups so later corrections propagate to every instance.

## Completion standard

Do not report a scene as complete until its XML validates, the project builds, relevant tests pass, and the scene loads through the CLI. For an interior, also verify that the shell includes its intended ceiling or roof, visible practicals own aligned prefab-local lights, emitter geometry is unlit and shadow-free, and every story camera has a readable focal subject with deliberate cast shadows. Report any limitation that CLI validation cannot establish.
