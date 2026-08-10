# Toolbars

Orion toolbars are **declarative descriptor arrays** rendered as a non-client band
above a window's client area.  The pattern used by the image editor is the
canonical way to build application-level and per-window toolbars.

## Overview

```text
┌─────────────────────────────────────────────────────┐
│  Menu bar                                           │
├─────────────────────────────────────────────────────┤
│  [New] [Open] [Save] │ [Undo] [Redo] │ [Zoom] ...  │  ← toolbar band
├─────────────────────────────────────────────────────┤
│                                                     │
│              client area / content                   │
│                                                     │
└─────────────────────────────────────────────────────┘
```

A toolbar is a window with the `WINDOW_TOOLBAR` flag.  Items are described by
`toolbar_item_t` structs and loaded with the `tbSetItems` message.  When a
button is clicked the toolbar sends `tbButtonClick` to its parent, carrying the
button's `ident` — typically a command ID that also appears in a menu.

## The descriptor

Defined in `orion/user/messages.h`:

```c
typedef struct {
  toolbar_item_type_t type;    // BUTTON, LABEL, COMBOBOX, TEXTEDIT, SEPARATOR, SPACER, DROPDOWN
  int                 ident;   // command ID / button identifier
  int                 icon;    // sysicon_* value; -1 = default (missing icon)
  int                 w;       // explicit width in pixels (0 = automatic)
  uint32_t            flags;   // BUTTON_PUSHLIKE, BUTTON_AUTORADIO, etc.
  const char         *text;    // label text, or combobox/textedit initial text
  const char         *tooltip; // hover tooltip; NULL = none
} toolbar_item_t;
```

Item types:

| Type | Description |
|---|---|
| `TOOLBAR_ITEM_BUTTON` | Icon-only button (owner-drawn) |
| `TOOLBAR_ITEM_LABEL` | Static text label |
| `TOOLBAR_ITEM_COMBOBOX` | Drop-down combobox (embedded child window) |
| `TOOLBAR_ITEM_TEXTEDIT` | Single-line text input (embedded child window) |
| `TOOLBAR_ITEM_SEPARATOR` | Narrow vertical divider |
| `TOOLBAR_ITEM_SPACER` | Invisible gap (no interaction) |
| `TOOLBAR_ITEM_DROPDOWN` | Split button: left fires `tbButtonClick`, right arrow fires `tbDropdown` |

## Two ways to define items

### 1. Static `const` array (per-window toolbars)

For toolbars that belong to a single window and don't change, define a file-scope
`static const` array:

```c
// win_layers.c
static const toolbar_item_t kLayersToolbar[] = {
  { TOOLBAR_ITEM_BUTTON,    ID_LAYER_NEW,       sysicon_image_add,  0, 0, NULL, "New layer" },
  { TOOLBAR_ITEM_BUTTON,    ID_LAYER_DUPLICATE, sysicon_page_copy,  0, 0, NULL, "Duplicate layer" },
  { TOOLBAR_ITEM_BUTTON,    ID_LAYER_DELETE,    sysicon_delete,     0, 0, NULL, "Delete layer" },
  { TOOLBAR_ITEM_SPACER,    0, 0, 0, 0, NULL, NULL },
  { TOOLBAR_ITEM_BUTTON,    ID_LAYER_MOVE_UP,   sysicon_arrow_up,   0, 0, NULL, "Move layer up" },
  { TOOLBAR_ITEM_BUTTON,    ID_LAYER_MOVE_DOWN, sysicon_arrow_down, 0, 0, NULL, "Move layer down" },
  { TOOLBAR_ITEM_SPACER,    0, 0, 8, 0, NULL, NULL },
  { TOOLBAR_ITEM_COMBOBOX,  ID_LAYER_BLEND_COMBO, -1, 120, 0, "Normal", NULL },
};
```

### 2. `.orion` XML (application-level toolbar)

For the main application toolbar, define items in the `.orion` file and the
build system generates a C array (`TB_MAIN[]` + `TB_MAIN_COUNT`):

```xml
<!-- imageeditor.orion -->
<toolbars>
  <toolbar name="main">
    <Button name="new"  menu="file" icon="sysicon_page_add"      text="New"  tooltip="New image" />
    <Button name="open" menu="file" icon="sysicon_folder_page"   text="Open" tooltip="Open image" />
    <Button name="save" menu="file" icon="sysicon_disk_save"     text="Save" tooltip="Save image" />
    <spacer w="10" />
    <Button name="undo" menu="edit" icon="sysicon_undo"          text="Undo" tooltip="Undo" />
    <Button name="redo" menu="edit" icon="sysicon_redo"          text="Redo" tooltip="Redo" />
    <spacer w="10" />
    <Button name="zoom_in"  menu="view" icon="sysicon_magnifier_zoom_in"  text="+"   tooltip="Zoom in" />
    <Button name="zoom_out" menu="view" icon="sysicon_magnifier_zoom_out" text="-"   tooltip="Zoom out" />
    <spacer w="10" />
    <Button name="show_background" menu="view" icon="sysicon_eye_show"
            flags="BUTTON_PUSHLIKE" text="BG" tooltip="Toggle background" />
  </toolbar>
</toolbars>
```

Include the generated header:

```c
#include "build/generated/apps/imageeditor/imageeditor.h"
```

The `menu=` attribute links each button to a menu item for consistent command
IDs.  The build tool resolves `name` + `menu` to produce the `ident` field.

## Creating a toolbar window

```c
window_t *win = create_window(
    "Toolbar",
    WINDOW_TOOLBAR | WINDOW_NOTITLE | WINDOW_ALWAYSONTOP |
    WINDOW_NORESIZE | WINDOW_NOTRAYBUTTON | WINDOW_NODRAG,
    MAKERECT(0, 0, screen_w, TOOLBAR_BAND_HEIGHT),
    NULL, my_toolbar_proc, hinstance, NULL);
show_window(win, true);
```

The `WINDOW_TOOLBAR` flag tells the framework to render the window as a
non-client band.  The band height is computed automatically from button size +
padding + bevels (`TOOLBAR_BAND_HEIGHT` = 28px at default 22px buttons).

For a WinAPI-style large toolbar with captions below icons, enable the label
style after setting the items:

```c
send_message(win, tbSetStyle, TOOLBAR_STYLE_SHOW_LABELS, NULL);
```

The existing `toolbar_item_t.text` supplies each caption. In this mode button
widths expand to fit their captions and the non-client toolbar band grows by
one small-font text row; client layout and input routing use the new height
automatically.

## Loading items

Send `tbSetItems` in `evCreate`.  The framework copies the array internally —
the caller does not need to keep it alive after the call.

```c
result_t my_toolbar_proc(window_t *win, uint32_t msg,
                         uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      send_message(win, tbSetItems,
                   (uint32_t)TB_MAIN_COUNT,
                   (void *)TB_MAIN);
      return true;
    case tbButtonClick:
      handle_menu_command((uint16_t)wparam);
      return true;
  }
  return false;
}
```

For a static array:

```c
case evCreate:
  send_message(win, tbSetItems,
               sizeof(kLayersToolbar) / sizeof(kLayersToolbar[0]),
               (void *)kLayersToolbar);
  return true;
```

## Single command dispatch

The key design rule: **every toolbar click routes through the same
`handle_menu_command()` function as menu items and keyboard shortcuts.**

```c
case tbButtonClick:
  handle_menu_command((uint16_t)wparam);
  return true;
```

This means:
- A toolbar button, a menu item, and a keyboard accelerator all execute the
  exact same code path.
- Adding a new button requires only: (1) define the command ID, (2) add a
  `toolbar_item_t` entry, (3) add a `case` in `handle_menu_command()`.
- No toolbar-specific logic is needed per button.

## Syncing toggle state

For toggle buttons (e.g., show/hide background), define a sync function that
reads application state and updates individual buttons:

```c
void imageeditor_sync_main_toolbar(void) {
  if (!g_app || !g_app->main_toolbar_win) return;
  bitmap_strip_t *strip = ui_get_sysicon_strip();
  window_t *bg_btn = get_window_item(g_app->main_toolbar_win, ID_VIEW_SHOW_BACKGROUND);

  if (bg_btn) {
    bool checked = !g_app->active_doc || g_app->active_doc->background.show;
    send_message(bg_btn, btnSetCheck,
                 checked ? btnStateChecked : btnStateUnchecked, NULL);
    if (strip) {
      int icon = checked ? (sysicon_eye_show - SYSICON_BASE)
                         : (sysicon_eye_hide - SYSICON_BASE);
      send_message(bg_btn, btnSetImage, (uint32_t)icon, strip);
    }
  }
}
```

Call the sync function:
- After `tbSetItems` (initial load)
- After any state change that affects toggle buttons
- After handling a `tbButtonClick` that toggles state

## `app_chrome` — menubar + toolbar wrapper

For standalone applications, `app_chrome` combines the menubar and main toolbar
into a single window that manages both:

```c
g_app->chrome_win = create_app_chrome(
    "Image Editor Chrome",
    editor_menubar_proc, kMenus, kNumMenus,
    main_toolbar_proc, hinstance);

g_app->menubar_win      = app_chrome_menubar(g_app->chrome_win);
g_app->main_toolbar_win = app_chrome_toolbar(g_app->chrome_win);
```

The chrome window:
- Automatically resizes both children on `evDisplayChange`
- Routes `evPaint` to both children
- Routes `tbButtonClick` from the toolbar to the toolbar proc
- Provides accessor functions: `app_chrome_menubar()`, `app_chrome_toolbar()`

## Summary of the pattern

```text
┌──────────────────────────────────────────────────────────┐
│ .orion XML            static const toolbar_item_t[]     │
│       ↓                         ↓                        │
│   TB_MAIN[]              kLayersToolbar[]                │
│       ↓                         ↓                        │
│         evCreate → tbSetItems → toolbar                  │
│                          ↓                               │
│              tbButtonClick(ident)                         │
│                          ↓                               │
│               handle_menu_command(id)                     │
│                          ↓                               │
│                   sync_toolbar()                          │
└──────────────────────────────────────────────────────────┘
```

Every toolbar — main, layers, timeline — follows this exact sequence.  The
pattern scales from a 3-button strip to a full application toolbar with
comboboxes, dropdowns, and toggle buttons.
