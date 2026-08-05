# AGENTS.md

## Build & Run

```sh
make              # Build the binary -> build/bin/simplegl
make test         # Build and run unit tests
make run          # Build and run with sample_room.xml
make clean        # Remove build/ directory
```

Dependencies: SDL2 (via pkg-config), OpenGL framework, libm. C11 standard, `-Wall -Wextra`.

## Run-time

```sh
./build/bin/simplegl scene.xml             # use first camera
./build/bin/simplegl scene.xml -cam Cam2   # select camera by name
```

## Project files

| File | Purpose |
|------|---------|
| `main.c` | Entry point, SDL2 window, FPS camera, game loop, `-cam` arg |
| `math.h` / `math.c` | `vec3`, `mat4`, linear algebra |
| `mesh.h` / `mesh.c` | `Mesh` (verts, tris, edges), primitive generators, **modifiers** (taper, twist, bend, stretch, skew) |
| `scene.h` / `scene.c` | Tiny XML parser, scene loading, named cameras, modifier dispatch |
| `render.h` / `render.c` | OpenGL 1.x fixed-function renderer with stencil shadows |
| `shadow.h` / `shadow.c` | Stencil shadow volume construction (silhouette detection + edge extrusion) |
| `tests.c` | Unit tests for mesh winding, edge sealing, volume, shadow volumes |
| `renderer.c` | Original monolithic single-file version (not in main build) |
| `screenshot.c` | Offscreen headless renderer, outputs PPM |
| `XML_SCENE.md` | Full XML scene format reference |

## Code conventions

- **No comments** unless absolutely necessary.
- Compact K&R brace style, tabs for indentation.
- `DA_PUSH` macro (from `math.h`) for all dynamic arrays.
- `vec3` and `mat4` are value types, passed and returned by value.
- All scene parsing uses dispatch tables: static arrays of `{ tag, parser_function }` to avoid `if/else` chains.
- Forward declarations are used sparingly, only when call order requires them.

## Scene file dispatch

Scene loading is in `scene.c`. Three dispatch tables:

1. **`scene_tags[]`** — top-level scene config tags (`camera`, `ambient`, `background`, `material`, `light`). Each has a `parse_*_tag(Scene*, XmlNode*)` function.
2. **`shape_parsers[]`** — primitive shapes (`box`, `sphere`, `cylinder`, `prism`, `cone`, `pyramid`, `torus`) plus `group` and `wall`. Each has a `parse_*(Scene*, XmlNode*, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow)` function.
3. **`modifier_parsers[]`** — mesh modifiers (`taper`, `twist`, `bend`, `stretch`, `skew`) applied as child elements of shape nodes. Each has a `parse_mod_*(Mesh*, XmlNode*)` function.

To add a new primitive:
1. Add a `static void parse_newprim(...)` function in `scene.c`.
2. Add `{ "newprim", parse_newprim }` to the `shape_parsers[]` array.
3. Generate the mesh (returning a `Mesh`), call `apply_modifiers(&mesh, n)`, then `scene_add_obj()`.

To add a new modifier:
1. Add a `mesh_apply_*(Mesh*, ...)` function in `mesh.h`/`mesh.c`.
2. Add a `parse_mod_*(Mesh*, XmlNode*)` wrapper in `scene.c`.
3. Add `{ "tag", parse_mod_* }` to the `modifier_parsers[]` array.

Common attributes (`pos`, `rot`, `scale`, `color`, `shininess`, `material`, `castShadow`) are extracted before dispatch, so individual shape parsers only need to read shape-specific attributes.
