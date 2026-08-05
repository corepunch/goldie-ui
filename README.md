# simplegl

A very small modular OpenGL scene renderer with real-time **stencil shadow
volumes**. Shadows are a first-class composition tool: position lights and
casters to create long silhouettes, pools of light, strong contrast, and
dramatic architectural shots. Scenes are described in XML and built from
walls, furniture, and basic primitives.

The active build uses small C modules with their declarations consolidated in
**`simplegl.h`**. There is no external XML library, shader file, or asset
pipeline. Dependencies are SDL2, OpenGL, and libm.

## Build & run

```sh
# Debian/Ubuntu:
sudo apt install libsdl2-dev libgl1-mesa-dev

make
./build/bin/simplegl scenes/sample_room.xml
```

It compiles clean with `-Wall -Wextra` on gcc/clang, Linux. (It uses only
OpenGL 2.1 fixed-function calls plus `glStencilOpSeparate`, so it should also
run on macOS's legacy GL / most Windows drivers with minimal changes to the
SDL attribute requests — not tested there.)

**Controls:** mouse looks around, `W A S D` move, `Q`/`E` move down/up,
`Shift` moves faster, and `Esc` quits. Press `1` for normal shadows, `4` for
lighting without shadows, or `5` for white wireframe. Keys `2` and `3` show
the shadow-volume diagnostics.

## Reusable prefab variation

Prefab instances support complete `pos`, `rot`, `scale`, and `pivotOffset`
transforms. Their named attach points follow the same transform. A prefab may
mark selected child shapes with `tint="1"`; `color` on an instance then
replaces only those shapes' diffuse color while preserving their material
shininess and leaving unmarked parts unchanged.

```xml
<!-- The book prefab marks covers tintable, but not its paper page block. -->
<prefab source="book" color="0.58 0.08 0.06"/>
<prefab source="book" pos="0.5 0 0" color="0.08 0.22 0.56"/>
```

Important authoring features still missing are named multi-color material
slots, per-camera visibility or state variants, animation, object/layer names
for CLI inspection, orthographic and aspect-safe cameras, textures and alpha,
and area lights or soft shadows. Prefab-wide `material`, `shininess`,
`castShadow`, and `renderable` inheritance are also not implemented; child
shapes continue to own those properties.

## Rendering modes

Normal rendering uses every light's `castShadows` setting and produces the
full stencil-shadow result:

```sh
./build/bin/simplegl scenes/sample_room.xml
```

Render with the same materials and lighting but disable all shadows:

```sh
./build/bin/simplegl scenes/sample_room.xml -no-shadows
```

Render scene geometry as an unlit white wireframe:

```sh
./build/bin/simplegl scenes/sample_room.xml -wireframe
```

The offscreen screenshot tool accepts the same flags:

```sh
make screenshot
./build/bin/screenshot scenes/sample_room.xml -cam Main -o shot.ppm
./build/bin/screenshot scenes/sample_room.xml -cam Main -no-shadows -o shot-no-shadows.ppm
./build/bin/screenshot scenes/sample_room.xml -cam Main -wireframe -o shot-wireframe.ppm
```

## From scene layout to cinematic overpaint

[`scenes/eclipse_shrine.xml`](scenes/eclipse_shrine.xml) is a compact
science-fantasy set built entirely from SimpleGL primitives. A stepped stone
dais leads to a brass eclipse ring and suspended red orb. Four twisted pylons,
two entrance obelisks, and a broken rectangular gate make the scene readable
from more than one camera, while the same sun and point light keep its long
shadows spatially consistent.

![Top-down wireframe overview of the Eclipse Shrine layout](docs/images/eclipse-shrine-overview.png)

The diagram is the `Overview` camera in white-wireframe mode. The entrance is
at the bottom, the eclipse assembly is centered on the dais, and the rear gate
frames it from both rendered viewpoints.

The comparisons below use the untouched SimpleGL output as an image-to-image
guide for an AI overpaint. Material detail and atmosphere are added, but the
camera, silhouettes, object placement, occlusion, lighting direction, and
major cast-shadow shapes are constrained by the source render.

| SimpleGL camera render | AI overpaint |
|---|---|
| ![Raw Approach camera render](docs/images/eclipse-shrine-approach.png) | ![AI overpaint of the Approach camera render](docs/images/eclipse-shrine-approach-overpaint.jpg) |
| `Approach` — centered hero shot from the entrance | The ring, orb, pylons, obelisks, stairs, gate, and shadow directions remain anchored to the render. |

| SimpleGL camera render | AI overpaint |
|---|---|
| ![Raw Oblique camera render](docs/images/eclipse-shrine-oblique.png) | ![AI overpaint of the Oblique camera render](docs/images/eclipse-shrine-oblique-overpaint.jpg) |
| `Oblique` — three-quarter view across the platform | The same landmarks and lighting relationships survive the camera change, demonstrating multi-shot scene continuity. |

Reproduce the three source images with:

```sh
make screenshot
./build/bin/screenshot scenes/eclipse_shrine.xml -cam Overview -wireframe -o overview.ppm
./build/bin/screenshot scenes/eclipse_shrine.xml -cam Approach -o approach.ppm
./build/bin/screenshot scenes/eclipse_shrine.xml -cam Oblique -o oblique.ppm
```

## Why these choices

- **Fixed-function GL, immediate mode (`glBegin`/`glEnd`).** No shaders to
  write or ship. `GL_LIGHTING` gives ambient/diffuse/specular for free, and
  the stencil-buffer ops we need for shadows are core GL 2.0. This keeps the
  entire renderer + shadow algorithm in one readable file.
- **Own tiny XML parser**, not tinyxml/libxml. Pulling in libxml2 means
  linking a large C library (and its dependency chain) for a schema that's
  really just `<tag attr="val">`. tinyxml2 is closer to reasonable, but it's
  C++ and still an extra file/dependency to vendor. ~150 lines of recursive-
  descent C parsing (elements, quoted attributes, nesting, self-closing
  tags, comments) covers the whole schema below and keeps the "single file,
  no deps but SDL2" property. If your scenes get much more complex you'd
  want to swap in a real parser — the loader code is isolated in
  `xml_parse()`/`load_scene()` so that's a contained change.
- **Booleans via boxes**, not real CSG. A proper solid-boolean library (BSP
  clipping, etc.) is a lot of code for what a room generator actually needs:
  rectangular holes in axis-aligned walls. So `<wall>` + `<opening>` doesn't
  do a boolean subtraction at all — it slices the wall's length into
  segments at each opening's edges and emits a handful of `<box>` objects:
  full-height boxes between openings, and a sill box + lintel box for each
  opening. See `build_wall_boxes()`. This is exactly "if forced to use boxes
  for booleans, so be it" — and it's enough for door/window frames, which is
  the actual use case. If you later need e.g. a circular hole or a boolean
  between two arbitrary meshes, that's a genuinely different (and much
  bigger) piece of code — worth doing as a separate module, not bolted on
  here.
- **Static scene ⇒ shadow volumes precomputed once.** Nothing in the scene
  format can move, so silhouette/volume computation happens once at load
  (`scene_build_all_shadow_volumes`), not per frame. If you add moving
  lights or objects later, call `build_shadow_volume()` again per-frame for
  whatever changed — the function is already factored out for that; only
  `main()`'s "compute once" call site needs to move into the render loop.

## Shadow algorithm

Classic robust **z-fail (Carmack's Reverse)** stencil shadow volumes, one
point light per pass:

The side-only, infinitely extruded Z-pass implementation is available with
`make zpass`, `make run-zpass`, and `make test-zpass`. These targets compile
with `USE_ZPASS` and write separate `*-zpass` binaries under `build/bin`.

1. **Ambient pass** – draw everything flat-shaded at `ambient * color`, with
   depth writes on. This seeds the depth buffer and the base (unlit) look.
2. For each shadow-casting light:
   a. **Stencil pass** – draw the light's closed shadow volume (silhouette
      side quads + light/dark caps) with color writes off, depth writes off,
      `glStencilOpSeparate`: back faces `INCR_WRAP` / front faces
      `DECR_WRAP` on depth-fail. This is the part that's still correct even
      when the camera itself is inside a shadow.
      Extruded vertices use homogeneous directions (`w=0`), and depth clamp
      prevents near/far clipping. If depth clamp is unavailable, stencil
      shadows are disabled and the lights render without shadows.
   b. **Lit pass** – redraw the scene fully lit (diffuse+specular, ambient
      zeroed to avoid double-counting), additively blended (`GL_ONE,
      GL_ONE`), only where the stencil value is `0` (i.e. *not* in shadow).
3. Non-shadow-casting lights just get an unconditional additive lit pass, no
   stencil test.

Silhouette detection needs a *flat* face normal per triangle
(`Mesh.triN`, computed straight from triangle vertex positions) — this is
kept separate from the shading normal (`Vertex.nrm`, smooth for
sphere/torus, per-facet for box/prism/cone) so the shadow math never cares
how a mesh happens to be shaded.

## XML scene authoring

Scene construction, placement conventions, prefab rules, the complete XML
schema, and required CLI validation live in the
[`populate-simplegl-scenes`](skills/populate-simplegl-scenes/SKILL.md) skill.
Use that skill whenever creating or changing scene and prefab XML files.

## Known limitations (all fixable, kept out on purpose for scope)

- One shadow-casting point light is exercised in the sample and is what the
  algorithm is written for; multiple lights work (the render loop already
  iterates `<light>` entries) but cost is O(lights × objects) shadow-volume
  triangles per frame relatively fast since they're precomputed once.
- Non-uniform scale (`scale="1 1 3"` on a rotated object) will slightly
  distort shading normals — normals use the rotation part of the transform
  only, not a full inverse-transpose. Fine for boxes/furniture; would matter
  for a heavily stretched sphere.
- No texturing — flat/vertex colors only.
- `<wall>` openings are axis-aligned rectangles only (that's the whole
  "boxes instead of real CSG" trade-off described above).
