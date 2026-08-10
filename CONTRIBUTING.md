# Contributing

## Build and test

```bash
make          # build the framework and sample apps
make test     # build and run all tests in tests/
```

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

# .orion UI authoring

## Orion auto-layout (WPF-based)

Orion's layout is directly modeled on WPF: Grid with star-sized columns, StackPanel
orientation, and Measure/Arrange auto-measurement.

### Grid layout

Always use explicit `<column>` elements (never `columns="N"`):
- Columns **without** `width=` are star-sized — they share available space equally
- Columns with `width="48"` get fixed allocation
- `WINDOW_FLEXSPACE` goes on the grid itself, never on controls inside columns
- Label columns typically `width="48"` to `width="80"`

```xml
<grid name="fields" spacing="4">
  <column name="labels" width="48">
    <label text="Name:" />
    <label text="Email:" />
  </column>
  <column name="inputs" flags="WINDOW_FLEXSPACE">
    <textedit name="name" value="1" />
    <textedit name="email" value="2" />
  </column>
</grid>
```

### Flex class defaults

| Always flex by default | Not flex by default |
|---|---|
| `space`, `reportview`, `multiedit` | `button`, `label`, `textedit`, `checkbox`, `combobox`, `separator`, `list` |

Containers (`grid`, `stack`, `flow`) are not inherently flex. Flex intent bubbles upward
through matching container orientation.

### Form height rules

- **Fixed-content forms**: omit `height=`, specify `width="X"` only — height auto-calculates
- **Flex-content forms**: specify both `width="X" height="Y"` — needed when `<multiedit>`,
  `<reportview>`, or `<grid>` with `WINDOW_FLEXSPACE` is present
- Horizontal `<space>` elements (pushing buttons) do NOT require explicit height

### Apple HIG spacing

- 8pt padding inside dialogs
- 4-6pt spacing between related controls
- 12-16pt spacing between control groups
- Label column: 48-56pt for short labels, 80pt for longer
- Buttons: right-aligned, default on right, cancel to its left

### Critical mistakes to avoid

* Do not use `columns="N"` attribute (removed — use `<column>` elements)
* Do not add `WINDOW_FLEXSPACE` to controls inside grid columns
* Do not add `WINDOW_FLEXSPACE` to `<column>` elements
* Do not forget `WINDOW_FLEXSPACE` on grid when embedding in a stack
* Do not use `<space>` before buttons in fixed-height forms (use `<separator>`)
* Do not use `frame=` on auto-layout children

### Scrolling and popup rules

- Mouse coordinates delivered to a window proc are already in content space with scroll applied; do not add scroll offset again for hit-testing
- Top-level scrollable popup paint projection applies its own scroll offset; draw rows at content coordinates
- Capture sends mouse events to the popup even outside it; convert content coordinates back to viewport for bounds-testing
- On outside captured click: restore pre-open value, release capture, return focus to owner, send no selection-change notification

### Controls quick reference

| Control | XML | Notes |
|---|---|---|
| Button | `<button name="ok" text="OK" flags="BUTTON_DEFAULT" />` | value= for control ID |
| Label | `<label text="Name:" />` | `color="text-disabled"` for hints |
| Text edit | `<textedit name="field" value="1" />` | single-line |
| Multi edit | `<multiedit flags="WINDOW_VSCROLL" />` | multi-line, needs flex height |
| Checkbox | `<checkbox name="flag" text="Enable" />` |
| Combobox | `<combobox name="choice" />` | see DB adapters above |
| Reportview | `<reportview flags="WINDOW_VSCROLL | WINDOW_FLEXSPACE" />` |
| Separator | `<separator />` | visual line, **no expansion** |
| Space | `<space />` | flexible spacer, **expands** along stack axis |

### Complete dialog pattern

```xml
<form name="settings" width="280" padding="8" spacing="8">
  <grid name="fields" spacing="4">
    <column name="labels" width="56">
      <label text="Username:" />
      <label text="Email:" />
    </column>
    <column name="inputs" flags="WINDOW_FLEXSPACE">
      <textedit name="username" value="1" />
      <textedit name="email" value="2" />
    </column>
  </grid>
  <checkbox name="auto_save" value="3" text="Auto-save" />
  <space />
  <stack orientation="horizontal" spacing="6">
    <space />
    <button name="ok" value="100" text="OK" flags="BUTTON_DEFAULT" />
    <button name="cancel" value="101" text="Cancel" />
  </stack>
</form>
```

# Test infrastructure

Tests live as individual `.c` files in `tests/` and use the header-only framework
at `tests/test_framework.h`. Each test file is compiled as its own binary and run
by `make test`.

## Core macros

```c
TEST_START("Suite name")   // print suite header
TEST("test name")          // announce a single test case (increments tests_run)
PASS()                     // mark current test passed
FAIL("reason")             // mark current test failed with a message
ASSERT(cond, msg)          // fail with msg if cond is false; returns from function
ASSERT_TRUE(cond)
ASSERT_FALSE(cond)
ASSERT_NULL(ptr)
ASSERT_NOT_NULL(ptr)
ASSERT_EQUAL(a, b)
ASSERT_NOT_EQUAL(a, b)
ASSERT_STR_EQUAL(a, b)
TEST_END()                 // print summary; returns 0 (pass) or 1 (fail) from main
```

## Minimal test skeleton

```c
#include "test_framework.h"
// Do NOT include ui.h unless the test genuinely needs SDL/OpenGL symbols.
// Duplicate any small pure-C helpers inline instead.

void test_something(void) {
  TEST("description of what is tested");
  ASSERT_EQUAL(actual, expected);
  PASS();
}

int main(void) {
  TEST_START("Module name");
  test_something();
  TEST_END();
}
```

## Key rules

- **Keep tests headless** — no display, GPU, or SDL init unless necessary
- **One test function per behavior** — split if a function exceeds ~20 lines
- **Test through public API** — never reach into struct internals or call `static` helpers
- **Cover edge cases**: NULL inputs, boundary values (0, INT_MAX), failure paths, happy path
- **Name descriptively**: function = `test_<behavior>`, `TEST("...")` string = plain English assertion
- **Assert eagerly, clean up always** — `ASSERT_*` calls `return` on failure; free resources before `PASS()`
- **Missing `TEST_END()`** is a bug — results won't print and exit code is wrong
- **`ASSERT` after allocation without cleanup** leaks on failure — free before assertions that can fail

## WinAPI message packing helpers (inline these; do not pull in ui.h)

```c
#define LOWORD(x)        ((uint16_t)((uint32_t)(x) & 0xffff))
#define HIWORD(x)        ((uint16_t)(((uint32_t)(x) >> 16) & 0xffff))
#define MAKEDWORD(lo,hi) ((uint32_t)(((uint16_t)(lo)) | ((uint32_t)((uint16_t)(hi))) << 16))
```

# Code review checklist

🔴 **Bugs / resource leaks**
- Missing `evDestroy` — resources allocated in `evCreate` not freed
- `HIWORD`/`LOWORD` packed backwards in `send_message` / `evCommand`
- `ASSERT` after resource allocation without cleanup (leaks on test failure)
- Missing `TEST_END()` in test files (results won't print, exit code wrong)

🟡 **Wrong pattern / maintainability**
- Raw `evKeyDown` handling instead of accelerator tables
- Application code that belongs in the framework (`kernel/` or `user/`)
- Drawing outside `evPaint` handler
- Testing internals instead of public API

🔵 **Style / idiomatic**
- Loose `x`/`y` pairs instead of `ipoint16_t` / `irect16_t`
- Overly large test functions (split at ~20 lines)
- Vague `TEST("...")` descriptions (should read like assertions)
- Missing `invalidate_window` after state changes
- Raw OpenGL calls outside renderer files
