---
layout: default
title: Filters — MVC Placement
nav_order: 20
---

# Filters — MVC Placement

This document describes where the image-editor filter system lives, how the
code is divided across the three MVC layers, and the one known boundary that
is not yet clean.

---

## What "filter" means here

There are three distinct things that share the word "filter" in the image
editor:

| Thing | Where it lives | Description |
|---|---|---|
| **Filter definition** | `g_app->filters[]` on `app_state_t` | Name string + compiled GL shader program.  Loaded once at startup from GLSL files on disk. |
| **Filter application** | `filtermgr.c` | Bake-to-GPU → readback routine that writes modified pixels back into the active layer. |
| **Filter gallery dialog** | `win_filtergallery.c` | Thumbnail grid + live-preview dialog.  Owns its own transient GPU objects for the duration of the dialog. |

---

## Files

```
examples/imageeditor/
  filtermgr.c          ← Model: load, free, apply filters
  win_filtergallery.c  ← View:  filter gallery dialog
```

GLSL shader files live at:

```
share/imageeditor/filters/<name>.frag.glsl
share/imageeditor/shaders/common.vert.glsl
```

---

## MVC layer assignments

### `g_app->filters[]` — application resource layer (model-adjacent)

`app_state_t` holds an array of `filter_t {name, program}` records populated
by `imageeditor_load_filters()`.  This is analogous to a font registry or
shader library: it is global application configuration loaded from disk at
startup and has no UI coupling.  In strict MVC it would sit in its own
resource/registry layer; keeping it on `app_state_t` is acceptable because
the controller already owns that struct.

```c
// imageeditor.h (app_state_t excerpt)
filter_t filters[IMAGEEDITOR_MAX_FILTERS];
int      filter_count;
```

### `filtermgr.c` — **model**

`imageeditor_apply_filter(doc, idx)` is a pure document mutation:

1. Upload the active layer to a GL texture (`canvas_upload`).
2. Run the shader program via `bake_texture_program`.
3. Read the result back with `read_texture_rgba`.
4. Delete the temporary GPU texture (`R_DeleteTexture`).
5. Overwrite `lay->pixels` with the result and set `canvas_dirty = true`.

No window handles are touched.  The GPU is used as a *compute tool*; the
only lasting side-effect is the updated pixel buffer in the document.
`imageeditor_apply_builtin_blur` and `imageeditor_apply_builtin_filter`
(sharpen, edge-detect) follow the same pattern.

```c
// filtermgr.c — model mutation, no UI
bool imageeditor_apply_filter(canvas_doc_t *doc, int filter_idx);
bool imageeditor_apply_builtin_blur(canvas_doc_t *doc, int radius);
bool imageeditor_apply_builtin_filter(canvas_doc_t *doc, uint16_t id);
```

### `win_filtergallery.c` — **view**

`filter_gallery_state_t` holds all transient GPU state for the dialog:

```c
typedef struct {
  canvas_doc_t *doc;
  int           selected;
  uint32_t      preview_tex;       // scaled copy of the source image
  uint32_t      thumb_sheet_tex;   // GPU texture owning the thumbnail strip
  bitmap_strip_t thumb_strip;      // strip descriptor for win_reportview
  bool          accepted;
} filter_gallery_state_t;
```

Everything in this struct is created in `evCreate` and destroyed in
`evDestroy`.  The dialog never writes into the document until the user clicks
**OK** or double-clicks a thumbnail, at which point `filter_gallery_accept`
calls `imageeditor_apply_filter` (the model function) and sets
`st->accepted = true`.

The thumbnail grid is a `win_reportview` in `RVM_VIEW_LARGE_ICON` mode.
Thumbnails are baked once via `filter_gallery_bake_thumbnails`:
`bake_texture_program + read_texture_rgba` → packed into a single vertical
strip texture, then handed to the list via `RVM_SETICONSTRIP`.

This is the cleanest MVC boundary in the editor: the view owns all
GPU preview state locally, commits to the model only on explicit user
acceptance, and tears everything down when dismissed.

---

## Sequence: user opens gallery, selects a filter, clicks OK

```
User → Photo > Filter Gallery…
  controller (win_menubar.c evCommand)
    → show_filter_gallery_dialog(parent)          [view entry-point]
      → show_dialog_from_form_ex(…, filter_gallery_proc, &st)
        evCreate:
          filter_gallery_make_preview_tex(doc)    [view: GPU alloc]
          filter_gallery_bake_thumbnails(&st)     [view: GPU alloc]
          RVM_ADDITEM × N                         [view: populate list]
        evCommand / RVN_SELCHANGE:
          filter_gallery_sync_preview(win, st)    [view: update preview]
        evCommand / btnClicked OK:
          filter_gallery_accept(win, st)
            doc_push_undo(doc)                    [model]
            imageeditor_apply_filter(doc, idx)    [model: mutate pixels]
            end_dialog(win, 1)
        evDestroy:
          R_DeleteTexture(preview_tex)            [view: GPU free]
          R_DeleteTexture(thumb_sheet_tex)        [view: GPU free]
  controller receives result
    send_message(doc->win, evStatusBar, …)        [view: status update]
```

---

## Known boundary issue — `layer.preview`

The live-preview mechanism for *other* filter dialogs (e.g., blur, levels)
stores a `ui_render_effect_t` value on the document struct so the canvas can
render a preview overlay each frame.  This is render-pipeline state that
leaked into `canvas_doc_t`.

The filter gallery avoids this problem by allocating its own `preview_tex`
inside `filter_gallery_state_t` and re-rendering through the selected
shader every time the selection changes.  No document state is modified
until the user accepts.

The correct fix for the other dialogs is to follow the same pattern: keep
all preview state in the dialog's local state struct, not in the document.

---

## Summary

| Layer | Responsibility |
|---|---|
| **Resource** (on `app_state_t`) | Filter name + compiled GL program, loaded once at startup |
| **Model** (`filtermgr.c`) | Pixel mutation — GPU used as a compute tool, result lands in `lay->pixels` |
| **View** (`win_filtergallery.c`) | Dialog-local GPU preview state, thumbnail strip, commit only on user acceptance |

The rule of thumb: if a GPU object is created to *display* something to the
user and will be discarded when the window closes, it belongs in the view's
local state struct.  If its only purpose is to compute a result that gets
written into the document, the function belongs in the model even though it
uses the GPU internally.
