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
       material="glass" castShadow="0"/>
</group>
```

The wall and pane above cannot disagree about the parent's translation or rotation. The pane is slightly smaller than the opening and thin along local Z.

## Wall opening calculations

Walls are centered on their `pos`, but opening `x` is measured from the wall's local left edge. For wall length `L`, opening start `x`, width `w`, sill `s`, and height `h`:

```text
insert center = (x + w/2 - L/2, s + h/2, 0)
insert size   = (w - horizontal_clearance, h - vertical_clearance, thickness)
```

Use deliberate, symmetric clearance. A pane that should nearly fill an opening normally subtracts `0.01` to `0.10` total scene units from width and height, depending on scene scale. Record unusually large clearance as an intentional design choice.

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

Materials provide diffuse color and shininess only. There is no transparency or texture support. A material named `glass` renders as an opaque shiny surface; make it thin and set `castShadow="0"`, but do not promise transparent glass.

Use `castShadow="0"` for floors, panes, and decorative surfaces that should not create stencil volumes. Use `castShadows="0"` on a light for an unshadowed additive light.

## Camera declarations

Aim cameras at useful targets, not arbitrary Euler directions. Keep the near plane away from geometry.

Every `<camera>` must carry a `comment` attribute describing its purpose. The `-list-cameras` flag reads these comments so an automated agent can select the right view without parsing the full XML:

```sh
./build/bin/simplegl scenes/scene.xml -list-cameras
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
8. Edited XML files pass `xmllint --noout`.
9. The project builds, relevant tests pass, and the scene loads with `-list-cameras`.
