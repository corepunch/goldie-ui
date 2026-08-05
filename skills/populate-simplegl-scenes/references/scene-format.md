# SimpleGL XML scene format reference

SimpleGL reads scenes from XML files. The root node is `<scene>`, containing a mix of config tags and shape objects.

## Quick example

```xml
<scene>
  <camera name="Main" comment="Default forward view" pos="0 2 6" look="0 1 0" fov="60"/>
  <ambient color="0.10 0.10 0.13"/>
  <background color="0.04 0.05 0.07"/>

  <light pos="0 4 0" color="1.0 0.95 0.85" intensity="1.3" castShadows="1"/>

  <material id="red" color="0.8 0.2 0.2" shininess="20"/>

  <box pos="0 0 0" size="2 2 2" material="red"/>
  <sphere pos="3 1 0" radius="0.8" color="0.2 0.5 1.0"/>
</scene>
```

## Config tags (top-level children of `<scene>`)

### `<camera>`

Defines a viewpoint. Multiple cameras allowed; select via `-cam Name` on the command line, or list all cameras with `-list-cameras`.

| Attribute | Type   | Default  | Description |
|-----------|--------|----------|-------------|
| `name`    | string | Camera1  | Camera identifier |
| `comment` | string | (empty)  | Human-readable description shown by `-list-cameras` |
| `pos`     | vec3   | 0 1.6 5  | Eye position |
| `look`    | vec3   | 0 1.2 0  | Look-at target |
| `fov`     | float  | 60       | Vertical FOV in degrees |

When no `<camera>` tag is present, a default "Camera1" is created with the defaults above.

### `<ambient>`

| Attribute | Type | Default           | Description |
|-----------|------|-------------------|-------------|
| `color`   | vec3 | 0.12 0.12 0.14    | Global ambient light |

### `<background>`

| Attribute | Type   | Default         | Description |
|-----------|--------|-----------------|-------------|
| `id`      | string | (none)          | Reference a preset background by name |
| `color`   | vec3   | 0.08 0.10 0.14  | Custom clear/background colour |

Preset backgrounds (use via `id`):

| Preset     | Color           | Notes |
|------------|-----------------|-------|
| `midnight` | 0.02 0.03 0.07 | Deep night blue |
| `twilight` | 0.06 0.05 0.10 | Purple-blue, post-sunset |
| `dusk`     | 0.08 0.10 0.14 | Dark blue-grey |
| `dawn`     | 0.16 0.10 0.14 | Warm dusky pink |
| `overcast` | 0.25 0.27 0.30 | Flat cloudy grey |
| `noon`     | 0.40 0.48 0.64 | Bright sky blue |
| `neutral`  | 0.18 0.20 0.24 | Mid-grey |
| `black`    | 0.00 0.00 0.00 | Pure black for compositing |

```xml
<background id="dusk"/>
<!-- or custom: -->
<background color="0.04 0.05 0.07"/>
```

### `<material>`

Named material referenced by shapes via the `material` attribute. Scene-defined materials override built-in presets of the same name.

| Attribute    | Type   | Default        | Description |
|--------------|--------|----------------|-------------|
| `id`         | string | (required)     | Unique material name |
| `color`      | vec3   | 0.8 0.8 0.8    | Diffuse RGB colour |
| `shininess`  | float  | 8.0            | Phong specular exponent |

Built-in presets (always available, no `<material>` tag needed):

| Preset    | Color           | Shininess | Notes |
|-----------|-----------------|-----------|-------|
| `wall`    | 0.80 0.78 0.72 | 6         | Warm off-white interior |
| `floor`   | 0.35 0.28 0.22 | 12        | Dark wood floor |
| `wood`    | 0.50 0.32 0.18 | 20        | Mid-brown furniture wood |
| `metal`   | 0.70 0.70 0.75 | 60        | Grey metallic |
| `glass`   | 0.65 0.80 0.85 | 90        | Blue-tinted shiny |
| `stone`   | 0.38 0.36 0.33 | 8         | Grey stone block |
| `concrete`| 0.52 0.50 0.46 | 4         | Matte grey |
| `plaster` | 0.90 0.88 0.80 | 3         | Warm flat white |
| `bronze`  | 0.48 0.30 0.14 | 40        | Dark brown metallic |
| `iron`    | 0.28 0.28 0.30 | 55        | Dark grey metallic |

To override a preset, define a `<material>` with the same `id` in the scene. Unlisted materials (e.g. `brass`, `slate`, `fabric`) must be defined explicitly.

### `<light>`

Point light source.

| Attribute    | Type | Default | Description |
|--------------|------|---------|-------------|
| `pos`        | vec3 | 0 3 0   | Light position |
| `color`      | vec3 | 1 1 1   | Light RGB colour |
| `intensity`  | float| 1.0     | Brightness multiplier |
| `castShadows`| int  | 1       | Whether this light casts stencil shadows |

`<light>` is transformable scene content as well as a valid top-level node. A
top-level position is in world space. Inside a `<group>` or prefab, its position
is local and inherits the complete parent translation, rotation, and scale.
Instance scale changes the light's positional offset but not its intensity.

Keep reusable practical lights inside their fixture prefab:

```xml
<!-- origin is the ceiling suspension point -->
<prefab>
  <cone pos="0 -0.53 0" radius="0.28" radiusTop="0.08" height="0.20" material="brass"/>
  <sphere pos="0 -0.61 0" radius="0.07" color="1 0.96 0.82"
          unlit="1" castShadow="0"/>
  <light pos="0 -0.64 0" color="1 0.70 0.36"
         intensity="0.85" castShadows="1"/>
</prefab>
```

The light above is inside the bulb and slightly below the opaque shade lip.
The shade may cast shadows; the emitter must not.

### `<sun>`

Directional light (infinite distance). Light rays travel parallel in the given `dir`. Useful for sunlight, moonlight, or any distant light source.

| Attribute    | Type | Default | Description |
|--------------|------|---------|-------------|
| `dir`        | vec3 | 1 -1 0  | Direction light travels (normalized automatically) |
| `color`      | vec3 | 1 1 1   | Light RGB colour |
| `intensity`  | float| 1.0     | Brightness multiplier |
| `castShadows`| int  | 1       | Whether this light casts stencil shadows |

Example: sunlight streaming through a window from the left:

```xml
<sun dir="0.75 -0.55 0.35" color="0.95 0.90 0.80" intensity="1.1" castShadows="1"/>
```

## Shape objects

All shapes share common attributes plus shape-specific ones. Shapes may contain modifier child elements.

### Common attributes

| Attribute    | Type   | Default          | Description |
|--------------|--------|------------------|-------------|
| `pos`        | vec3   | 0 0 0            | World position |
| `rot`        | vec3   | 0 0 0            | Euler rotation `rx ry rz` in degrees, applied X→Y→Z |
| `scale`      | vec3   | 1 1 1            | Non-uniform scale (ignored by `<wall>`) |
| `material`   | string | (none)           | Reference to a `<material id="...">` |
| `color`      | vec3   | 0.8 0.8 0.8      | Diffuse colour (used if no material ref) |
| `shininess`  | float  | 8.0              | Specular exponent (used if no material ref) |
| `castShadow` | int    | 1                | Whether this object casts shadows |
| `renderable` | int    | 1                | Whether this object is drawn; non-renderable objects may still cast shadows |
| `unlit`      | int    | 0                | Render the authored diffuse color without ambient or additive light contribution |
| `pivotOffset`| vec3   | 0 0 0            | Offsets the rotation center. Translate by offset, rotate, translate back |
| `attach`     | string | (none)           | Snap to a named instance's attach point: `instanceName:slotName` |
| `tint`       | int    | 0                | On prefab children, allow instance `color` to replace this part's diffuse color |

If a `material` attribute is given, `color` and `shininess` are taken from the referenced material definition.

An unlit object still renders into the depth buffer. Its `castShadow` setting is
independent, although visible emitter geometry normally uses
`unlit="1" castShadow="0"` so it stays bright without blocking the light it
represents.

**Rotation convention:** `rot="rx ry rz"` applies rotations around X, then Y, then Z. Positive Y rotation maps the **+X** axis toward **-Z** and **+Z** toward **+X**.

### `pivotOffset` attribute

Shifts the rotation center away from the object's origin. The object is translated by `+pivotOffset`, rotated, then translated by `-pivotOffset`. Useful for hinged objects (book covers, open drawers) or any shape that should rotate around an edge instead of its center.

```xml
<!-- box rotating 45° around its left edge (half-width = -0.15) -->
<box pos="0 0.5 0" size="0.3 0.02 0.2" rot="0 0 45" pivotOffset="-0.15 0 0"/>
```

The `pivotOffset` is in local space, applied before the object's own `rot`. Rotation-only matrices (for normals) are unaffected.

### `<box>`

Axis-aligned box centered at origin.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `size`    | vec3 | 1 1 1   | Width, height, depth |

### `<sphere>`

UV sphere centered at origin.

| Attribute | Type  | Default | Description |
|-----------|-------|---------|-------------|
| `radius`  | float | 0.5     | Sphere radius |
| `rings`   | int   | 16      | Latitude subdivisions |
| `slices`  | int   | 24      | Longitude subdivisions |

### `<cylinder>`

Capped cylinder along the Y axis.

| Attribute | Type  | Default | Description |
|-----------|-------|---------|-------------|
| `radius`  | float | 0.5     | Radius |
| `height`  | float | 1.0     | Height along Y |
| `sides`   | int   | 24      | Number of radial segments |

### `<prism>`

Regular N-sided prism along Y (flat-shaded).

| Attribute | Type  | Default | Description |
|-----------|-------|---------|-------------|
| `radius`  | float | 0.5     | Radius of circumscribed circle |
| `height`  | float | 1.0     | Height along Y |
| `sides`   | int   | 6       | Number of sides (default: hexagonal) |

### `<cone>` / `<pyramid>`

Truncated cone (frustum) along Y. `<pyramid>` is a `4`-sided default.

| Attribute   | Type  | Default | Description |
|-------------|-------|---------|-------------|
| `radius`    | float | 0.5     | Base radius |
| `radiusTop` | float | 0.0     | Top radius (0 = pointed) |
| `height`    | float | 1.0     | Height along Y |
| `sides`     | int   | 24/4    | Radial segments (24 for cone, 4 for pyramid) |

### `<torus>`

Torus lying in the XZ plane, centered at origin.

| Attribute       | Type  | Default | Description |
|-----------------|-------|---------|-------------|
| `majorRadius`   | float | 0.5     | Distance from center to tube center |
| `minorRadius`   | float | 0.15    | Tube radius |
| `majorSegments` | int   | 24      | Segments around the ring |
| `minorSegments` | int   | 12      | Segments around the tube |

### `<wall>`

A flat wall with rectangular openings (doors/windows). Uses "boolean via boxes" — the wall is split into box segments around openings. Walls ignore `scale`; `castShadow` applies to the generated wall boxes.

| Attribute   | Type  | Default | Description |
|-------------|-------|---------|-------------|
| `length`    | float | 4.0     | Wall length along local X |
| `height`    | float | 2.7     | Wall height along local Y |
| `thickness` | float | 0.2     | Wall depth along local Z |

Child `<opening>` elements:

| Attribute | Type   | Default     | Description |
|-----------|--------|-------------|-------------|
| `type`    | string | "door"      | "door" or "window" |
| `x`       | float  | 0           | Position along wall length |
| `width`   | float  | 1.0         | Opening width |
| `height`  | float  | 2.1/1.2     | Opening height (door: 2.1, window: 1.2) |
| `sill`    | float  | 0.0/0.9     | Height from floor (door: 0.0, window: 0.9) |

Walls also consume intersecting `<bool-negative-box>` nodes collected from the
scene and instantiated prefabs before wall geometry is built. This makes a
window or door prefab capable of carrying its own rough opening. Document order
does not matter.

### `<bool-negative-box>`

A non-rendered rectangular wall cutter. It accepts the common `pos`, `rot`,
`scale`, and `pivotOffset` transforms plus `size`. Its local X is opening width,
local Y is opening height, and local Z must cross the wall thickness.

This is deliberate wall-opening metadata, not unrestricted mesh CSG: it cuts
only `<wall>` geometry, and its axes must align with the target wall's local
axes. Parent or prefab rotation is supported when the cutter and wall remain
aligned. Oblique cutters are ignored.

```xml
<!-- window.xml: origin is the opening center; local +Z faces the room -->
<prefab>
  <bool-negative-box size="2.0 1.7 0.30"/>
  <box pos="-0.94 0 0.04" size="0.12 1.70 0.16" material="wood"/>
  <box pos="0.94 0 0.04" size="0.12 1.70 0.16" material="wood"/>
  <!-- top, bottom, pane, and mullions -->
</prefab>
```

Place the complete insert once: `<prefab source="window" pos="x y z"/>`.
Do not declare a duplicate child `<opening>` on the wall.

### `<group>`

A container that applies its transform to all children. Groups do not render geometry.

```xml
<group pos="0 2 0" rot="0 45 0">
  <box size="1 1 1"/>
  <sphere pos="1 0 0" radius="0.3"/>
</group>
```

## Modifiers

Modifiers are child elements of any shape (`<box>`, `<sphere>`, `<cylinder>`, `<prism>`, `<cone>`, `<pyramid>`, `<torus>`). They are applied in document order, in local-space (before transform).

Deform modifiers (`taper`, `twist`, `bend`, `stretch`, `skew`) operate relative to the object's bounding box along the chosen axis. The axis range [min, max] is mapped to [0, 1]. Each accepts `axis="y"` (`x`, `y`, or `z`), defaulting to `"y"`.

The `<array>` modifier duplicates the mesh rather than deforming it. Place it last so copies include the effect of any preceding deform modifiers.

### `<taper>`

Scales the two perpendicular axes along the chosen axis.

| Attribute    | Type   | Default | Description |
|--------------|--------|---------|-------------|
| `amount`     | float  | 0.0     | Taper intensity (± values, positive = narrow top along axis) |
| `curvature`  | float  | 1.0     | Non-linearity exponent (1 = linear, >1 = bowed, <1 = pinched) |
| `axis`       | char   | y       | Axis along which taper varies (x, y, or z) |

```xml
<box size="2 3 2">
  <taper amount="0.4" axis="y"/>
</box>
```

### `<twist>`

Rotates the perpendicular axes around the chosen axis proportionally to position along it. Normals are also rotated.

| Attribute | Type  | Default | Description |
|-----------|-------|---------|-------------|
| `angle`   | float | 0.0     | Total twist angle in degrees |
| `axis`    | char  | y       | Axis around which to twist (x, y, or z) |

```xml
<box size="2 4 2">
  <twist angle="45" axis="y"/>
</box>
```

### `<bend>`

Bends the chosen axis into a circular arc in the plane defined by the axis and its first perpendicular component.

| Attribute | Type  | Default | Description |
|-----------|-------|---------|-------------|
| `angle`   | float | 0.0     | Bend angle in degrees |
| `axis`    | char  | y       | Axis to bend (x, y, or z) |

```xml
<cylinder radius="0.2" height="3">
  <bend angle="90" axis="y"/>
</cylinder>
```

### `<stretch>`

Non-linear stretch/squash along the chosen axis; pinch the middle, expand the ends (or vice versa).

| Attribute  | Type  | Default | Description |
|------------|-------|---------|-------------|
| `amount`   | float | 0.0     | Stretch amount (positive = squeeze middle) |
| `amplify`  | float | 1.0     | How much perpendicular axes squeeze in response |
| `axis`     | char  | y       | Axis along which to stretch (x, y, or z) |

```xml
<sphere radius="0.5">
  <stretch amount="0.5" amplify="0.5" axis="y"/>
</sphere>
```

### `<skew>`

Shears the two perpendicular axes proportionally to position along the chosen axis.

| Attribute | Type  | Default | Description |
|-----------|-------|---------|-------------|
| `amount`  | float | 0.0     | Skew intensity (positive = top shifts along perpendiculars) |
| `axis`    | char  | y       | Axis along which to skew (x, y, or z) |

```xml
<box size="1 3 1">
  <skew amount="0.5" axis="y"/>
</box>
```

### `<array>`

Duplicates the mesh `count` times, applying a per-step translation and rotation to each copy. Useful for rows of books, shelf tiers, stair steps, or any repeating geometry. The array operates in local mesh space before the shape's own transform.

| Attribute     | Type  | Default | Description |
|---------------|-------|---------|-------------|
| `count`       | int   | 1       | Total copies (original + duplicates). Must be >= 1 |
| `translation` | vec3  | 0 0 0   | Offset per step |
| `rotation`    | vec3  | 0 0 0   | Euler rotation per step in degrees |

```xml
<!-- 8 books in a row, each 0.03 apart along X -->
<box size="0.02 0.3 0.2" material="fabric">
  <array count="8" translation="0.03 0 0"/>
</box>

<!-- Spiral staircase: 12 steps, each raised 0.2 in Y and rotated 15° around Y -->
<box size="0.8 0.05 0.3" material="wood">
  <array count="12" translation="0 0.2 0" rotation="0 15 0"/>
</box>
```

## Prefabs

Prefabs are reusable object definitions stored in `prefabs/name.xml`. Each prefab file begins with an XML comment describing its default orientation (which way it "faces" at `rot="0 0 0"`). Prefabs are loaded lazily (on first reference) and cached for reuse.

Prefabs may contain point lights. Each instance creates its own transformed
light, allowing one fixture definition to keep its cord, shade, bulb, and light
source aligned under translation, rotation, and scale.

**Orientation convention:** every prefab that has a natural "front" (e.g. chair, sofa) must document which direction it faces at default rotation. Use the rotation convention above to place and orient prefabs.

### `<prefab>`

Instantiate a prefab. `pos`, `rot`, `scale`, `pivotOffset`, and `attach` apply
to the complete instance. Instance `color` is a selective tint as described
below. Prefab-wide `material`, `shininess`, `castShadow`, and `renderable`
inheritance are not currently implemented; define those on child shapes.

| Attribute | Type   | Default | Description |
|-----------|--------|---------|-------------|
| `source`  | string | (required) | Prefab file name, loads from `prefabs/<source>.xml` |
| `name`    | string | (none)     | Instance name for `attach` references — only needed when something attaches to this instance |

Prefab `scale` applies to its complete local transform, including named attach
points. A prefab-level `color` selectively replaces the diffuse color of
descendant shapes marked `tint="1"`; unmarked parts keep their own material or
color. Tinting does not replace material shininess. This lets one book prefab
produce different cover colors while its page block remains paper-colored:

```xml
<!-- prefabs/book.xml -->
<prefab>
  <box size="0.44 0.03 0.32" material="book_cover" tint="1"/>
  <box pos="0 0.06 0" size="0.39 0.06 0.28" material="paper"/>
</prefab>

<!-- scene -->
<prefab source="book" color="0.65 0.08 0.06"/>
<prefab source="book" pos="0.5 0 0" color="0.06 0.20 0.62"/>
```

Example: place 4 chairs around a dining table (table spans X=±0.8, Z=-1.0 to -2.0):

```xml
<prefab source="dining_table" name="dining_table" pos="0 0 -1.5"/>
<!-- chair faces +Z by default; rotY(180) = faces -Z (into table) -->
<prefab source="chair" pos="0 0 -0.75" rot="0 180 0"/>
<!-- chair at back: default +Z is already toward table -->
<prefab source="chair" pos="0 0 -2.25"/>
<!-- chair at left: rotY(90) maps +Z -> +X (toward table right) -->
<prefab source="chair" pos="-1.05 0 -1.5" rot="0 90 0"/>
<!-- chair at right: rotY(-90) maps +Z -> -X (toward table left) -->
<prefab source="chair" pos=" 1.05 0 -1.5" rot="0 -90 0"/>
```

### Attach points

Prefabs can declare named reference points using `<attach>` elements. These enable placing objects on surfaces without manual Y calculation.

**Defining attach points** — in the prefab file (`prefabs/dining_table.xml`):

```xml
<prefab>
  <attach name="center" pos="0 0.78 0"/>
  <attach name="edge_n" pos="0 0.78 -0.53"/>
  <attach name="edge_s" pos="0 0.78 0.53"/>
  <attach name="edge_e" pos="0.83 0.78 0"/>
  <attach name="edge_w" pos="-0.83 0.78 0"/>
  <box pos="0 0.75 0" size="1.6 0.06 1.0" material="wood"/>
  ...
</prefab>
```

Attach point positions are in the prefab's local coordinate space.

**Using attach points** — in any scene element via `attach="instanceName:slotName"`:

```xml
<prefab source="dining_table" name="dining_table" pos="0 0 -1.5"/>
<!-- Sphere placed on the table's center without calculating y=0.78 -->
<sphere attach="dining_table:center" radius="0.14" material="fabric"/>
```

When `attach` is present, the element's `pos` is replaced by the world-space position of the named attach point. Only the owning prefab instance needs a `name` attribute; the element using `attach` does not.

Prefab file `prefabs/chair.xml`:

```xml
<!-- chair: backrest at z=-0.2, front faces +Z. rotY(+90)->+X, rotY(-90)->-X -->
<prefab>
  <attach name="seat" pos="0 0.475 0"/>
  <box pos="0 0.45 0" size="0.45 0.05 0.45" material="wood"/>
  <box pos="0 0.75 -0.2" size="0.45 0.55 0.05" material="wood"/>
  <prism pos="-0.18 0.22 -0.18" radius="0.025" height="0.45" sides="4" material="wood"/>
  <prism pos=" 0.18 0.22 -0.18" radius="0.025" height="0.45" sides="4" material="wood"/>
  <prism pos="-0.18 0.22  0.18" radius="0.025" height="0.45" sides="4" material="wood"/>
  <prism pos=" 0.18 0.22  0.18" radius="0.025" height="0.45" sides="4" material="wood"/>
</prefab>
```

## Complete example

```xml
<scene>
  <camera name="Front" comment="Frontal view from outside the scene" pos="0 3 8" look="0 1.5 0" fov="55"/>
  <camera name="Back"  comment="Rear view from behind" pos="0 3 -8" look="0 1.5 0" fov="55"/>

  <ambient color="0.1 0.1 0.12"/>
  <background color="0.05 0.05 0.07"/>

  <material id="brass" color="0.8 0.7 0.2" shininess="40"/>
  <material id="steel" color="0.6 0.6 0.65" shininess="80"/>

  <light pos="2 4 2" color="1.0 0.9 0.8" intensity="1.2" castShadows="1"/>
  <light pos="-3 2 0" color="0.8 0.8 1.0" intensity="0.6" castShadows="0"/>

  <!-- twisted brass column -->
  <box pos="-2 1.5 0" size="0.5 3 0.5" material="brass">
    <twist angle="45"/>
  </box>

  <!-- tapered steel pedestal -->
  <cylinder pos="0 1 0" radius="0.6" height="2" material="steel">
    <taper amount="0.4"/>
  </cylinder>

  <!-- group with nested transforms -->
  <group pos="2 3 0" rot="0 30 0">
    <sphere radius="0.5" color="0.9 0.3 0.3"/>
    <box pos="0 -0.8 0" size="0.3 0.3 0.3" color="0.3 0.3 0.9"/>
  </group>
</scene>
```
