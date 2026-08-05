# XML Scene Format

SimpleGL reads scenes from XML files. The root node is `<scene>`, containing a mix of config tags and shape objects.

## Quick example

```xml
<scene>
  <camera name="Main" pos="0 2 6" look="0 1 0" fov="60"/>
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

Defines a viewpoint. Multiple cameras allowed; select via `-cam Name` on the command line.

| Attribute | Type   | Default | Description |
|-----------|--------|---------|-------------|
| `name`    | string | Camera1 | Camera identifier |
| `pos`     | vec3   | 0 1.6 5 | Eye position |
| `look`    | vec3   | 0 1.2 0 | Look-at target |
| `fov`     | float  | 60      | Vertical FOV in degrees |

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
| `rot`        | vec3   | 0 0 0            | Euler rotation (degrees, X→Y→Z order) |
| `scale`      | vec3   | 1 1 1            | Non-uniform scale (ignored by `<wall>`) |
| `material`   | string | (none)           | Reference to a `<material id="...">` |
| `color`      | vec3   | 0.8 0.8 0.8      | Diffuse colour (used if no material ref) |
| `shininess`  | float  | 8.0              | Specular exponent (used if no material ref) |
| `castShadow` | int    | 1                | Whether this object casts shadows |

If a `material` attribute is given, `color` and `shininess` are taken from the referenced material definition.

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

A flat wall with rectangular openings (doors/windows). Uses "boolean via boxes" — the wall is split into box segments around openings. Walls ignore `scale` and `castShadow`.

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

All modifiers operate relative to the object's Y-axis bounding box. The Y range [minY, maxY] is mapped to [0, 1].

### `<taper>`

Scales X and Z proportionally along Y.

| Attribute    | Type  | Default | Description |
|--------------|-------|---------|-------------|
| `amount`     | float | 0.0     | Taper intensity (± values, positive = narrow top) |
| `curvature`  | float | 1.0     | Non-linearity exponent (1 = linear, >1 = bowed, <1 = pinched) |

```xml
<box size="2 3 2">
  <taper amount="0.4"/>
</box>
```

### `<twist>`

Rotates the mesh around the Y axis proportionally to Y position.

| Attribute | Type  | Default | Description |
|-----------|-------|---------|-------------|
| `angle`   | float | 0.0     | Total twist angle in degrees |

```xml
<box size="2 4 2">
  <twist angle="45"/>
</box>
```

### `<bend>`

Bends the mesh into a circular arc. The Y axis of the mesh becomes an arc in the XZ? no, XY plane.

| Attribute | Type  | Default | Description |
|-----------|-------|---------|-------------|
| `angle`   | float | 0.0     | Bend angle in degrees |

```xml
<cylinder radius="0.2" height="3">
  <bend angle="90"/>
</cylinder>
```

### `<stretch>`

Non-linear stretch/squash along Y: pinches the middle, expands the ends (or vice versa).

| Attribute  | Type  | Default | Description |
|------------|-------|---------|-------------|
| `amount`   | float | 0.0     | Stretch amount (positive = squeeze middle) |
| `amplify`  | float | 1.0     | How much XZ stretches to match (1.0 for volume preservation-like) |

```xml
<sphere radius="0.5">
  <stretch amount="0.5" amplify="0.5"/>
</sphere>
```

### `<skew>`

Shears the mesh: shifts X and Z proportionally to Y position.

| Attribute | Type  | Default | Description |
|-----------|-------|---------|-------------|
| `amount`  | float | 0.0     | Skew intensity (positive = top shifts right) |

```xml
<box size="1 3 1">
  <skew amount="0.5"/>
</box>
```

## Multiple modifiers stacking

Modifiers are applied sequentially, each operating on the mesh produced by the previous:

```xml
<box size="1 4 1">
  <taper amount="0.3"/>
  <twist angle="60"/>
  <bend angle="30"/>
</box>
```

## Complete example

```xml
<scene>
  <camera name="Front" pos="0 3 8" look="0 1.5 0" fov="55"/>
  <camera name="Back"  pos="0 3 -8" look="0 1.5 0" fov="55"/>

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
