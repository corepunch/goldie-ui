# ARCHITECTURE.md

## Overview

SimpleGL is a pure-C stencil-shadow demo renderer: a single `.exe` reads an XML scene description, parses it into a geometry database, then renders it in real-time using OpenGL 1.x fixed-function with stencil-buffer shadow volumes.

## Modules

```
main.c ──► scene.c ──► shadow.c ──► render.c
  │            │            │
  ▼            ▼            ▼
math.c      mesh.c       mesh.c
```

### math.c — Linear algebra

Provides `vec3` (3D vector) and `mat4` (4x4 column-major matrix). Core operations:

- **vec3**: add, sub, scale, mul, dot, cross, len, norm
- **mat4**: identity, mul, translate, scale, rot_x/y/z, rot_xyz (Euler, X→Y→Z order), perspective, lookat, xform_point, xform_dir
- **DA_PUSH**: generic dynamic-array push macro (`DA_PUSH(arr, len, cap, value)`).

All types are value types — passed and returned by value on the stack.

### mesh.c — Geometry primitives

Defines `Mesh` as a triangle soup: `Vertex*` (position + normal), `Tri*` (3 vertex indices), `Edge*` (2 vertex indices + triangle refs t0/t1). Supplied primitives:

| Function | Shape |
|----------|-------|
| `gen_box(w,h,d)` | Axis-aligned box centered at origin |
| `gen_sphere(r, rings, slices)` | UV sphere |
| `gen_cylinder(r, h, sides)` | Closed cylinder along Y |
| `gen_prism(r, h, sides)` | Regular N-gonal prism along Y |
| `gen_cone(rb, rt, h, sides)` | Truncated cone/frustum along Y |
| `gen_torus(R, r, majSeg, minSeg)` | Torus in XZ plane |

Mesh utilities: `mesh_transform` (applies model + rotation matrix), `mesh_compute_face_normals` (cross-product per-triangle, face-averaged per-vertex), `mesh_build_edges` (for shadow volume silhouette detection), `mesh_flip_winding` (fixes inside-out geometry), `mesh_signed_volume` (winding consistency check).

**Modifiers** — 5 mesh deformation functions applied to local-space vertices before transform:

| Function | Effect |
|----------|--------|
| `mesh_apply_taper(m, amount, curvature)` | Scale X,Z non-uniformly along Y |
| `mesh_apply_twist(m, angle_deg)` | Rotate around Y proportional to Y |
| `mesh_apply_bend(m, angle_deg)` | Map Y axis into circular arc in XY plane |
| `mesh_apply_stretch(m, amount, amplify)` | Non-linear squash/stretch along Y |
| `mesh_apply_skew(m, amount)` | Shear X,Z proportional to Y |

All modifiers compute the mesh Y bounding box, map each vertex y to [0,1], then apply the deformation.

### scene.c — XML parser + scene loader

**Tiny XML parser** (no external deps): `XmlNode` tree with `XmlAttr` key-value pairs. Handles tags, attributes with quoted values, comments, self-closing tags, and text content (ignored). Parser is recursive-descent operating on a `const char**` pointer.

**Scene data model** (`scene.h`):
- `Camera` — named viewpoint with position, look-at target, FOV
- `Material` — named material with RGB color and Phong shininess
- `Light` — point light with position, color, intensity, shadow-casting flag
- `SceneObj` — a transformed `Mesh` with resolved material/color, shininess, shadow flag
- `Scene` — aggregates: active camera (convenience fields), `Camera*` array, ambient color, background color, dynamic arrays of lights/materials/objects, plus one `ShadowVolume` per light

**Loading flow:**
1. `read_file()` reads the entire XML into a null-terminated buffer.
2. `xml_parse()` builds the `XmlNode` tree.
3. `load_scene()` iterates root children through `scene_tags[]` dispatch table for `camera`, `ambient`, `background`, `material`, `light`.
4. `parse_nodes()` iterates root children through `shape_parsers[]` dispatch table for `box`, `sphere`, `cylinder`, `prism`, `cone`, `pyramid`, `torus`, `group`, `wall`.
5. Each shape parser reads shape-specific attributes, generates a `Mesh`, calls `apply_modifiers()` to process any child `<taper>`, `<twist>`, `<bend>`, `<stretch>`, `<skew>` elements, then calls `scene_add_obj()` to transform it, fix winding, compute normals, and build edges.

**Dispatch tables** use static C arrays of `{ "tagname", parser_function }` to eliminate if-else chains:
```c
static const struct { const char *tag; shape_parser_fn parse; } shape_parsers[] = {
    { "box",      parse_box },
    { "sphere",   parse_sphere },
    ...
};
static const struct { const char *tag; modifier_parser_fn parse; } modifier_parsers[] = {
    { "taper",   parse_mod_taper },
    { "twist",   parse_mod_twist },
    ...
};
```

**Multiple cameras:** Each `<camera>` tag is stored in a `Camera*` array with a `name` attribute. `scene_select_camera()` chooses the active camera. Command-line `-cam Name` calls this after loading. If no cameras are defined, a default "Camera1" is created.

**Group support:** `parse_group` calls `parse_nodes` recursively, passing its accumulated `M` (model matrix with scale) and `R` (rotation-only matrix for normals). This enables nested coordinate-space hierarchies.

**Wall support:** `parse_wall` handles a `<wall>` tag with child `<opening>` tags. It performs "boolean via boxes" — the wall length axis is partitioned at opening boundaries, and each segment produces up to 3 rectangular boxes (below sill, above opening, above wall). Walls ignore scale and never cast shadows.

### shadow.c — Stencil shadow volumes

Implements the classic vertex-shader-free algorithm:
1. For each triangle edge: classify as silhouette edge if one adjacent face faces the light and the other faces away.
2. Extrude silhouette edges away from the light: `extruded_vertex = lightDir * large_distance`.
3. Store extruded quads (2 triangles each) as triangle list in `ShadowVolume.verts`.

`scene_build_all_shadow_volumes()` builds a shadow volume per light for every shadow-casting object.

### render.c — OpenGL rasterizer

Uses OpenGL 1.x fixed-function pipeline (no shaders). Rendering passes:

1. **Ambient pass**: Enable depth-write, disable stencil-write. Draw all objects with ambient color.
2. **Per-light pass** (for each shadow-casting light):
   - Clear stencil buffer to 0.
   - **Shadow volume pass (Z-fail)**: Disable color writes, disable depth-write, set stencil op to increment on Z-fail for front faces and decrement on Z-fail for back faces. Render shadow volume geometry.
   - **Lighting pass**: Enable additive blending, depth-test EQUAL, stencil-test EQUAL to 0. Draw all objects with diffuse + specular using point-light math via `glColor`.
3. **Debug modes**: wireframe shadow volumes (red), stencil != 0 overlay (red).

### main.c — Application loop

SDL2 window + OpenGL 2.1 context. FPS-style camera with mouse-look and WASD movement. Frame loop: poll SDL events, update camera from key states, `render_frame()`.

## Data flow

```
XML file
  │
  ▼
scene.c: load_scene() ──► Scene (lights[], objs[], mats[])
  │
  ▼
shadow.c: scene_build_all_shadow_volumes() ──► ShadowVolume per light
  │
  ▼
render.c: render_frame(scene, w, h, proj, view)
  │
  ▼
OpenGL framebuffer ──► SDL_GL_SwapWindow
```

## Adding features

- **New primitive shape**: Add `gen_*` to `mesh.c`/`mesh.h`, add `parse_*` to `shape_parsers[]` in `scene.c`.
- **New scene tag**: Add `parse_*_tag` to `scene_tags[]` in `scene.c`.
- **New render pass**: Modify `render_frame()` in `render.c`.
- **New shadow technique**: Modify `build_shadow_volume()` in `shadow.c`.
