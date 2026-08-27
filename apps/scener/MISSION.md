# SimpleSketch3D Mission Statement

## Purpose

SimpleSketch3D exists to make **3D useful to 2D artists without requiring them to learn a full 3D package**.

Many illustrators use Blender not because they want to become 3D artists, but because 3D solves a small set of painful problems extremely well:

- perspective
- spatial construction
- camera angles
- lighting
- cast shadows
- repeated views of the same character or environment

Their goal is often not to produce finished 3D artwork. The 3D scene is a **temporary reference**: a fast blockout that establishes believable geometry, perspective, composition, and lighting before the artist paints over it in Photoshop, Clip Studio Paint, or another 2D tool.

SimpleSketch3D should be built specifically for that workflow.

## The Problem

Drawing convincing scenes is difficult. Perspective, construction, occlusion, light direction, cast shadows, and repeated views of the same subject all require significant manual work.

3D can remove much of that friction, but general-purpose applications such as Blender introduce a different kind of friction:

- large, complex interfaces
- many modes and concepts irrelevant to a 2D artist
- workflows built around hotkeys that occasional users forget
- tiny controls and panels
- terminology designed for full 3D production
- hundreds of features that are unnecessary when the final result will be painted over anyway

A 2D artist may only need to:

1. add a sphere, cube, cylinder, or simple prop;
2. move, rotate, and scale it;
3. duplicate it;
4. arrange a scene;
5. choose a camera angle;
6. position a light or sun;
7. inspect the resulting perspective and shadows;
8. render a reference image;
9. paint over it.

That workflow should not require learning Blender.

## Mission

> **Replace Blender for the 3D-reference workflow of 2D artists — not by recreating Blender, but by exposing only the artistic operations they actually need.**

SimpleSketch3D is not intended to become an easier general-purpose 3D package.

It should instead become a **3D Reference Studio for 2D Artists**.

The application should make it possible to go from an idea to a useful perspective-and-lighting reference in minutes, even for someone who opens a 3D tool only occasionally.

## What Matters

### 1. Perspective

The application should act as a perspective solver.

The artist places objects in 3D space and chooses a viewpoint. The software handles foreshortening, relative scale, vanishing behavior, and spatial relationships automatically.

The artist should be able to concentrate on composition rather than perspective construction.

### 2. Simple Spatial Blockout

The goal is not production-quality modeling.

A character can begin as spheres, capsules, and cylinders.

A city can begin as boxes.

A car can begin as a few primitive volumes.

A room can begin as walls, furniture prefabs, and simple geometric proxies.

The blockout only needs to communicate enough form for the artist — or an image-generation model — to understand the scene.

### 3. Camera Exploration

The same scene should be easy to inspect from many angles.

This is especially important for:

- comics
- illustrated books
- concept art
- sequential illustrations
- repeated locations
- recurring characters
- cinematic compositions

The artist should be able to quickly try high angles, low angles, close-ups, wide shots, portrait framing, landscape framing, and unusual viewpoints without rebuilding anything.

### 4. Lighting and Cast Shadows

Lighting is not decoration. It is part of the reference.

SimpleSketch3D should make it extremely easy to answer questions such as:

- Where will this character's shadow fall?
- How long will the shadows be at sunset?
- Which side of the building is illuminated?
- How does this composition read with strong side light?
- What changes if the sun is high versus near the horizon?

Point lights and directional sunlight should be direct, visual, and easy to manipulate.

### 5. Disposable Reference Rendering

The 3D render is usually not the finished image.

It is a scaffold.

Its job is to provide:

- perspective
- proportions
- object placement
- occlusion
- silhouettes
- camera framing
- lighting direction
- cast-shadow shapes

The output can then be painted over by a human artist or used as a structural reference for AI image generation.

This means SimpleSketch3D should prioritize **clarity and speed over photorealism**.

## Core Workflow

```text
idea
  ↓
simple 3D blockout
  ↓
arrange objects
  ↓
choose camera
  ↓
choose lighting
  ↓
render reference
  ↓
paint over / AI overpaint
  ↓
final illustration
```

This workflow is the product.

## Product Principles

### Do Not Build Blender

Every feature should be evaluated against the core question:

> Does this make it faster or easier for a 2D artist to construct a useful visual reference?

If the answer is no, the feature is probably outside the core mission.

The project should resist becoming a general-purpose DCC application.

### Prefer Direct Manipulation

A user should not need to memorize commands such as:

- `G` to move
- `S` to scale
- `R` to rotate
- `Shift+A` to add
- `Shift+D` to duplicate
- obscure camera-view commands
- hidden panel locations

Those operations should be visible and understandable in the interface.

The ideal interaction is:

**Add Sphere → drag it → squash it → duplicate it.**

**Add Sun → drag the sun around the sky.**

**Choose Camera → frame the composition.**

**Render Reference.**

### Optimize for Occasional Users

The application should remain usable even if the artist has not opened it for six months.

A user should not have to relearn the software every time they return.

Important operations must be discoverable from the interface rather than dependent on muscle memory.

### Simple Modeling Is Enough

Primitive modeling is a strength, not a limitation, when the final image will be painted over.

The most valuable modeling capabilities are those that quickly create readable reference forms:

- box
- sphere
- cylinder
- capsule
- cone
- plane
- wall
- transform
- duplicate
- mirror
- array
- bevel
- simple extrusion
- reusable prefabs

More sophisticated modeling should only be added when it directly improves the reference workflow.

### Camera and Light Are First-Class Tools

Camera and lighting controls are at least as important as modeling controls.

A simple model viewed from the right angle with useful lighting is more valuable to a 2D artist than a complex model with cumbersome camera controls.

### Real-World Scale Matters

Scenes should preserve consistent proportions and dimensions so that the same location can be reused across many illustrations and camera angles.

This turns the 3D scene into a persistent spatial reference rather than a disposable single-shot trick.

### Multi-View Consistency Is a Core Advantage

One scene should support many final images.

The same environment can be rendered from different cameras while preserving:

- room layout
- character scale
- furniture placement
- architectural proportions
- occlusion
- lighting direction
- shadow relationships

This is valuable both for human artists and for AI-assisted visual storytelling.

## Current Direction

SimpleSketch3D already follows this mission closely.

Its architecture is centered around:

- simple primitives
- reusable prefabs
- transforms
- cameras
- point lights
- directional sunlight
- real-time shadows
- scene composition
- multiple viewpoints
- reference rendering
- AI-friendly scene descriptions
- direct object manipulation
- paint-over workflows

The project should continue moving toward a friendly visual editor built around these capabilities.

## Near-Term Priorities

For the 2D-reference use case, the highest-value improvements are:

1. clear add-object UI;
2. obvious move / rotate / scale controls;
3. one-click duplication;
4. easy ground-plane creation;
5. visual sun-direction control;
6. simple point-light placement;
7. intuitive camera creation from the current view;
8. focal-length / field-of-view presets;
9. portrait, landscape, square, and common illustration aspect ratios;
10. one-click reference rendering;
11. bevel support for quickly making blockout geometry more readable;
12. a strong prefab library for furniture, architecture, vehicles, props, and character mannequins.

Later, useful extensions may include:

- mesh import;
- minimal character posing;
- richer materials;
- soft shadows / area lights;
- basic animation;
- shot variants;
- additional reference render modes.

These should remain subordinate to the main workflow rather than turning the application into a general 3D suite.

## Product Definition

SimpleSketch3D should not ask:

> How can we make Blender simpler?

It should ask:

> What is the 2D artist actually trying to accomplish by opening Blender?

The answer is usually much smaller:

**Perspective.  
Scale.  
Composition.  
Camera.  
Light.  
Shadows.  
Simple form.  
Reference.**

SimpleSketch3D should provide those things directly.

**That is the mission.**
