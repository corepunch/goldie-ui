# Shot Composition Guide for Scene-Blockout Agent

**Purpose:** you (the agent) build one 3D scene — geometry, lights, shadow casters — then place *multiple cameras* in it, one per panel/page. Each camera placement is a "shot." This document tells you how to choose camera position, framing, lighting, and shadow behavior so the shots read as dynamic, intentional visual storytelling rather than a static screenshot of a room from six angles.

Source lineage: this synthesizes standard cinematography/animation-layout staging principles (the tradition Mateu-Mestre's *Framed Ink* and the Lee/Buscema *Marvel Way* book both draw from), not verbatim text from either book.

---

## 1. Every shot answers one question first

Before placing a camera, decide: **what is this panel telling the reader that the previous one didn't?** A shot exists to convey one of:
- **Geography** — where are we, how is the space laid out (establishing shot)
- **Action** — what is happening, physically, right now
- **Emotion/reaction** — what a character feels
- **Relationship** — power, distance, or connection between two subjects
- **Detail/stakes** — an object, clue, or threat the reader needs to register

Pick the answer first, then choose framing/angle/lighting that serves *only* that answer. Don't reuse a generic "nice angle" — every camera in the scene should be justified by its story job.

## 2. Camera placement fundamentals

### 2.1 Height = power/vulnerability
- **Low angle (camera below eye-line, looking up):** subject reads as powerful, dominant, threatening. Use for villains, reveals, moments of triumph.
- **High angle (camera above, looking down):** subject reads as small, vulnerable, trapped, or observed. Use for defeat, isolation, "God's eye" scene-setting.
- **Eye-level:** neutral, intimate, puts reader on equal footing with subject. Default for dialogue/character beats.

Set `camera.pitch` accordingly: negative pitch (looking up) for low angle, positive (looking down) for high angle, ~0° for eye-level. A shift of even 10–15° reads clearly; don't be timid — subtle angle changes get lost, especially at small panel sizes.

### 2.2 Distance = intimacy vs. context
- **Wide/establishing:** full geometry visible, subject small in frame. Used once per scene, usually first panel, to orient the reader. Don't repeat unless geography changes.
- **Medium shot:** waist-up or full figure with some environment — the workhorse shot for action and blocking.
- **Close-up:** face or hands fill frame, environment mostly cropped out. Reserve for emotional peaks or critical detail (an object, an expression).
- **Extreme close-up:** eyes, a single hand, a trigger. Use sparingly — it's a punctuation mark, not a paragraph.

Vary distance panel-to-panel. A run of same-distance shots ("all mediums") is the single most common cause of a comic page feeling flat. Alternate: wide → medium → close, or medium → close → extreme-close for escalating tension.

### 2.3 Angle = the "3/4 rule"
Avoid pure front-on (flat, dull, "mugshot") and pure profile (flat, no depth) as defaults. Place the camera **off-axis**, roughly 30–60° from the subject's facing direction, so you get:
- overlapping forms (near arm crosses torso, etc.) → reads as three-dimensional
- a visible front plane *and* side plane of the subject → depth cue even in a still image

Rule of thumb for `camera.yaw` relative to subject facing: aim for 30–45° off dead-on for most shots. Reserve pure profile for two-shots (side-by-side confrontations, "versus" framing) and pure front-on for direct address / confrontation with the reader.

### 2.4 The line of action / 180° rule
For any shot involving two or more characters interacting (dialogue, combat), establish an imaginary line running through the subjects' axis of interaction. Keep every camera in the sequence on the *same side* of that line unless you deliberately want to disorient the reader (rare — reserve for chaos/plot-twist moments). Crossing the line without intent causes the reader to lose spatial continuity between panels.

When placing multiple cameras in one scene for a dialogue sequence: pick a 180° arc on one side of the two subjects and stay within it.

### 2.5 Diagonals and imbalance = energy
A perfectly level, centered, symmetrical composition reads as calm/static/formal. For **dynamic** action panels:
- Tilt the camera (Dutch/canted angle) 5–15° for unease, disorientation, impact. Don't overuse — it loses power if every panel is tilted.
- Compose so the subject's main action line (a raised sword, a running diagonal, a falling body) runs corner-to-corner of the frame rather than parallel to the frame edges. Diagonal compositions read as kinetic; horizontal/vertical-aligned compositions read as stable/calm.
- Place the subject off-center (rule-of-thirds intersections) for tension and implied off-frame space; center the subject only for confrontation, iconic/hero moments, or direct symmetry-driven power poses.

## 3. Staging — readability before beauty

This is the single biggest lesson from animation-layout tradition (and the throughline of *Framed Ink*): **a shot's #1 job is instant silhouette readability.** If a viewer can't tell what's happening from the pose/shape alone at a glance, the shot has failed regardless of how nice the lighting is.

Checklist for every camera placement:
1. **Silhouette test:** if you flattened the subject to pure black, would the pose/action still read? Rotate the camera until the important gesture (a punch, a pointed finger, a slumped posture) is not foreshortened into ambiguity.
2. **Avoid tangents:** don't let the camera angle cause unrelated scene edges to touch or align with character edges (e.g., a doorframe line appearing to grow out of a character's head). Nudge the camera position slightly to separate silhouettes.
3. **One clear focal point per shot.** If two things compete for attention (a character and a bright window behind them), either reposition the camera, adjust which light is brightest, or defocus/darken the secondary element.
4. **Overlap = depth, gaps = confusion.** Deliberate overlap of foreground/midground/background elements sells depth; accidental gaps that make the subject look "pasted on" over the background break it.

## 4. Foreground / midground / background — the three-plane rule

Every shot should ideally contain three depth layers:
- **Foreground:** something close to camera, often cropped by frame edges (a doorway edge, a table corner, a character's shoulder). Creates depth and a sense that the reader is *inside* the scene, not looking at a diorama.
- **Midground:** the main subject/action.
- **Background:** environment context, can be softer/simpler.

For your renderer: when placing a camera, check whether any geometry sits between camera and subject that can serve as foreground framing (a chair back, a doorframe, foliage, another character's silhouette at frame edge). If nothing is available, consider whether the scene blockout needs one more prop placed specifically to serve as foreground framing for that shot.

Foreground elements are also useful as **natural frames** — an archway, window, or gap between two objects that frames the subject and directs the eye. This is a fast way to add compositional intent without moving lights.

## 5. Lighting

### 5.1 Motivated light first
Every light in the scene should have an implied source (window, lamp, fire, sky). Don't add lights that don't correspond to something in-world — even fill/rim lights should read as plausible bounce or a secondary practical source. This keeps the render from looking like a stage-lit product photo.

### 5.2 Three-point as baseline, then break it on purpose
- **Key light:** primary, defines the main light/shadow direction on the subject. Set at roughly 30–45° off camera axis and somewhat elevated, as a default neutral setup.
- **Fill light:** softer, opposite side of key, reduces shadow harshness. Lower intensity than key (roughly half or less). Reduce or remove fill for moodier/harsher scenes.
- **Rim/back light:** placed behind subject, separates subject from background by lighting the edge (hair, shoulder line). Critical when subject and background are similar in value/color.

For **dynamic, moody, or dramatic panels**, deliberately break this: drop fill light near to zero for high-contrast chiaroscuro (see 5.3), or make rim light the *only* strong light for a dramatic silhouette-against-glow shot.

### 5.3 High contrast (chiaroscuro) for drama
Strong, single-direction key light with minimal fill produces large, graphic shadow shapes — reads as tense, dramatic, mysterious, or dangerous. Use for:
- villain reveals, threats, night scenes, interrogations, moments of dread.

Even, low-contrast, multi-source lighting reads as safe, calm, mundane, comedic. Use for everyday domestic scenes, comic relief, establishing "normal" before it's disrupted.

**Practical for the agent:** expose two lighting "modes" as scene parameters — `key_intensity`, `fill_intensity`, `contrast_ratio` (key:fill). High ratio (8:1 or higher) = dramatic. Low ratio (2:1 or less) = calm/neutral.

### 5.4 Light direction tells story on its own
- **Top light:** neutral-to-ominous (deep eye sockets, heavy shadows) depending on intensity — classic "interrogation lamp" or overhead sun.
- **Under-light (light from below):** almost always reads as unnatural/scary — campfire-story effect. Use deliberately for horror/uncanny beats only.
- **Side light (90° from camera):** half the face/body lit, half in shadow — good for morally ambiguous characters, internal conflict, dramatic tension.
- **Backlight only (subject in silhouette):** withholds detail/emotion, builds mystery, works well for a reveal-delay or an imposing entrance.
- **Front, flat light:** minimizes shadow, reduces drama — use intentionally for "safe," clinical, or flashback/memory scenes to visually differentiate them from the main narrative's lit style.

## 6. Shadows

### 6.1 Cast shadows ground objects and carry information
A shadow anchors an object to the floor/wall it's near — without a contact shadow, objects read as floating/pasted. Since your renderer uses stencil shadow volumes, always confirm every subject and major prop is casting onto the nearest surface; a missing shadow under a character is one of the fastest ways a render reads as "off."

### 6.2 Shadow shapes as storytelling
Cast shadows can carry narrative content beyond the object casting them:
- A shadow can reveal an off-screen threat before the source enters frame (a long shadow stretching into a room from an unseen doorway).
- A shadow can distort a character's silhouette into something more monstrous/larger than their actual pose — useful for tension without changing the character model at all, just light angle + distance from the wall.
- Shadow *bars* (light through blinds, fence, cage-like structures) crossing a subject imply confinement, surveillance, danger, even with no literal cage in the geometry.

When blocking a scene, consider placing a light so it casts a meaningful secondary shadow pattern (window mullions, railing, foliage) across the subject or floor, especially for suspense/threat panels.

### 6.3 Shadow length and softness = time of day and mood
- Long, low-angle shadows (light near the horizon) = dawn/dusk, often used for finality, melancholy, or high drama.
- Short, harsh, near-vertical shadows (light near-overhead) = midday, can read as exposed, unshaded, tense (a "nowhere to hide" quality) or simply neutral/mundane depending on contrast.
- Soft-edged shadows (larger/softer light source, e.g. overcast sky) = calmer, gentler mood.
- Hard-edged shadows (small/point light source, e.g. bare bulb, sun) = graphic, dramatic, higher tension.

If your shadow-volume renderer supports it, treat "shadow edge softness" as a mood parameter alongside intensity, not just a technical toggle.

## 7. Negative space

Negative space (empty/unoccupied area of frame — a plain wall, sky, floor) is not "wasted" area; it's a compositional tool — and for this pipeline it's also **functional real estate**: captions and dialogue text get placed there. This makes negative space a hard requirement, not just an aesthetic nicety.

### 7.0 Reserve space for text — non-negotiable
Every panel needs at least one clean, low-detail, low-contrast region large enough to hold a caption box or speech balloon without covering a face, a key action line, or an important shadow shape. When placing a camera:
- Identify the intended **text zone** *before* finalizing framing (commonly upper-third, lower-third, or a side margin — pick based on where the composition is naturally empty per the rules below).
- Keep that zone free of busy geometry, high-contrast shadow patterning, or anything essential to the read of the image. A plain wall, sky, floor, or out-of-focus background works; a shadow-bar pattern (6.2) or a character's face does not.
- Don't let the "looking room"/"leading room" space (below) and the text zone fight each other — ideally make them the same region, since both want to be open, low-content areas anyway.
- For dialogue-heavy panels, consider biasing negative space toward the top or side edges rather than only in the direction of gaze, so speech balloons have a consistent, predictable place to live across a sequence without overlapping subjects panel to panel.
- If a shot is intentionally tight/claustrophobic (minimal negative space, below) but the panel *needs* text, either widen the framing slightly or shift composition to open a margin at one edge — don't let the tightness rule override the functional need for a text-safe area.

The remaining points below describe negative space as a mood/storytelling tool — apply them, but always resolve them in a way that still leaves a usable text zone.
- **Space in the direction a character looks or moves** ("looking room"/"leading room") reads as natural and lets the reader anticipate what's next. Leaving empty frame *behind* a character's gaze/movement instead feels cramped or wrong.
- **Isolating a subject in a large empty field** communicates loneliness, insignificance, vastness, or vulnerability — a classic device for grief, defeat, or awe.
- **Minimal negative space (tightly cropped, subject fills frame)** communicates claustrophobia, urgency, intensity, or intimacy.
- Negative space can also be *the* focal point — e.g., an empty chair, an empty side of a bed — implying absence/loss without needing to render the missing element at all.

**Practical for the agent:** when framing a shot, deliberately decide how much of the frame is "occupied" vs. "open," and bias open space toward the side a character faces/moves for standard shots, or compress it tightly for tension beats.

## 8. Panel-to-panel and page-level considerations

Since the same 3D scene will serve multiple panels/pages, treat each *sequence* of shots (not just each individual shot) as a composition:

1. **Vary shot type panel to panel** — don't stack two same-distance, same-angle shots back to back unless intentionally creating a "beat/pause" (a held reaction, a deliberate repetition for comedic or dramatic timing).
2. **Establish, then get close.** Typical sequence: wide/establishing → medium (action/blocking) → close (reaction/detail) → back out for the next beat. This mirrors how a reader's eye wants to be oriented before being pulled in emotionally.
3. **Match the line of action across the sequence** (see 2.4) so characters don't visually "flip sides" between panels without narrative reason.
4. **Escalate contrast/tilt/angle extremity with escalating story tension.** Save your most extreme low-angles, harshest chiaroscuro lighting, and steepest Dutch tilts for the sequence's peak beat — if every panel is maximally dramatic, none of them are.
5. **Panel shape/aspect ratio itself can be a storytelling tool** if your pipeline supports variable panel dimensions: wide horizontal panels suit sweeping establishing shots or slow horizontal action (a chase, a landscape); tall vertical panels suit height, falls, looming threats, or emphasize a single upright figure; a large panel (or full page) suits a splash/hero moment; small panels in a tight grid suit rapid action or racing tension.

## 9. Quick decision table for the agent

| Story goal | Camera height | Distance | Angle off-axis | Lighting ratio | Notes |
|---|---|---|---|---|---|
| Establish location | eye-level or slightly high | wide | any, favor 3/4 | low-medium | show full geometry, one per scene |
| Normal dialogue beat | eye-level | medium | 30–45° | low (2:1) | keep both/all speakers on same side of line of action |
| Character triumph/power | low | medium-close | 3/4 or near-frontal | medium | strong key, optional rim |
| Character defeat/vulnerability | high | medium-close | 3/4 | low, soft | wide negative space around subject |
| Threat/danger reveal | eye-level or low | wide→close over sequence | 3/4, consider silhouette | high (8:1+) | use shadow patterning (6.2), hard shadow edges |
| Emotional close-up | eye-level | close/extreme-close | 3/4, avoid flat frontal | context-dependent | crop environment out, negative space = looking room |
| Action/impact | dynamic, varies | medium | diagonal composition, consider Dutch tilt | high | diagonal main action line, foreground framing if possible |
| Quiet/aftermath | eye-level or high | wide, subject small | 3/4 | low, soft shadows | maximize negative space around isolated subject |

## 10. Practical implementation notes for this pipeline

- **One scene, many cameras:** since geometry/lights are shared across panels, plan the *light rig* for the scene's dominant mood first (e.g., "tense nighttime interior" → high contrast ratio, hard shadows, one motivated key). Individual panels then vary primarily through camera position/angle/distance rather than relighting from scratch — but don't be afraid to add a secondary light or dim/boost an existing one per-shot if a specific panel needs a mood shift (e.g., a flashback within the scene).
- **Shadow casters:** make sure every character/prop that matters compositionally (see 6.1–6.2) is flagged as a shadow caster in the XML scene, especially anything meant to produce a narrative shadow shape (window frame, railing, off-frame figure).
- **Camera field of view:** wider FOV (~50–70°) exaggerates depth and works well for dynamic/action framing and environments; narrower FOV (~25–35°) compresses depth, flattens perspective, and suits calmer or more formal/emotional close-ups. Vary FOV deliberately as another tool alongside distance/angle, not just distance alone.
- **Silhouette-check pass:** before finalizing a camera, do a mental (or literal, if the pipeline allows a quick render pass) silhouette check per §3 — this is cheap to verify and catches most "flat" or confusing shots before final render.
- **Text-zone check pass:** alongside the silhouette check, verify every panel has a defined, uncluttered text zone (§7.0) before finalizing the camera — this is as much a hard pass/fail check as the silhouette test, since a panel without room for its caption/dialogue isn't usable regardless of how well it composes otherwise.
