---
layout: default
title: Architecture
nav_order: 3
---

# Architecture

Orion mirrors the layered design of classic Windows:

| Application | Examples | your code |
|---|---|---|
| commctl | Common Controls | buttons, lists, menubar |
| user | Window Management | create/destroy, draw, text |
| kernel / platform | Rendering + native backends | OpenGL, input queue, OS integration |

## `user/` – Window Management (USER.DLL)

Responsible for window lifecycle, the message queue, drawing primitives, and
text rendering.

| File | Purpose |
|---|---|
| `user.h` | `window_t`, `irect16_t`, public API |
| `messages.h` | Message constants (`kWindowMessage*`), window flags, colours |
| `window.c` | `create_window`, `destroy_window`, `move_window`, `find_window` |
| `message.c` | `send_message`, `post_message`, `get_root_window` |
| `event.c` | Platform event translation and mouse/key/wheel routing |
| `scrollbar.c` | Built-in window scrollbar input and position updates |
| `draw.h` / `draw_impl.c` | `fill_rect`, `draw_rect`, `draw_icon8/16`, viewports |
| `text.h` / `text.c` | `draw_text_small`, `strwidth`, bitmap font atlas |

## `kernel/` – Rendering Core (KERNEL.DLL)

Provides the renderer and low-level shared services. Platform backends supply
the native event queue; `user/event.c` translates that input into window
messages. The application's `main()` calls
`ui_init_graphics()` then loops with `get_message` / `dispatch_message` /
`repost_messages`.

| File | Purpose |
|---|---|
| `kernel.h` | `ui_init_graphics`, `get_message`, `dispatch_message`, `UI_WINDOW_SCALE` |
| `renderer.c` | Sprite/quad rendering via VAO/VBO, orthographic projection |
| `joystick.c` | Gamepad / joystick input |

## `commctl/` – Common Controls (COMCTL32.DLL)

Each control is a window procedure (`winproc_t`) that handles a standard
message set.

| Control | Proc | Header |
|---|---|---|
| Button | `win_button` | `commctl.h` |
| Checkbox | `win_checkbox` | `commctl.h` |
| Combobox | `win_combobox` | `commctl.h` |
| Text edit | `win_textedit` | `commctl.h` |
| Label | `win_label` | `commctl.h` |
| List | `win_list` | `commctl.h` |
| Column view | `win_reportview` | `columnview.h` |
| Menu bar | `win_menubar` | `menubar.h` |
| Console | `win_console` | `commctl.h` |

## Z-Order and `WINDOW_ALWAYSONTOP`

Windows are stored in a linked list.  `find_window` returns the **last**
matching window (highest z-order).  `move_to_top` places a regular window
just before the first `WINDOW_ALWAYSONTOP` entry so palette / menu-bar
windows always stay on top regardless of user clicks.

## `UI_WINDOW_SCALE`

Defined in `kernel/kernel.h` with `#ifndef` guard so it can be overridden at
compile time:

```c
#ifndef UI_WINDOW_SCALE
#define UI_WINDOW_SCALE 2   // default: SDL window is 2x the logical size
#endif
```

`SCALE_POINT(x)` divides raw SDL mouse coordinates by `UI_WINDOW_SCALE` to
produce logical pixel coordinates used throughout the framework.

## Window and input event routing

`dispatch_message()` in `user/event.c` translates platform input into Orion
messages. Top-level targeting starts with `find_window()`. Mouse input then has
two equivalent delivery paths:

1. **Direct delivery:** when the target returned by `find_window()` receives
   the event itself, `LOCAL_X` / `LOCAL_Y` convert screen coordinates to that
   window's content coordinates and add `win->hscroll.pos` /
   `win->vscroll.pos`.
2. **Nested delivery:** `handle_mouse()` recursively routes from a parent to
   the deepest child under the pointer. It hit-tests each child's `frame` in
   the parent's viewport, subtracts `c->frame.x/y`, and adds
   `c->hscroll.pos/vscroll.pos` before recursing or calling `send_message()`.

These paths implement the same contract: a window procedure receives mouse
coordinates in its own **content space**, with system scrolling already
applied. A view should use the coordinates it receives directly; it must not
add its scroll position again. This is a window-system responsibility so a
control behaves identically as a root child or inside arbitrarily nested
stacks, grids, and other layout containers.

### Coordinate spaces

| Space | Meaning | Used for |
|---|---|---|
| Platform screen | Physical platform coordinates | Incoming platform event |
| Logical screen | Platform coordinates divided by `UI_WINDOW_SCALE` | Top-level lookup |
| Parent viewport | Visible coordinates within a parent | Testing against child `frame` |
| Window content | Window-local coordinates plus its scroll position | Mouse messages delivered to `win->proc` |

Child `frame` coordinates are relative to the parent's content layout. Do not
add a child's scroll offset before checking whether the pointer is inside its
visible frame: hit-test in viewport space first, then convert to child content
space for delivery.

### Scrolling and built-in scrollbars

`WINDOW_HSCROLL` and `WINDOW_VSCROLL` attach framework-owned scrollbars to a
window. Their input is intercepted centrally by `send_message()` before the
window procedure runs. `user/scrollbar.c` updates the position and emits
`evHScroll` / `evVScroll`; controls publish their range/page with
`set_scroll_info()` and redraw in response. Wheel events target the deepest
child, use a visible built-in scrollbar when available, and otherwise bubble
to the parent with translated coordinates.

The system owns input-coordinate adjustment, scrollbar hit-testing, dragging,
and wheel routing. A scrollable view still owns its content extent, paints at
`content_position - scroll_position`, and handles scroll notifications to
invalidate or synchronize its model.

### Toolbars and non-client input

Toolbars occupy non-client space above the normal client area. Toolbar input
therefore does not follow ordinary child routing: `dispatch_message()` finds
the toolbar host and sends button down/up through the embedded-toolbar helpers.
Title-bar drag/resize and built-in scrollbar input are likewise handled before
ordinary view dispatch. Captured mouse input is delivered directly to the
capturing window, bypassing normal descendant hit-testing.

### Debugging checklist

- If a click is wrong only after scrolling, compare the painted content
  coordinate with the delivered mouse content coordinate.
- If only nested controls fail, inspect `handle_mouse()` rather than adding a
  correction to the control.
- If only a top-level/direct target fails, inspect `LOCAL_X` / `LOCAL_Y`.
- If the failure is over a scrollbar, toolbar, title bar, or during capture,
  inspect the corresponding interception path before the window procedure.
- Keep hit-testing in viewport space and event delivery in content space;
  mixing those two produces an error exactly equal to the scroll offset.
