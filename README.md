# simplegl

A very small OpenGL scene renderer, one C file, with real-time **stencil
shadow volumes**. It reads a room/scene description from an XML file (so an
LLM can just generate the XML) and draws it: walls, furniture, and basic
primitives, lit and shadowed by a point light.

Everything lives in **`renderer.c`** — no external XML library, no shader
files, no asset pipeline. Dependencies: SDL2 (window/GL context/input) and
your system's OpenGL. That's it.

## Build & run

```sh
# Debian/Ubuntu:
sudo apt install libsdl2-dev libgl1-mesa-dev

make
./simplegl sample_room.xml
```

or directly:

```sh
gcc -O2 renderer.c -o simplegl $(pkg-config --cflags --libs sdl2) -lGL -lm
```

It compiles clean with `-Wall -Wextra` on gcc/clang, Linux. (It uses only
OpenGL 2.1 fixed-function calls plus `glStencilOpSeparate`, so it should also
run on macOS's legacy GL / most Windows drivers with minimal changes to the
SDL attribute requests — not tested there.)

**Controls:** mouse looks around (captured on start), `W A S D` move,
`Q`/`E` down/up, `Shift` to move faster, `Esc` to quit.

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

1. **Ambient pass** – draw everything flat-shaded at `ambient * color`, with
   depth writes on. This seeds the depth buffer and the base (unlit) look.
2. For each shadow-casting light:
   a. **Stencil pass** – draw the light's shadow volume (silhouette side
      quads + near/far caps) with color writes off, depth writes off,
      `glStencilOpSeparate`: back faces `INCR_WRAP` / front faces
      `DECR_WRAP` on depth-fail. This is the part that's still correct even
      when the camera itself is inside a shadow.
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

## XML scene schema

```xml
<scene>
  <camera pos="0 1.6 5" look="0 1 0" fov="60"/>
  <ambient color="0.12 0.12 0.14"/>
  <background color="0.05 0.06 0.08"/>

  <light pos="0 3 0" color="1 1 1" intensity="1.0" castShadows="1"/>

  <material id="wood" color="0.5 0.32 0.18" shininess="20"/>

  <!-- primitives: pos / rot (xyz degrees) / scale, material="id" or color="r g b" -->
  <box      pos="0 0.5 0" size="1 1 1" material="wood"/>
  <sphere   pos="0 1 0" radius="0.5" rings="16" slices="24" material="wood"/>
  <cylinder pos="0 0.5 0" radius="0.3" height="1" sides="24" material="wood"/>
  <prism    pos="0 0.5 0" radius="0.3" height="1" sides="6" material="wood"/>
  <cone     pos="0 0.5 0" radius="0.5" radiusTop="0" height="1" sides="24" material="wood"/>
  <pyramid  pos="0 0.5 0" radius="0.5" height="1" material="wood"/> <!-- cone, sides=4 -->
  <torus    pos="0 1 0" majorRadius="0.5" minorRadius="0.15" material="wood"/>

  <!-- wall built from boxes, with punched openings (the "boolean") -->
  <wall pos="0 0 -3" length="6" height="2.8" thickness="0.2" material="wall">
    <opening type="door"   x="2" width="1.0" height="2.1"/>
    <opening type="window" x="4" width="1.2" height="1.2" sill="0.9"/>
  </wall>

  <!-- group: composes furniture from primitives under one transform -->
  <group pos="2 0 1" rot="0 30 0">
    <box .../> <box .../> <cylinder .../>
  </group>
</scene>
```

Notes:
- Every primitive/`group`/`wall` accepts `pos="x y z"`, `rot="rx ry rz"`
  (degrees, applied X then Y then Z), and (except `wall`) `scale="x y z"`.
- `material="id"` looks up a `<material>`; or skip it and set `color="r g b"`
  / `shininess="n"` directly on the element.
- `castShadow="0"` on any primitive excludes it from shadow-casting (useful
  for the floor/glass panes). `castShadows="0"` on `<light>` makes that
  light unconditionally additive with no shadow test.
- `<wall>`'s local X axis is its length, Y is height (0 = floor at the
  wall's `pos`), Z is thickness, centered. `<opening x=".." width="..">`
  positions the opening along that length axis. `type="door"` defaults
  `sill=0`; `type="window"` defaults `sill=0.9`.

`sample_room.xml` is a worked example: four walls (two with openings), a
floor, a glass pane in the window, a sofa/coffee table/dining chair built
from boxes/cylinders/prisms, and a torus/sphere/pyramid as decoration —
enough to see the whole feature set at once, including a shadow cast by
the sofa onto the floor.

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
