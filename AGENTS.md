# Debug macros

These macros control verbose debug logging. Set to `1` at build time to enable,
`0` to disable. Default is `0` (off).

| Macro | File | Description |
|---|---|---|
| `GITCLIENT_DEBUG` | `examples/gitclient/gitclient.h` | gitclient app logging |
| `TABLEVIEW_DEBUG` | `commctl/tableview.c` | tableview control logging |
| `SOCIALFEED_DEBUG` | `examples/socialfeed/socialfeed.h` | socialfeed app logging |
| `IMAGEEDITOR_DEBUG` | `examples/imageeditor/imageeditor.h` | imageeditor app logging |
| `TASKMANAGER_DEBUG` | `examples/taskmanager/taskmanager.h` | taskmanager app logging |

Example: `make CFLAGS="-DGITCLIENT_DEBUG=1 -DTABLEVIEW_DEBUG=1"`

# Interaction trace logging

Every app must include **always-on** trace logging for user-triggered actions (clicks,
selections, tab switches, diff refreshes, state mutations). Use `fprintf(stderr, ...)`
— never guard behind a compile-time flag. This makes the event pipeline auditable
without a recompile.

## App-level code (e.g. `apps/gitclient/`)

Use the `GC_TRACE` macro defined in the app's header:

```c
// In gitclient.h — always-on, flushes immediately to stderr
#define GC_TRACE(...) do {                                       \
  fprintf(stderr, "[gc] " __VA_ARGS__);                          \
  fputc('\n', stderr);                                           \
  fflush(stderr);                                                \
} while (0)
```

Log at every user-facing event entry point:

- **`evCommand`**: tab switches (`tcnSelChange`), `RVN_SELCHANGE` (branch/log/file
  selection), `RVN_ITEMCHECK` (staging checkbox), `RVN_DBLCLK`, `btnClicked`,
  `GC_DIFF_STAGE_HUNK`, `GC_DIFF_TOGGLE_UNIFIED`
- **State mutations**: `gc_set_view_mode`, `gc_refresh_all`, `gc_open_repo`
- **Diff pipeline**: `gc_diff_refresh` — log commit/file/path/line-count/hunk-count

Each trace includes the window pointer, selection index, and relevant state
so the entire cascade (mouse → reportview → RVN_SELCHANGE → gc_diff_refresh)
is visible in one stream.

## Framework-level code (e.g. `orion/commctl/reportview.c`)

Use bare `fprintf(stderr, ...)` with a local prefix:

```c
fprintf(stderr, "[rv] mousedown win=%u mx=%d my=%d idx=%d old_sel=%d count=%d\n",
        (unsigned)win->id, mx, my, index, data->selected, data->count);
```

Log the window id so app-level `GC_TRACE` can be correlated with the control
that fired the notification.

**Convention:** app-level traces use the app's prefix (`[gc]`), framework-level
traces use the module's short prefix (`[rv]` for reportview, `[tv]` for tableview).

## Framework error logging

All incorrect or rejected framework behavior must be logged unconditionally to
`stderr`; never hide these diagnostics behind a debug macro. This includes
invalid message parameters, out-of-range indices, unavailable state, failed
resource allocation, and rejected state transitions. Include the framework
module prefix, window id when available, and the relevant values so the caller
can identify the bad request and its current state. Flush after diagnostics
that precede an early return.

# Window input architecture

Before changing mouse hit-testing, scrolling, scrollbars, toolbars, or nested
window dispatch, read [ARCHITECTURE.md](ARCHITECTURE.md#window-and-input-event-routing).
The central parent-to-child mouse router is `handle_mouse()` in `user/event.c`;
coordinate conversion belongs in the window system, not in individual views.

## Coordinate delivery to child windows

`handle_mouse()` receives coordinates in the **parent's viewport space**. When
dispatching to a child, coordinates must be converted to the **child's content
space** by adding the child's scroll offset:

```c
int lx = x - c->frame.x + (int)c->hscroll.pos;
int ly = y - c->frame.y + (int)c->vscroll.pos;
```

This ensures nested child windows receive content-space coordinates including
their scroll offsets, regardless of how many layout containers they are nested
inside. Missing `+ vscroll.pos` causes click-to-select after scrolling to hit
the wrong row (off by `vscroll.pos / ENTRY_HEIGHT`). This is a system-level
responsibility — no view or control should compensate for it.

# Code style

## Naming and formatting

- C99, no C++
- K&R bracing, 2-space indent
- `snake_case` for functions and variables; `snake_case_t` for types; `SCREAMING_SNAKE_CASE` for constants and macros
- Include guards: `#ifndef __MODULE_NAME_H__`
- Prefer `stdint.h` types (`uint32_t`, `uint16_t`) when size matters
- Prefer `ipoint16_t` / `irect16_t` over bare `int x, int y` pairs — matches WinAPI `POINT` / `RECT` convention
- **Prefer high-level rect/point utilities** over manual `x,y,w,h` arithmetic. Use `rect_split_*`, `rect_trim_*`, `rect_inset`, `rect_offset`, `rect_center`, `rect_contains_point` (all in `orion/user/rect.h`) to express layout and hit-test intent declaratively. Reserve bare field access for cases where no existing utility covers the operation.
- Minimal comments — only where logic is genuinely non-obvious

## Vertical space

Minimize vertical space. When a pattern repeats (switch cases, similar
assignments, etc.), format it as a compact aligned table — each entry on one
line, columns aligned — to read like a spreadsheet. Avoid wasted lines.

```c
// Good — compact, scannable, like a spreadsheet:
case AX_KEY_ENTER:     vgat_pty_write(st->pty_fd, "\r", 1);     return true;
case AX_KEY_BACKSPACE: vgat_pty_write(st->pty_fd, "\x7f", 1);   return true;
case AX_KEY_ESCAPE:    vgat_pty_write(st->pty_fd, "\x1b", 1);   return true;

// Bad — bloated:
case AX_KEY_ENTER:
  vgat_pty_write(st->pty_fd, "\r", 1);
  return true;
```

# WinAPI → Orion reference

Orion deliberately mirrors the WinAPI mental model. Think WinAPI first, then map:

| WinAPI concept | Orion equivalent |
|---|---|
| `HWND` | `window_t *` |
| `WNDPROC` | `winproc_t` — `result_t fn(window_t*, uint32_t msg, uint32_t wparam, void *lparam)` |
| `WM_*` messages | `kWindowMessage*` constants (e.g. `evCreate`, `evPaint`, `evDestroy`) |
| `CreateWindow` | `create_window(title, flags, rect, parent, proc, userdata)` |
| `DestroyWindow` | `destroy_window(win)` |
| `ShowWindow` | `show_window(win, visible)` |
| `InvalidateRect` | `invalidate_window(win)` |
| `GetMessage` / `DispatchMessage` | `get_message(&e)` / `dispatch_message(&e)` + `repost_messages(-1)` |
| `WM_COMMAND` routing | `evCommand`, `HIWORD(wparam)` = notification code, `LOWORD(wparam)` = control id |
| `TranslateAccelerator` | `translate_accelerator(win, table, &e)` before `dispatch_message` |
| `DialogBox` / `EndDialog` | `show_dialog(parent, proc, userdata)` / `end_dialog(win, result)` |
| `SetWindowLongPtr` / user data | `win->userdata` (allocated with `allocate_window_data(win, size)`) |
| `RECT` | `irect16_t { int x, y, w, h; }` via `MAKERECT(x,y,w,h)` |
| `POINT` | `ipoint16_t { int x, y; }` |
| `BN_CLICKED` | `btnClicked` |
| `CB_ADDSTRING` / `CBN_SELCHANGE` | `CB_ADDSTRING` / `CBN_SELCHANGE` |

# Framework patterns

## Message handling

Every window proc must handle at minimum `evCreate`, `evPaint`, and `evDestroy`.
Notifications always travel as `evCommand` to the parent.

- Return `true` if you handled a message, `false` if you did not (like returning 0 vs. calling `DefWindowProc`).
- Control IDs go in `LOWORD(wparam)`, notification codes in `HIWORD(wparam)`.

```c
// Correct — HIWORD = notification, LOWORD = control ID
send_message(parent, evCommand, MAKEDWORD(id, btnClicked), (void *)win);
```

## Accelerator tables over raw key handling

Never handle `evKeyDown` directly for keyboard shortcuts. Use accelerator tables:

```c
// BAD — polling keys bypasses the framework
case evKeyDown:
  if (wparam == SDL_SCANCODE_S) save_file();
  break;

// GOOD — use load_accelerators / translate_accelerator
accel_t table[] = {
  {MOD_CTRL, SDL_SCANCODE_S, ID_FILE_SAVE},
};
accel_table_t *accel = load_accelerators(table, ARRAY_LEN(table));
```

Accelerators fire as `evCommand` with `kAcceleratorNotification` in `HIWORD(wparam)`.

## Repainting

- Any state change that affects appearance must call `invalidate_window(win)`.
- Never call drawing functions outside `evPaint`. Trigger repaint via `invalidate_window`.
- Use `allocate_window_data(win, size)` for per-window state (like `SetWindowLongPtr`).

## Resource cleanup

Every `evCreate` that allocates resources (strings, buffers, textures) must have a matching
`evDestroy` that frees them. Leaks in window procs are a bug.

## Separation of concerns

- Application-level logic stays out of controls; framework-level logic stays out of apps.
- Framework features (timers, clipboard, drag-and-drop) belong in `kernel/` or `user/`, not in app code.
- No raw OpenGL calls outside `kernel/renderer.c` / `kernel/renderer_impl.c`.

## Extend, don't reinvent

When an app needs control behaviour that existing Orion controls don't yet support,
extend the framework control in `orion/commctl/` rather than implementing a
custom version in app code.  The command-panel tab row was built with custom
toolbar-button radio groups; when tabs needed icons, the right fix was to add
`tcSetImageStrip` / `tcSetTabIcon` to `win_tabview` and switch the app to use
it, not to keep the custom implementation.  Adding a message or option to a
framework control is always preferable to duplicating the control's event
routing, hit-testing, keyboard handling, and accessibility.

# Repository layout

```
ui.h              ← include this in every app; pulls in all subsystems
user/             ← window management, message queue, drawing, text, accelerators
kernel/           ← SDL event loop, init, renderer
commctl/          ← reusable controls: button, checkbox, edit, label, list, combobox, console
tests/            ← all test source files (*.c)
tests/test_framework.h   ← the test framework
tests/test_env.h ← SDL-init helper for tests that need a display
Makefile          ← `make test` builds and runs all tests/
```
