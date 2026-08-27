## Conclusion

No—the executable is currently using classic **Z-pass**, not Carmack’s reverse.

That fully explains the inversion when the camera approaches or enters a shadow volume. The README and architecture documentation describe Z-fail, but they are stale.

### What the active code does

The binary is built from the modular sources, including `render.c` and `shadow.c`, according to [Makefile](/Users/igor/Developer/simplegl/Makefile:8). `renderer.c` is not compiled.

In [render.c](/Users/igor/Developer/simplegl/render.c:107), the code explicitly says Z-pass and configures:

```c
glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_KEEP, GL_INCR_WRAP);
glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
```

`glStencilOpSeparate` arguments are:

```text
face, stencil-fail, depth-fail, depth-pass
```

Therefore the increment/decrement operations are in the fourth position: they occur on **depth pass**.

Likewise, [shadow.c](/Users/igor/Developer/simplegl/shadow.c:21) generates only silhouette side quads:

```c
/* Side quads only — z-pass needs no caps */
```

There are no light/front or dark/far caps, so this geometry cannot be switched to Z-fail simply by changing the stencil operations.

This was an intentional regression/change in commit `a8bb49c`:

> “Switch shadow rendering to a z-pass (depth-pass) stencil method … and remove caps by generating side quads only.”

Meanwhile, [README.md](/Users/igor/Developer/simplegl/README.md:70) and [ARCHITECTURE.md](/Users/igor/Developer/simplegl/ARCHITECTURE.md:100) still claim the active renderer uses capped Z-fail.

The old, uncompiled [renderer.c](/Users/igor/Developer/simplegl/renderer.c:789) contains the previous depth-fail operations and [finite caps](/Users/igor/Developer/simplegl/renderer.c:692). It is closer to Carmack’s reverse, though not completely robust because its extrusion and far cap are finite.

## Why it flips—and why “half inside” is possible

Z-pass counts volume crossings from the camera/near plane toward each visible surface. It assumes every pixel ray starts outside all shadow volumes.

When the near clipping rectangle intersects a volume:

- Some pixel rays start outside the volume.
- Other pixel rays start inside it.
- The entry boundary for those inside rays was clipped away.
- Their stencil count has the wrong initial value.

So this is inherently per-pixel. Testing only whether the camera position is inside a volume does not solve it. Failure can begin before the camera point crosses the boundary because the near plane is an area, not a point.

NVIDIA’s technical description states exactly this limitation: Z-pass is wrong when the camera is inside a volume; Z-fail instead counts crossings behind the visible surface and works regardless of the camera’s position, provided the volume is properly closed. [GPU Gems 3, Chapter 11](https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-11-efficient-and-robust-shadow-volumes-using)

Your near plane is only `0.05`, in [main.c](/Users/igor/Developer/simplegl/main.c:95), which reduces how early the intersection happens but cannot eliminate it.

## Documented solutions

### 1. Robust Z-fail / Carmack’s reverse — recommended here

For every shadow volume:

- Render stencil changes on **depth failure**:
  ```c
  glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP, GL_KEEP);
  glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
  ```
- Generate a closed volume:
  - Light/front cap.
  - Extruded silhouette sides.
  - Dark/far cap.
- Ensure the far end cannot be clipped.

This fixes partially intersecting near planes naturally: each pixel is classified independently by counting from the visible surface toward a known outside point at infinity.

The canonical robust construction combines:

1. Z-fail.
2. Infinite shadow extrusion using homogeneous vertices with `w = 0`.
3. A projection whose far plane is at infinity.

That is the central result of Everitt and Kilgard’s paper, [Practical and Robust Stenciled Shadow Volumes for Hardware-Accelerated Rendering](https://arxiv.org/abs/cs/0301002).

For this fixed-function renderer, homogeneous extrusion remains feasible without shaders: the shadow-volume representation would need four-component vertices, and extruded vertices would be emitted as directions such as:

```text
(vertex - light, w=0)
```

using `glVertex4f`. Ordinary cap vertices use `w=1`.

This is preferable to merely restoring the old finite `renderer.c` implementation. Its `SHADOW_EXTRUDE` cap can still cross the camera far plane or terminate before sufficiently distant receivers.

### 2. Z-fail plus depth clamp

`GL_DEPTH_CLAMP` disables near/far clipping and clamps generated depth values instead. Khronos explicitly lists stencil shadow volumes and avoiding near-plane capping as motivating cases. [ARB_depth_clamp specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_depth_clamp.txt)

This can simplify robust Z-fail, but the application requests an OpenGL 2.1 context. `ARB_depth_clamp` became core in OpenGL 3.2, so availability in the current legacy macOS context must be detected at runtime. Requesting a modern macOS core context would also remove the immediate-mode/fixed-function API this renderer relies on.

Therefore it is a useful optional path, but not the best portability baseline.

### 3. Dynamically choose Z-pass or Z-fail

A common optimization is:

- Use cheap, cap-free Z-pass when a caster’s volume is provably clear of the camera near plane.
- Use capped Z-fail whenever it might intersect.

The conservative test must cover the entire near rectangle, not only the camera point. If half the near plane is inside, the affected volume is rendered entirely with Z-fail.

This reduces cap/overdraw costs while retaining correctness, but it requires both complete rendering paths.

### 4. Corrected Z-pass algorithms

Research includes:

- **ZP+**: initializes/corrects the stencil count around the near plane.
- **++ZP / AtomicZP**: obtains the missing initial count through additional rasterization or atomic counting.
- **Split-plane shadow volumes**: consistently selects Z-pass or Z-fail per tile/pixel.

These specifically address partial near-plane intersection, but are substantially more complicated and assume more modern programmable hardware:

- [A Practical and Efficient Approach for Correct Z-Pass Stencil Shadow Volumes](https://graphics.tudelft.nl/~marroquim/publications/pdfs/usta-HPG2019.pdf)
- [Split-Plane Shadow Volumes](https://diglib.eg.org/server/api/core/bitstreams/a20dd513-d9c9-4346-8bcc-49addf687324/content)

They are unnecessary for this small OpenGL 2.1 renderer unless Z-fail performance becomes a demonstrated problem.

### 5. Explicitly cap the near-plane intersection

One can geometrically clip each Z-pass volume against the camera near plane and generate a closing cap there.

This is documented but generally avoided: it requires per-frame clipping, is numerically fragile, and can produce cracks where independently generated cap and side geometry disagree. Z-fail/infinite extrusion was developed largely to avoid this machinery.

## Other relevant limitations in this code

- The current extrusion is finite: [shadow.c](/Users/igor/Developer/simplegl/shadow.c:29) computes `light + 100 * (vertex - light)`. Shadows can therefore terminate or be clipped in sufficiently large scenes.
- The standard silhouette construction assumes consistently wound, closed, two-manifold caster meshes. GPU Gems explicitly documents that requirement. Your generated primitives pass sealed-volume tests, but imported or future irregular geometry would need validation or the more robust per-triangle construction.
- The tests pass, but [tests.c](/Users/igor/Developer/simplegl/tests.c:157) explicitly expects “sides only.” They verify Z-pass volume construction, not camera-inside behavior, caps, stencil operations, or rendered results.

## Recommendation

Restore Carmack’s reverse in the active modular implementation, but implement the complete robust version:

1. Add front and back caps.
2. Change stencil writes from depth-pass to depth-fail.
3. Represent extruded vertices at homogeneous infinity.
4. Use an infinite-far projection for the shadow pass—or depth clamp when verified available.
5. Add rendered regression scenes where the near plane is outside, partially intersects, and is fully inside a shadow volume.
6. Update README and architecture documentation to match the actual implementation.

No source files were changed during this investigation. `make test` passes, confirming the existing tests encode the current side-only Z-pass design.