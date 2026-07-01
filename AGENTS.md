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

# Window input architecture

Before changing mouse hit-testing, scrolling, scrollbars, toolbars, or nested
window dispatch, read [docs/architecture.md](docs/architecture.md#window-and-input-event-routing).
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

# Code style: vertical space

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

This ensures nested child windows receive content-space coordinates including
their scroll offsets, regardless of how many layout containers they are nested
inside. Missing `+ vscroll.pos` causes click-to-select after scrolling to hit
the wrong row (off by `vscroll.pos / ENTRY_HEIGHT`). This is a system-level
responsibility — no view or control should compensate for it.
