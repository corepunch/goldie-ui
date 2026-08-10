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

# Dialog Data Exchange (DDX)

Always use DDX for dialog field binding. The framework provides `ctrl_binding_t`,
`DDX_TEXT`, `DDX_CHECK`, `DDX_COMBO`, `dialog_push`, and `dialog_pull` in
`orion/user/user.h`. Never write manual `send_message(w, edGetText, ...)` /
`send_message(w, btnGetCheck, ...)` sequences in dialog procs when DDX covers it.

**Canonical pattern:**

```c
typedef struct {
  char name[256];
  bool flag;
} my_state_t;

static const ctrl_binding_t my_bindings[] = {
  DDX_TEXT (ID_MY_DIALOG_NAME, my_state_t, name),
  DDX_CHECK(ID_MY_DIALOG_FLAG, my_state_t, flag),
};

static result_t my_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  my_state_t *st = (my_state_t *)win->userdata;
  if (msg == evCreate) {
    win->userdata = lparam; st = (my_state_t *)lparam;
    /* populate combos, fill defaults into st, then: */
    dialog_push(win, st, my_bindings, ARRAY_LEN(my_bindings));
    return true;
  }
  if (msg != evCommand || HIWORD(wparam) != btnClicked) return false;
  uint16_t id = LOWORD(wparam);
  if (id == ID_MY_DIALOG_CANCEL) { end_dialog(win, 0); return true; }
  if (id == ID_MY_DIALOG_OK) {
    dialog_pull(win, st, my_bindings, ARRAY_LEN(my_bindings));
    /* validate st, act, end_dialog */
    return true;
  }
  return false;
}
```

**When DDX does NOT apply** — skip it and keep plain send_message when:
- The dialog has no persistent state struct (e.g. conflict dialog with only list + action buttons).
- A control drives live UI reactions on every keystroke (`edUpdate` populating another field, live search filtering). Those reactions must stay as explicit `evCommand` handlers.
- A combo drives selection-change side effects (`cbSelectionChange` populating other fields). The selection-change handler stays; DDX still applies to the text fields that get populated.

**`DDX_COMBO` vs `DDX_TEXT` for comboboxes** — `DDX_COMBO` binds an `int` index field. If you need the selected string (e.g. to pass to a git command), bind with `DDX_TEXT` instead — comboboxes hold their current text in `window->title` and respond to `edGetText`.

# Database adapters for combobox population

Always use the database adapter mechanism to populate comboboxes rather than
manual `send_message(w, cbAddString, ...)` sequences in dialog procs. Define
`source`/`display`/`value` on the `<combobox>` element in the `.orion` file;
orionc generates a `combobox_params_t` structure that the combobox control uses
to auto-populate from the database at creation time and on `evSetDatabase`.

**`.orion` attributes:**

| Attribute | Description |
|---|---|
| `source` | Database path (e.g. `db.branches`) |
| `display` | Field name shown in the dropdown |
| `value` | Field name for the actual value (e.g. `name` for strings, `id` for FKs) |
| `filter_field` | Optional: field name to filter on (e.g. `is_remote`) |
| `filter_value` | Optional: value to match as a string (e.g. `"0"` or `"1"`) |

**Example — push/pull dialog comboboxes:**

```xml
<combobox name="remote" flags="flexspace"
          source="db.remotes" display="name" value="name" />
<combobox name="branch" flags="flexspace"
          source="db.branches" display="name" value="name"
          filter_field="is_remote" filter_value="0" />
```

**When database adapters do NOT apply** — skip them and keep manual `cbAddString`/`cbClear` when:
- The combobox drives live UI reactions on every keystroke (`edUpdate` live search filtering) that re-fills the list.
- The combobox items come from a non-database source (e.g. `git_get_remotes` output used in a static list).

**Default selection after auto-population** — set the state struct's field to the
desired default string *before* `dialog_push`. For the first item, use `dbFetch`
to get the first record's value. Example after auto-population:

```c
if (!st->remote[0] && g_gc && g_gc->db) {
  result_node_t *remotes = (result_node_t *)send_db_message(
    g_gc->db, dbFetch, MAKEDWORD(ID_DB_REMOTES, 0), (void *)0);
  if (remotes) {
    db_remote_t *r = *(db_remote_t **)remotes->data;
    if (r) strncpy(st->remote, r->name, sizeof(st->remote) - 1);
    free_result_list(remotes);
  }
}
dialog_push(win, st, bindings, ARRAY_LEN(bindings));
```
