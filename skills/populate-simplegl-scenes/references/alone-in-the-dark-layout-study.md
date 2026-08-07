# Alone in the Dark floor and fixed-camera study

Use this reference when a scene needs several connected rooms, corridor pacing,
architectural repetition, stairs, or a fixed-camera sequence. It records useful
patterns from extracted *Alone in the Dark*, *Alone in the Dark 2*, and *Alone
in the Dark 3* floor files. Treat the patterns as design evidence, not as a
template or a source of canonical dimensions.

## Source and interpretation

The reviewed snapshot contains:

| Game | Source directory | Floor files | Room IDs | Cameras |
|---|---|---:|---:|---:|
| AitD 1 | `AitD/output` | 8 | 57 | 144 |
| AitD 2 | `AitD/output2` | 15 | 63 | 201 |
| AitD 3 | `AitD/output3` | 14 | 162 | 245 |
| Total |  | 37 | 282 | 590 |

Room counts come from `roomN/...` comments on extracted boxes. They include
sparse or linked-room records, especially in AitD 3, so they are a useful map
of floor complexity rather than a precise count of fully modeled rooms. The
median extracted floor contains 7 room IDs and 15 cameras.

The XML is a reference conversion, not the original authoring format:

- Visible boxes approximate collision and structural volumes. Object records
  are often represented by tiny placeholder boxes and do not preserve the
  original model silhouette or useful furniture dimensions.
- Invisible `material="glass" renderable="0"` boxes are scenario zones. In
  particular, `sceType0` is a linked-room/visibility zone, not a camera trigger
  volume. Do not use its `param` value to assign a camera to a room.
- Cameras are correctly useful at floor scope, but the converted files do not
  retain the original per-camera coverage polygons. A camera-to-room or
  camera-to-trigger matrix must therefore be authored anew.
- Extracted world dimensions preserve relationships within a file, but floors
  use different elevations and sometimes contain distant or partial geometry.
  Copy proportions and staging ideas; establish SimpleGL scale independently.

The accompanying mockup reference shows the intended production relationship:
coarse 3D establishes projection, contact, occlusion, and major silhouettes;
the painted background supplies finish. SimpleGL mockups may be more detailed,
but detail must remain subordinate to shot-readable structure.

## The floor is the continuity unit

The strongest reusable idea is to plan a floor before polishing individual
rooms. A floor owns a shared coordinate system, connected room graph, vertical
transitions, and a camera family. This gives doors, corridor turns, and stairs
real destinations and makes adjacent shots spatially coherent.

For a multi-room SimpleGL scene:

1. Draw the room-and-portal graph first. Mark entrances, exits, locked routes,
   stairs, overlooks, and any route that returns to an earlier space.
2. Place every room shell and corridor in one floor coordinate frame before
   dressing any room. Use groups for room-local transforms, but verify portals
   and floor elevations in world space.
3. Define circulation bands and camera beats across the complete floor. Do not
   solve each room as an isolated attractive box.
4. Keep the floor in one scene when shots depend on cross-room sightlines,
   shared stairs, or continuous camera geography. Split it only for a concrete
   loading, performance, or production reason, while retaining a floor plan
   reference that preserves coordinates and connections.
5. Name cameras by floor, spatial beat, and purpose rather than by an arbitrary
   serial alone, for example `F1_Hall_EntryWide` or `F1_Study_DeskReveal`.

## Room layout lessons

The extracted shells favor compact rooms and a smaller number of deliberately
elongated circulation spaces. A heuristic pass over 197 substantial shell
bounds found a median footprint aspect ratio of about `1.46:1`; only 16 were
at least `3:1`. The exact dimensions are conversion-dependent, but the contrast
is useful: ordinary rooms are not all corridors, and corridor proportions are
reserved for a clear pacing function.

- Build recognizable spatial identities. Vary room proportion, entrance side,
  ceiling height, major axis, and focal wall instead of repeating the same
  centered rectangle with different furniture.
- Offset adjoining rooms and doors. A slight dogleg, side entry, shallow
  vestibule, or partial partition prevents every doorway from exposing the
  complete next room and creates a reveal for the next camera.
- Use architectural mass to divide a large room into shot-sized pockets:
  columns, chimney breasts, alcoves, screens, stairs, and heavy furniture can
  create foreground, midground, and background without adding arbitrary walls.
- Preserve a legible route through each room. Dress edges and focal pockets
  densely, but keep the path between relevant portals and interactions clear.
- Give a room one dominant read from its entry and a different secondary read
  after traversal. A camera sequence benefits when crossing the room changes
  which architectural layer is foreground.
- Let adjacent rooms contrast. A narrow hall opening into a broad chamber, a
  low room preceding a tall stair hall, or a cluttered service space preceding
  a formal sparse room produces rhythm at floor scale.

Representative shell cases worth reopening in the source snapshot include
`AITD1_floor02.xml` room 1, `AITD2_floor02.xml` room 1,
`AITD2_floor12.xml` room 0, `AITD3_floor01.xml` room 4, and
`AITD3_floor10.xml` room 12. They are elongated examples; do not treat all of
them as literal corridors without checking their rendered architecture.

## Corridors as pacing devices

Do not model a corridor as leftover space between rooms. It is a sequence of
approach, concealment, reveal, and transition beats.

- Prefer bends, offsets, widened nodes, and short terminal views over one
  undifferentiated tube. A turn gives the camera a natural cut point and keeps
  the destination from being exhausted in the first view.
- Put a visual anchor at or beyond the useful end of a corridor: a lit door,
  stair, window, statue, strong shadow, or color/value break. The anchor should
  explain the corridor's direction without revealing every intervening threat
  or interaction.
- Use door recesses, columns, wall projections, and foreground furniture to
  break the long silhouette. Repetition may establish rhythm, but interrupt it
  at decision points.
- Widen intersections and stair landings enough to stage a turn, encounter, or
  camera handoff. Keep the travel lane narrower than major rooms so arrival has
  a perceptible release.
- Cover a long route with successive spatial beats. Each fixed view should own
  a comprehensible stretch with a clear entry and exit; do not rely on one deep
  axial camera for the entire run.
- At a camera change, preserve travel direction or provide a strong landmark.
  A cut that reverses screen direction while also hiding the destination makes
  navigation needlessly ambiguous.

## Object placement and blockout fidelity

The converted object boxes are positional markers, not evidence for asset size
or quality. Use the mockup image and the architectural layouts for higher-level
placement lessons:

- Place the largest furniture and occluders during the shell pass because they
  affect camera access, navigation, foreground framing, and painted-background
  seams. A large cabinet or sofa is part of shot architecture.
- Favor edge clusters and asymmetric islands over uniformly distributing props
  across the floor. Keep a readable central or diagonal movement channel.
- Arrange furniture in relationships: a desk faces its working side, chairs
  orient toward a table or conversation partner, and storage opens into usable
  clearance. Do not place isolated objects only to fill empty coordinates.
- In every intended camera, choose which objects supply foreground cropping,
  which carry the action, and which establish background context. Geometry that
  never affects silhouette, shadow, action, or spatial understanding can remain
  simplified.
- Build view-critical silhouettes and contact geometry before small surface
  detail. A proxy should already communicate object category, facing, usable
  side, and height; paint-over cannot reliably repair a wrong projection or
  occlusion order.
- Keep intentional rest areas. Dense perimeter dressing is effective only when
  the route, focal action, and text zone remain readable.

## Columns and repeated structure

Columns are used most effectively as spatial rhythm and partial separation,
not as freestanding decoration. `AITD3_floor00.xml` contains a particularly
clear run of approximately `0.5`-square, `4.6`-high members along one shared
axis, mostly spaced about `1.9` units apart. The useful lesson is the bay rhythm,
not those literal measurements.

- Align a column run to an architectural axis and use consistent bays. Change
  spacing only to mark an entry, stair, focal object, or change of room.
- Use columns to create layered sightlines: a near column may crop the frame,
  middle columns establish depth, and a gap can isolate the focal subject.
- Keep the travel path and important gestures out of unavoidable column
  occlusion. Test every fixed camera; a good plan-view rhythm can become a row
  of accidental tangencies in projection.
- Let columns justify ceiling beams, arches, railings, or changes in floor
  treatment. Unsupported repeated posts read as arbitrary set dressing.
- Prefer an `<array>` for a genuinely regular run, then author exceptional bays
  separately. Avoid copy-pasted members with small accidental spacing drift.

## Staircases and vertical transitions

The extraction includes both long repeated stair runs and compact filled-step
constructions. `AITD2_floor02.xml` shows repeated blocks with a consistent
approximately `0.7` rise/run module across several levels;
`AITD3_floor00.xml` includes a tighter sequence whose visible step height grows
in regular increments. These are useful construction references, not ergonomic
standards.

- Choose rise, run, width, landing elevation, and destination first. Generate
  regular steps with `<array>` when possible so the top meets its landing
  exactly.
- Treat the stair and both landings as one traversal composition. The lower
  approach, turn or intermediate landing, and upper destination must be
  spatially plausible and camera-readable.
- Use stairs to reveal vertical information gradually. A low approach camera
  can withhold the upper floor; a high landing camera can expose the route just
  traveled and make vulnerability legible.
- Place a strong architectural or lighting cue at the destination. The viewer
  should understand why the stair exists and where it leads.
- Reserve enough headroom and lateral clearance for the traveler and intended
  camera. Keep rails and posts from merging with the subject silhouette.
- For a switchback or long ascent, plan separate lower, landing, and upper
  camera beats instead of forcing the whole traversal into one view.

## Fixed-camera findings

Across all 590 extracted cameras:

- The median vertical field of view is `45.41` degrees, and 331 cameras use
  that exact converted value. The middle half lies from `45.41` to about
  `52.23` degrees; 90 percent are at or below about `65.32` degrees.
- Only 9 cameras are narrower than `40` degrees and 35 are wider than `70`
  degrees. Extreme lenses are punctuation rather than the default.
- The median vertical look angle is about `21` degrees downward. 467 cameras
  look down by more than 5 degrees, 84 are approximately level, and only 39
  look up by more than 5 degrees.
- Camera height varies radically, including near-floor views and steep overhead
  views. Raw height is not portable because floor elevations differ; the
  reusable principle is purposeful height contrast.

Apply those observations as a starting grammar:

1. Begin fixed-camera coverage near `45–55` degrees vertical FOV. Change the
   lens only when the shot needs stronger spatial exaggeration, compression,
   or unusually broad coverage.
2. Favor a moderately elevated three-quarter view for navigation and room
   comprehension. A downward angle shows floor routes, furniture footprints,
   and character contact while retaining wall depth.
3. Mix in a small number of level, low, steep, or overhead views for a specific
   reveal, threat, vulnerability, or vertical transition. Do not randomize
   camera height for variety alone.
4. Aim at the traversal/action volume, not at the geometric center of the room.
   The frame should show where the subject entered, what currently matters, and
   enough of the next route to support movement.
5. Use architecture as foreground masking. Door jambs, columns, rails, and
   furniture edges can create depth and controlled concealment, but must not
   block required interactions.
6. Give each camera a coverage region during authoring even though the source
   conversion omitted the originals. Draw the regions in plan, test overlaps,
   and define the handoff boundary before polishing composition.
7. Overlap adjacent coverage enough that a subject cannot disappear between
   shots. In overlap areas, select the camera that best preserves subject
   visibility, travel direction, and the established side of action.
8. Avoid switching exactly in a narrow doorway if the new shot makes the
   subject appear on the wrong side or immediately hides them behind the frame.
   Put the handoff where both outgoing and incoming compositions remain clear.
9. Reserve at least one clean low-detail text zone in each story camera. The
   painted-background workflow makes negative space an authored deliverable.

Do not infer that every room needs the same number of cameras. The aggregate is
roughly two cameras per extracted room ID, but the floor files do not preserve
a trustworthy room-to-camera relation and several rooms are partial records.
Allocate cameras by traversal beats, occlusion, actions, and story emphasis.

## Camera coverage worksheet

For a floor with fixed views, record this before final scene dressing:

| Camera | Story/spatial job | Coverage region | Entry edge | Exit edge | Subject range | Foreground mask | Landmark | Text zone | Previous/next continuity |
|---|---|---|---|---|---|---|---|---|---|
| `F1_Hall_EntryWide` | Establish hall and stair | plan polygon or bounds | south door | north turn | near to mid hall | west jamb | lit stair | upper right | preserves northward travel |

The coverage region is an authoring concept, not currently a SimpleGL XML tag.
Keep it in the source brief or a floor camera matrix. Camera `comment`
attributes should summarize the job and continuity in prose.

## What to copy and what not to copy

Copy:

- floor-scale planning and connected spatial continuity;
- contrast between compact rooms and deliberate elongated routes;
- camera families with a strong moderate-FOV, downward-looking baseline;
- purposeful exceptions in height and lens;
- columns, door frames, furniture, and stairs used as compositional layers;
- blockout geometry designed for a later image-making pass.

Do not copy:

- raw dimensions without re-establishing character and prefab scale;
- placeholder object box sizes or materials;
- incomplete shells, missing ceilings, or extraction artifacts;
- a supposed room-camera mapping derived from `sceType0`;
- camera names that communicate only an index;
- original fixed views without adapting them to the new story action, text
  placement, aspect ratio, and renderer constraints.

## Review checklist

Before accepting a multi-room fixed-camera floor, verify:

1. The room-and-portal graph is complete and every portal has a destination.
2. Corridors have authored approach, turn, reveal, and arrival beats.
3. Major furniture, columns, stairs, and partitions support both circulation
   and foreground/midground/background layering.
4. Repeated structure has intentional bay spacing and justified exceptions.
5. Every stair reaches both landings exactly and has usable headroom.
6. Each camera has one explicit job, a coverage region, and named handoffs.
7. Adjacent cameras preserve travel direction or show a strong reorientation
   landmark.
8. Default lenses remain moderate; every extreme FOV or camera height has a
   specific narrative reason.
9. Every shot keeps the subject readable throughout its coverage and reserves
   a text-safe region.
10. The final SimpleGL scene still follows the normal enclosure, lighting,
    contact, shadow, XML, build, test, load, and screenshot-review requirements.
