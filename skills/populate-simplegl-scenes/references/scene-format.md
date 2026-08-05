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

| Attribute | Type | Default           | Description |
|-----------|------|-------------------|-------------|
| `color`   | vec3 | 0.05 0.06 0.08    | Clear/background colour |

### `<material>`

Named material referenced by shapes via the `material` attribute.

| Attribute    | Type   | Default        | Description |
|--------------|--------|----------------|-------------|
| `id`         | string | (required)     | Unique material name |
| `color`      | vec3   | 0.8 0.8 0.8    | Diffuse RGB colour |
| `shininess`  | float  | 8.0            | Phong specular exponent |

### `<light>`

Point light source.

| Attribute    | Type | Default | Description |
|--------------|------|---------|-------------|
| `pos`        | vec3 | 0 3 0   | Light position |
| `color`      | vec3 | 1 1 1   | Light RGB colour |
| `intensity`  | float| 1.0     | Brightness multiplier |
| `castShadows`| int  | 1       | Whether this light casts stencil shadows |

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

If a `material` attribute is given, `color` and `shininess` are taken from the referenced material definition.

**Rotation convention:** `rot="rx ry rz"` applies rotations around X, then Y, then Z. Positive Y rotation maps the **+X** axis toward **-Z** and **+Z** toward **+X**.

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

All modifiers operate relative to the object's bounding box along the chosen axis. The axis range [min, max] is mapped to [0, 1].

Each modifier accepts `axis="y"` (`x`, `y`, or `z`), defaulting to `"y"`.

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

## Prefabs

Prefabs are reusable object definitions stored in `prefabs/name.xml`. Each prefab file begins with an XML comment describing its default orientation (which way it "faces" at `rot="0 0 0"`). Prefabs are loaded lazily (on first reference) and cached for reuse.

**Orientation convention:** every prefab that has a natural "front" (e.g. chair, sofa) must document which direction it faces at default rotation. Use the rotation convention above to place and orient prefabs.

### `<prefab>`

Instantiate a prefab by reference. Common attributes (`pos`, `rot`, `scale`, `material`, `color`, `shininess`, `castShadow`) apply as usual.

| Attribute | Type   | Default | Description |
|-----------|--------|---------|-------------|
| `ref`     | string | (none)  | Prefab name, loads from `prefabs/name.xml` |

Example: place 4 chairs around a dining table (table spans X=±0.8, Z=-1.0 to -2.0):

```xml
<prefab ref="dining_table" pos="0 0 -1.5"/>
<!-- chair faces +Z by default; rotY(180) = faces -Z (into table) -->
<prefab ref="chair" pos="0 0 -0.75" rot="0 180 0"/>
<!-- chair at back: default +Z is already toward table -->
<prefab ref="chair" pos="0 0 -2.25"/>
<!-- chair at left: rotY(90) maps +Z -> +X (toward table right) -->
<prefab ref="chair" pos="-1.05 0 -1.5" rot="0 90 0"/>
<!-- chair at right: rotY(-90) maps +Z -> -X (toward table left) -->
<prefab ref="chair" pos=" 1.05 0 -1.5" rot="0 -90 0"/>
```

Prefab file `prefabs/chair.xml`:

```xml
<!-- chair: backrest at z=-0.2, front faces +Z. rotY(+90)->+X, rotY(-90)->-X -->
<prefab>
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
