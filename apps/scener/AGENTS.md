# AGENTS.md

## Build & Run

```sh
make scener                            # Build -> build/bin/scener
make build/bin/test_scener_input_test  # Build the focused Scener test
build/bin/test_scener_input_test       # Run the focused Scener test
```

Scener is built from the Orion repository root and uses Orion's platform,
windowing, controls, and rendering libraries. C11 standard, `-Wall -Wextra`.

## Run-time

```sh
./build/bin/scener apps/scener/scenes/sample_room.blks
./build/bin/scener --render apps/scener/scenes/sample_room.blks --camera Cam2 --output-dir render
```

## Project files

| File | Purpose |
|------|---------|
| `main.c` | Orion app lifecycle, CLI parsing, document startup, screenshot output |
| `simplegl.h` | Shared declarations for all modules |
| `math.c` | `vec3`, `mat4`, linear algebra |
| `mesh.c` | `Mesh` (verts, tris, edges), primitive generators, **modifiers** (taper, twist, bend, stretch, skew) |
| `scene.c` | Tiny XML parser, scene loading, named cameras, modifier dispatch, **prefab loading** |
| `render.c` | OpenGL 1.x fixed-function renderer with stencil shadows |
| `shadow.c` | Stencil shadow volume construction (silhouette detection + edge extrusion) |
| `tests/scener_input_test.c` | Focused command-state and prefab-document tests |
| `skills/populate-simplegl-scenes/` | Scene population workflow and format reference |
| `scenes/` | Runnable and diagnostic scene files (`*.blks`) |
| `prefabs/` | Reusable object files (`chair.blk`, `sofa.blk`, etc.) |

## Scene XML authoring

[`skills/populate-simplegl-scenes/references/scene-format.md`](skills/populate-simplegl-scenes/references/scene-format.md)
is the canonical reference for `.blks` scene roots, `.blk` prefab roots, every
supported element and attribute, defaults, units, and CLI validation. Read it
before editing scene XML. Follow
[`skills/populate-simplegl-scenes/SKILL.md`](skills/populate-simplegl-scenes/SKILL.md)
for placement, composition, lighting, camera, and rendered-review requirements.

After editing a scene or referenced prefab:

```sh
xmllint --noout apps/scener/scenes/sample_room.blks
make scener
./build/bin/scener --list-cameras apps/scener/scenes/sample_room.blks
./build/bin/scener --render apps/scener/scenes/sample_room.blks --output-dir render
```

## Code conventions

- **No comments** unless absolutely necessary. When a design choice is non-obvious (e.g. why a timer is created on demand, why a specific constant value was chosen, why a particular algorithm was used), document it with a short inline comment. Do not comment the *what* — comment the *why*.
- Compact K&R brace style, tabs for indentation.
- `DA_PUSH` macro (from `simplegl.h`) for all dynamic arrays.
- `vec3` and `mat4` are value types, passed and returned by value.
- All scene parsing uses dispatch tables: static arrays of `{ tag, parser_function }` to avoid `if/else` chains.
- Forward declarations are used sparingly, only when call order requires them.
- **No magic numbers.** Extract all numeric constants to `#define` at the top of the file. Use descriptive names (e.g. `ORBIT_BASE_SENSITIVITY`, `EXTRUDE_DISTANCE`, `WELD_THRESHOLD`). The only exceptions are `0`, `1`, `-1`, and `2` in trivial contexts (loop bounds, signs, identity values).

## Scene file dispatch

Use `skills/populate-simplegl-scenes/SKILL.md` for any task that creates,
populates, edits, or validates scene or prefab files. Follow its CLI validation
requirements.

Scene loading is in `scene.c`. Three dispatch tables:

1. **`scene_tags[]`** — top-level scene config tags (`camera`, `material`, `sun`). Each has a `parse_*_tag(Scene*, XmlNode*)` function. `ambient` and `background` are `<scene>` attributes (`<scene ambient="..." background="...">`).
2. **`shape_parsers[]`** — transformable scene content: primitive shapes (`box`, `sphere`, `cylinder`, `prism`, `cone`, `pyramid`, `torus`), point `light`, `group`, `prefab`, and `wall`. This lets point lights inherit group and prefab transforms.
3. **`modifier_parsers[]`** — mesh modifiers (`taper`, `twist`, `bend`, `stretch`, `skew`) applied as child elements of shape nodes. Each has a `parse_mod_*(Mesh*, XmlNode*)` function.

`bool-negative-box` is handled by a prepass rather than `shape_parsers[]`. The
prepass expands groups and prefabs before wall construction; each aligned box
that fully crosses a wall becomes a rectangular opening in that wall. It is
wall-opening metadata, not general mesh CSG.

To add a new primitive:
1. Add a `static void parse_newprim(...)` function in `scene.c`.
2. Add `{ "newprim", parse_newprim }` to the `shape_parsers[]` array.
3. Generate the mesh (returning a `Mesh`), call `apply_modifiers(&mesh, n)`, then `scene_add_obj()`.

To add a new modifier:
1. Add a `mesh_apply_*(Mesh*, ...)` declaration in `simplegl.h` and its implementation in `mesh.c`.
2. Add a `parse_mod_*(Mesh*, XmlNode*)` wrapper in `scene.c`.
3. Add `{ "tag", parse_mod_* }` to the `modifier_parsers[]` array.

Common attributes (`pos`, `rot`, `scale`, `color`, `shininess`, `material`, `castShadow`, `renderable`, `unlit`) are extracted before dispatch, so individual shape parsers only need to read shape-specific attributes.
