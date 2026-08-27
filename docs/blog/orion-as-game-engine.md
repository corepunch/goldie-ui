---
layout: default
title: Orion as a Multiwindow Retro Gaming Engine
parent: Blog
nav_order: 1
date: 2026-04-27
---

# Orion as a Multiwindow Retro Gaming Engine

*April 27, 2026*

---

Most game engines give you a single fullscreen surface and ask you to build everything on top of it. Orion takes a different bet: what if every panel, viewport, and HUD element were a genuine window — a first-class citizen with its own state, paint cycle, and event handler?

That question turns out to be surprisingly powerful for a certain genre of game.

## The Pitch

Imagine *Dungeon Master* or *Ultima Underworld* — a dungeon crawler where the 3D viewport, the map, the character stats, and the inventory all live in resizable, moveable windows. Or a strategy game in the vein of *Civilization* or *Master of Magic* where the tech tree, the city editor, and the main map are genuinely separate windows you can drag around a desktop. Or a space trader where the cargo manifest is an actual spreadsheet you scroll through.

This is the **Multiwindow Retro Gaming Engine** pattern: use Orion's WinAPI-style windowing system as the backbone of your game, and let each game panel be an Orion window.

## Why Orion Fits

### 1. The Message Loop Is the Game Loop

A classic game loop looks like this:

```c
while (running) {
    process_input();
    update();
    render();
}
```

Orion's main loop is:

```c
ui_event_t event;
while (get_message(&event)) {
    dispatch_message(&event);
}
```

These are the same basic shape. `get_message` pumps events from Orion's native
Platform backend and `dispatch_message` routes them into the window tree. Game
state updates can run from timers or application messages, while rendering stays
inside `evPaint`. This keeps input, simulation, and drawing in the same message
architecture as the rest of the application.

### 2. Windows Are Game Entities

In Orion every window has:

- A **proc** function (`winproc_t`) — essentially `OnMessage()`
- A **userdata** pointer — your game-object state
- A **frame** rect — position and size on screen
- An **invalidate/paint** cycle — dirty-flag rendering, exactly what games want

Sound familiar? This is the same pattern as a game-object component with `update()` and `draw()`. The window procedure *is* the component update loop, and `evPaint` *is* the draw callback.

```c
// A game viewport as an Orion window
static result_t dungeon_view_proc(window_t *win, uint32_t msg,
                                  uint32_t wparam, void *lparam) {
    dungeon_state_t *st = (dungeon_state_t *)win->userdata;
    switch (msg) {
        case evCreate:
            st = calloc(1, sizeof(dungeon_state_t));
            win->userdata = st;
            load_level(st, "level01.dat");
            return true;
        case evPaint: {
            irect16_t client = get_client_rect(win);
            render_3d_view(st, &client);
            return true;
        }
        case evKeyDown:
            handle_movement(st, (int)wparam);
            invalidate_window(win);
            return true;
        case evDestroy:
            free(st);
            return true;
    }
    return false;
}
```

### 3. OpenGL Rendering Is Already There

Orion uses hardware-accelerated rendering through the public renderer interface
in `orion/kernel/renderer.h`. Textures, meshes, and drawing primitives share the
same frame lifecycle as Orion's chrome. A custom viewport therefore renders in
its window's client region during `evPaint`, alongside ordinary controls.

### 4. Joystick and Gamepad Support

`orion/kernel/joystick.c` wraps the native Platform joystick API and exposes
device initialization, availability, and naming through Orion's kernel layer.
Applications can build game-specific input messages on that shared platform
service without adding a second windowing dependency.

### 5. The Retro Aesthetic Is Built In

Orion ships with bitmap system and content fonts, a pixel-art icon sheet, and a
desktop-oriented theme. If your game wants draggable windows, title bars, menus,
and compact workstation-style controls, those pieces already share one visual
system.

## A Concrete Blueprint: Four-Window Dungeon Crawler

Here is how you'd structure a classic dungeon crawler using Orion:

```
┌──────────────────────────────────────┐
│  Dungeon Master Clone                │
├──────────────────┬───────────────────┤
│                  │   Mini-map        │
│  3D Viewport     ├───────────────────┤
│  (dungeon_view)  │   Character Stats │
│                  │   (stats_panel)   │
├──────────────────┴───────────────────┤
│  Inventory Grid  (inventory_panel)   │
└──────────────────────────────────────┘
```

Each panel is an Orion window:

| Window | proc | State |
|---|---|---|
| `dungeon_view` | `dungeon_view_proc` | 3D renderer state, current room |
| `minimap` | `minimap_proc` | Fog-of-war bitmap |
| `stats_panel` | `stats_proc` | HP, MP, status effects |
| `inventory_panel` | `inventory_proc` | Item grid, drag-and-drop |

They communicate via `send_message` and `post_message`, the same mechanisms used
by Orion controls. Controls send `evCommand` notifications to the root window;
application windows can use focused custom messages such as `evRoomChanged` to
schedule updates without coupling their internal state.

Because they are real Orion windows, panels can participate in ordinary focus,
resize, paint, and input routing. An application can also implement detachable
panels by recreating a child as a top-level window while keeping the represented
game state in the model rather than in the live window.

## Beyond the Dungeon

The multiwindow pattern applies far beyond RPGs:

- **Space trader** — main star map window + trade window (a real `win_reportview` spreadsheet) + ship status panel
- **Real-time strategy** — main map + construction queue (a `win_list`) + minimap + tech tree dialog
- **Programming game** — code editor window (`win_edit`) + execution log (`win_console`) + visualisation canvas
- **Puzzle game** — game board window + hint panel + move history list

In every case you get scrollbars, resize handles, keyboard focus, menus, and all the other plumbing for free.

## Getting Started

The fastest path is to clone Orion and look under `apps/`. Hello World shows the
window lifecycle and command flow. File Manager demonstrates a multi-panel
layout with report views, while Scener is the closest complete reference for
scene editing and custom rendering.

```bash
git clone https://github.com/corepunch/orion-ui
cd orion-ui
git submodule update --init --recursive
make build/bin/helloworld build/bin/filemanager build/bin/scener
build/bin/filemanager
```

Start with [Getting Started](../getting-started) for the application lifecycle,
then use [Scener](../examples#smaller-references) and its
`apps/scener/README.md` guide as a larger rendering example. Keep reusable input
or rendering capabilities in Orion and keep game rules in the application
layer.

## Closing Thought

Orion's standalone window and message architecture works equally well for tools and games. The windows are your viewports, and the message loop is your game loop.

If that idea resonates with you, [grab the source](https://github.com/corepunch/orion-ui) and start building.

---

*Tagged: game engine, retro, architecture, C, native platform, OpenGL*
