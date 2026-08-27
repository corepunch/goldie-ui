# Layout and validation reference

## Coordinate model

SimpleGL uses X for horizontal width, Y for height, and Z for depth. Primitive `pos` values identify their centers. A box resting on `y=0` therefore uses `pos.y = size.y / 2`.

Transforms are hierarchical. A child inside `<group>` uses group-local coordinates. Use this for assemblies and every collection that shares a rotated coordinate frame.

```xml
<group pos="-4 0 0" rot="0 90 0">
  <wall length="6" height="2.8" thickness="0.2" material="wall">
    <opening type="window" x="2" width="1.4" height="1.2" sill="0.9"/>
  </wall>
  <box pos="-0.3 1.5 0" size="1.3 1.1 0.03"
       material="glass"/>
</group>
```

The wall and pane above cannot disagree about the parent's translation or rotation. The pane is slightly smaller than the opening and thin along local Z.

## Wall opening calculations

Walls are centered on their `pos`, but opening `x` is measured from the wall's local left edge. For wall length `L`, opening start `x`, width `w`, sill `s`, and height `h`:

```text
insert center = (x + w/2 - L/2, s + h/2, 0)
insert size   = (w - horizontal_clearance, h - vertical_clearance, thickness)
```

Use deliberate, symmetric clearance. A pane can be inset behind a perimeter
frame, but the frame's outside boundary must meet the opening boundary unless a
visible construction gap is intentional. A crossbar or mullion alone is not a
perimeter frame.

Prefer packaging the opening and insert together:

```xml
<!-- prefab origin and cutter are at the opening center -->
<prefab>
  <bool-negative-box size="1.4 1.2 0.3"/>
  <!-- frame outer extents are exactly 1.4 x 1.2 -->
</prefab>
```

Place that prefab at the intended opening center, with its local Z aligned to
wall thickness. Its cutter depth must span both wall faces with at least
`0.001` scene units of excess per face. Keep the cutter wholly inside the
wall's X/Y extents, and never combine it with a duplicate child `<opening>`.
The renderer pre-collects prefab cutters, so the wall may appear earlier in the
XML. Cutters are rectangular wall metadata; arbitrary or oblique mesh CSG is
not supported.

Do not place a pane as a world-space sibling of a rotated wall. If grouping is impossible, transform both its center and thin axis into world space explicitly. For a wall rotated 90 degrees around Y, an unrotated world-space box can represent the pane by swapping its X and Z sizes, but grouping remains safer.

## Primitive placement

- Box: centered on all axes. Rest on a surface using half its Y size.
- Sphere: centered at `pos`; rest on a surface using `pos.y = radius`.
- Cylinder/prism/cone/pyramid: centered along Y; rest using half `height`.
- Torus: centered at `pos`, lying in the XZ plane.
- Wall: base is at local `y=0`; length extends symmetrically around local X after the wall transform.

For assemblies, calculate positions from declared dimensions. Avoid visually tuned constants until the structural dimensions are correct.

Use `0.001` scene units as the default contact tolerance. For a nominally grounded or connected part, the absolute difference between its lower/upper surface and the target surface must not exceed that tolerance. Small gaps such as `0.005` are errors, not harmless rounding.

Do not overlap visible faces on the same plane. A depth buffer cannot consistently decide which coplanar fragment owns a pixel, so the result flickers or forms striped patches as the camera moves. Build assemblies from non-overlapping exterior regions: for example, fit a sofa base and backrest between its arms instead of extending all three boxes across the same front or side planes. Adjacent parts may share an edge, and hidden structural intersections are acceptable only when none of their exterior faces overlap. Do not use tiny offsets or polygon offset to conceal unintended duplicate geometry.

## Visual relationships and termination

Classify nearby geometry before spacing it:

- **Assembly contact:** Parts that construct or operate as one object, such as a table and its chairs, a window frame and mullions, or a lamp and cord, may touch or overlap where the relationship is intentional.
- **Independent objects:** Unrelated fixtures, furniture, and decorations must retain readable negative space. Do not let their bounds, silhouettes, or cast shadows touch accidentally; tangency makes separate objects read as one malformed assembly.

Make every exposed linear member terminate against an intended support. Bars,
mullions, rails, legs, and cords must not stop visibly inside open space. Keep a
nominal contact within `0.001` scene units. For a rectangular member meeting a
curved frame, calculate the boundary at the member's outermost edge so both
corners reach or enter the support without leaving a visible gap. For a circular
boundary centered at `(cx, cy)` with radius `r`, the upper intersection at local
X coordinate `x` is:

```text
y = cy + sqrt(r*r - (x - cx)*(x - cx))
```

Use the member edge nearest the tighter part of the curve, not only its center
line. Permit a small hidden penetration into the support when necessary, but do
not overshoot through its visible exterior face.

Check independent-object spacing in the rendered view as well as world space.
Perspective can close a valid three-dimensional gap, and cast shadows can merge
otherwise separate silhouettes. Start with a projected gap at least as wide as
the smaller object's nearby trim or structural member, then enlarge it until the
separation remains obvious in every affected story camera. Treat deliberate
occlusion as a composition choice and verify that it does not imply a false
physical connection.

## Attach points and pivot offset

Use semantic subfolders to organize authored prefab families and composite
objects. Keep generic furniture and items independent, then reference them from
room-specific composites such as `workshop/desks/main.blk` or
`workshop/commode/stocked.blk`. Do not encode hierarchy with underscore
prefixes in a flat filename.

Use `attach="instanceName:slotName"` on any shape or prefab to place it at a prefab's named reference point without manual surface-height calculations. The target instance must carry a `name` attribute.

```xml
<prefab source="furniture/dining_table" name="dining_table" pos="0 0 -1.5"/>
<sphere attach="dining_table:center" radius="0.14" material="fabric"/>
```

The sphere lands on the table surface without computing `y = tableTop + sphereRadius`. When `attach` is present the element's `pos` is replaced by the attach point's world-space position.

Place a general-purpose surface attach at the usable surface center. Name it
`top_surface` for the primary work or table surface. Name additional named
slots by location: `under_center` for the clearance volume beneath a bench,
`shelf_lower` and `shelf_upper` for tiered storage, or `edge_n`/`edge_s` for
perimeter positions. Do not put the default shelf/table attach on its front
edge: a child prefab is positioned by its origin, so doing so centers half of
the child's footprint outside the support. If a composition needs multiple
offsets, place the support and props inside one local `<group>`, use the
surface height as each child's baseline, and author local X/Z offsets from the
surface center.

Before accepting a supported prop, transform all footprint corners by its
scale and yaw and verify they remain within the support rectangle with a small
visible margin. Checking only the origin is insufficient, especially after
rotation.

For shelves, desks, and active work surfaces, avoid mechanically uniform rows.
Use small deterministic differences in yaw, spacing, depth, and scale while
maintaining exact vertical contact and non-intersection. Rotate asymmetric
props enough to read but not so far that they overhang. A cylinder does not
visibly change under Y rotation; vary its position or neighboring silhouette
instead. Use X/Z tilt only when the prefab origin is a plausible contact pivot
and the resulting footprint does not penetrate the support.

Treat visible storage volume as part of the authored object. A commode with an
open lower bay, a cubby wall, or a dressed desk should usually be a composite
prefab that owns smaller item-prefab instances. Stock the volume at multiple
depths and heights while keeping item footprints inside the support.

Use `pivotOffset` to rotate a shape around an edge instead of its center. The offset is in local space, applied before rotation:

```xml
<box size="0.3 0.02 0.2" rot="0 0 45" pivotOffset="-0.15 0 0"/>
```

## Rotation and facing

`rot="rx ry rz"` applies X, then Y, then Z rotations. Positive Y rotation maps local +Z toward world +X and local +X toward world -Z.

Current directional prefabs:

| Prefab | Default front | Footprint |
|---|---|---|
| `chair` | +Z | approximately 0.45 × 0.45 |
| `sofa` | +Z | approximately 2.2 × 0.9 |
| `coffee_table` | none | 1.2 × 0.7 |
| `dining_table` | none | 1.6 × 1.0 |

For an object facing +Z by default:

| Y rotation | Resulting front |
|---:|---|
| `0` | +Z |
| `90` | +X |
| `-90` | -X |
| `180` | -Z |

For a prefab whose default front is local +Z and a target direction `(dx, dz)`, use `rot.y = atan2(dx, dz)` converted to degrees. Cardinal rotations are preferable when the intended direction is cardinal; otherwise calculate the angle rather than eyeballing it.

## Materials and renderer constraints

Built-in preset materials are always available: `wall`, `floor`, `wood`, `metal`, `glass`. Reference them without defining `<material>` tags. Define custom materials explicitly; a scene-defined `<material>` with the same `id` overrides a preset.

Built-in background presets: `midnight`, `twilight`, `dusk`, `dawn`, `overcast`, `noon`, `neutral`, `black`. Use `<scene background="dusk">` for presets or `<scene background="r g b">` for a custom color.

Author material, shape, ambient, background, light, sun, unlit-emitter, and
dummy colors as sRGB `0..1` values. Do not manually linearize them. Light
`intensity` is a separate linear scalar and must not be gamma-corrected or
folded into `color`; keep the color within `0..1` and raise `intensity` when a
source must be brighter than white. The renderer converts colors once before
linear lighting and automatically encodes the result when writing the sRGB
framebuffer. See
[scene-format.md](scene-format.md#color-space-and-numeric-units) for the full
attribute table.

Materials provide diffuse color and shininess only. There is no transparency or texture support. A material named `glass` renders as an opaque shiny surface; make it thin, and do not promise transparent glass.

Use the default shadow casting for floors, panes, and decorative surfaces. Reserve `castShadow="0"` for self-luminous emitters or explicitly documented non-physical helper geometry. Use `castShadows="0"` on a light for an unshadowed additive light.

Use `renderable="0" castShadow="1"` for scene boundaries that must remain invisible to the camera while still blocking light and contributing to stencil shadow volumes.

Use the `<array>` modifier to create repeating geometry (books, shelves, stairs). It duplicates the mesh `count` times with per-step `translation` and `rotation`, producing compact scene files for repetitive structures.

## Interior enclosure and lighting

Treat a room as a complete shell: floor, walls, and ceiling or roof. In the common box-room case, place the ceiling so its lower face meets the wall tops. Omit it only for an explicitly open or roofless design. Keep plan cameras below a visible ceiling so they continue to show the interior.

When cameras need to view a closed room from outside, mark the camera-facing wall
`renderable="0" castShadow="1"`. It stays invisible to the viewer but continues
blocking directional light and contributing to stencil shadow volumes so the
interior lighting remains correct:

```xml
<wall pos="0 0 0" length="10" height="4.2" thickness="0.2"
      material="plaster" renderable="0" castShadow="1"/>
```

Follow the enclosed-room pattern in `scenes/sample_room.blks`: combine a low ambient base with at least one motivated, shadow-casting key light. A room must contain light sources that shape the space, not merely enough ambient illumination to avoid black pixels.

- Put a reusable practical point light inside the same prefab as its fixture geometry. Place it inside the bulb or flame, below the ceiling and on the emitting side of any opaque shade.
- Mark the visible emitter `unlit="1" castShadow="0"`: unlit keeps its authored bright color, while disabling shadow casting prevents it from occluding its own point light. Keep the shade and fixture body shadow-casting.
- Use a window, doorway, or second practical to motivate a weaker fill or rim. Keep it subordinate to the key so shadows remain dramatic.
- Aim a directional sun or moon light through the corresponding opening rather than through a solid wall or ceiling. Point`dir` **45–60 degrees below horizontal** and offset it **15–45 degrees horizontally from the wall axis**: `dir="-0.6 -1 1"` for 45° down with 30° offset, `dir="0 -1.7 1"` for ~60° down. Never let either horizontal component be zero — shadows parallel to walls read as flat and uninteresting.
- Confirm important characters, props, and interactions receive both readable illumination and grounding cast shadows.
- Avoid lifting ambient light until shadows disappear. Correct the key position, intensity, and motivated fill first.
- Compare and tune light intensities as linear multipliers. Do not apply gamma
  compensation to an intensity because the displayed result already receives
  sRGB encoding after lighting.

## Camera declarations

Aim cameras at useful targets, not arbitrary Euler directions. Keep the near plane away from geometry.

Every `<camera>` must carry a `comment` attribute describing its purpose. The `--list-cameras` flag reads these comments so an automated agent can select the right view without parsing the full XML:

```sh
./build/bin/scener --list-cameras scenes/scene.blks
```

Example output:

```text
Main             "Front-half view from entrance"
Top              "Overhead top-down"
Close            "Close-up of dining table"
```

Use descriptive comments: a person or agent reading the list should understand what each camera shows and when to select it.

## Review checklist

Before completion, verify:

1. Every material reference resolves in the containing scene.
2. Every opening lies within its wall and does not overlap another opening unexpectedly.
3. Every insert uses the wall's local frame and fits inside the opening.
4. Every grounded object touches the intended surface without penetrating it.
5. Every directional prefab faces its target.
6. Repeated objects use prefabs or groups rather than divergent copies.
7. Exterior face extents do not overlap on the same plane.
8. Every interior has its intended ceiling or roof, and overhead cameras remain inside the shell.
9. Every room has a motivated shadow-casting key; reusable practicals own prefab-local point lights, emitters are unlit and shadow-free, and hero subjects remain readable.
10. Every supported prop's transformed footprint stays inside its surface; default surface attach points are centered rather than placed on an edge.
11. Lived-in prop clusters use deliberate variation without floating, penetration, overlap, or accidental overhang.
12. Edited XML files pass `xmllint --noout`.
13. Every practical point light remains inside its emitter and below the shade lip after instance transforms and scale.
14. The project builds, relevant tests pass, and the scene loads with `--list-cameras`.
15. Every window or door prefab cutter crosses its wall completely, remains wall-axis-aligned, and matches the visible outer frame boundary without an accidental reveal gap.
16. Every visible RGB value is authored directly as sRGB, while light
    intensity remains a separate, unmodified linear scalar.
17. Every exposed bar, mullion, rail, leg, cord, or similar member terminates cleanly against its intended support without a visible floating endpoint or exterior overshoot.
18. Unrelated neighboring objects retain readable negative space in every affected story camera; no accidental overlap, silhouette tangency, or shadow merger makes them appear grouped.
