# GitHub Copilot Instructions for Orion

## Project Overview

Orion is a UI framework library extracted from DOOM-ED, organized in a Windows-like architecture with three main layers:
- **user/** - Window management and user interface (USER.DLL equivalent)
- **kernel/** - Event loop and SDL integration (KERNEL.DLL equivalent)  
- **commctl/** - Common controls (COMCTL32.DLL equivalent)

The framework is written in C and uses SDL2 for windowing/input and OpenGL 3.2+ for rendering.

## Code Architecture and Conventions

### Version 1.0 Refactor Policy (Required)

- Orion is treated as **1.0-first**: prioritize clean current design over preserving legacy behavior.
- Do not add backward-compatibility shims, aliases, fallback fields, dual-path APIs, or migration adapters unless explicitly requested.
- When introducing a new approach, update the existing codebase to that approach directly and remove superseded patterns in the same change.
- Prefer one canonical representation per concept (no duplicate old/new fields or lookup keys).

### Directory Structure
- `user/` contains window management, message queue, drawing primitives, and text rendering
- `kernel/` contains SDL event loop, initialization, and joystick/gamepad support
- `commctl/` contains reusable UI controls (buttons, checkboxes, edit boxes, labels, lists, comboboxes, console)
- `examples/` contains example programs demonstrating framework usage
- `ui.h` is the main header that includes all UI subsystems

### Header File Policy (Required)

- Avoid header sprawl in feature directories.
- Prefer one public header per directory, named after that directory (for example, `examples/formeditor/formeditor.h`).
- New feature declarations for that directory should be added to the directory header instead of creating additional `*.h` files.
- `*.c` files should carry implementation ownership comments (for example, "implemented in win_canvas.c") where this meaning is not obvious from naming.
- Do not introduce module-local header files unless explicitly requested for cross-directory API boundaries.

### WinAPI Philosophy
- The codebase stays close to **WinAPI style** — but uses snake_case for function and type names instead of PascalCase
- When implementing new features, think "how would this be done in WinAPI?" and follow those patterns
- If a required feature is missing from the core framework (e.g., hotkeys/accelerators, timers, clipboard), **add it to the framework** — do not implement workarounds in application code (e.g., do not handle `WM_KEYDOWN` manually where `WM_COMMAND` from an accelerator is the correct mechanism)
- Common WinAPI patterns to follow: message loops, window procedures, control notifications via `WM_COMMAND`, `HIWORD`/`LOWORD` packing, resource tables (menus, accelerators), dialog modal loops
- For plugin-provided **UI controls/windows**, use WinAPI-style integration only: register classes, instantiate by class name (`create_window(..., "class_name", ...)`), pass creation state via `lparam`, and communicate through messages/notifications (`evCommand`, control-specific messages). Do **not** design C function-table APIs for window controls unless the feature is explicitly non-window service logic.
- **Always search the existing framework before inventing new mechanisms.** Orion already has toolbars (`WINDOW_TOOLBAR`), toolbar buttons (`tbAddButtons`), bitmap strips (`bitmap_strip_t`), accelerators, dialogs, status bars, etc. If you need something that sounds like it belongs in a UI framework, look for it first.

### Reuse-First Architecture Rule (Required)

- **Add new primitives only when reuse is truly impossible.** If existing windows, classes, messages, flags, or layout metadata can express the behavior, use them.
- **Do not create parallel configuration models.** If orientation/state already exists in flags or class semantics, do not add duplicate fields like separate `layout_orientation` state.
- **Do not add duplicate declarative knobs for existing behavior.** If layout type is already represented by class/proc (`stack`, `grid`, `column`), do not add another independent selector that encodes the same thing.
- **Prefer class/proc-driven behavior over global mode switches.** In WinAPI style, behavior belongs to window classes and message handlers, not to app-wide enum dispatchers.
- **For layout engines, split by responsibility.** Avoid one monolithic layout function with long `if (stack)`, `if (grid)`, `if (column)` chains. Keep shared math in helpers, but keep container-specific measure/arrange logic in focused functions tied to each container type.

### Feature Implementation Playbook (Required)

- **Write the minimum code that delivers the behavior.** If the same functionality can be achieved with fewer moving parts, prefer the smaller design.
- **One concern, one owner.** Keep view responsibilities in views (hit-test/target resolution), controller responsibilities in controllers (validation/dispatch), and model responsibilities in model/document code (state mutation).
- **Resolve targets once.** Do not repeat hit-testing or target selection in multiple layers.
- **Keep payloads semantic-only.** Pass identifiers and intent; keep lifecycle/transient state in the subsystem that owns it.
- **Prefer flags over new booleans when the framework already has flags for the same behavior.** If WinAPI-style flags or existing Orion flags represent the state, use those flags instead of introducing parallel `bool` fields.
- **No parallel runtime/editor models.** Avoid duplicated representations for the same concept (for example, live window tree plus separate element tree with drift risk).
- **No runtime XML mutation for editor interactions.** Runtime editing should work on runtime state; XML stays in IO/load/save boundaries.
- **Prefer existing messages/flags/classes first.** Add new APIs only when existing primitives cannot express the behavior cleanly.
- **Delete superseded paths in the same change.** Do not keep old and new codepaths alive unless explicitly requested.
- **Use WinAPI-style flow as default.** Message-driven control flow, explicit ownership, and thin dispatch layers are the baseline.
- **Apply this to new domains (database support included).** Implement vertical slices that are small and direct: resolve target -> validate command -> mutate state -> relayout/refresh.

### Refactor Compression Direction

When shrinking or simplifying existing code, prefer the same direction used in the socialfeed database refactor:
- **Compress repeated logic into local helpers** instead of copying the same loop or branch structure several times.
- **Move variation into parameters or table data**, not into new wrapper functions for each type or case.
- **Remove thin named wrappers** when they only forward to a generic helper and do not add behavior.
- **Keep behavior-specific side effects in place** while genericizing the common mechanics around them.
- **Prefer table-driven dispatch over branch-heavy message handlers** when the table shapes are similar.
- **Collapse repeated load/save/fetch/insert patterns** into one same-file helper when the ownership stays local.
- **Delete superseded paths in the same change**; do not keep old and new forms of the same logic alive.
- **Strive for smaller code by reducing shape duplication**, not by moving code to another file just to hide it.

The goal is a smaller surface area with the same behavior: one canonical path for the common mechanics, and only the domain-specific differences left inline.

**Examples (Required Reading for New Features):**

```c
// Target resolution: do it once in canvas/view, pass resolved target forward.
// WRONG: controller re-runs hit-test.
window_t *target = canvas_find_component_drop_target(doc, type, x, y);
return fe_controller_drop_create(doc, &payload, target);

// RIGHT: canvas resolves; controller validates + dispatches only.
bool fe_controller_drop_create(window_t *doc,
                               const ui_drag_item_payload_t *payload,
                               window_t *target) {
  if (!doc || !payload || !target) return false;
  return fe_doc_drop_create_component(payload->tool_ident, target);
}
```

```c
// Payload design: semantic-only payload.
// WRONG: payload carries drag lifecycle state.
typedef struct {
  int tool_ident;
  bool dragging;
  bool entered_target;
} ui_drag_item_payload_t;

// RIGHT: payload carries only command intent.
typedef struct {
  int tool_ident;
} ui_drag_item_payload_t;
```

```c
// Flags over duplicate bools.
// WRONG: add a parallel bool that duplicates an existing flag.
typedef struct {
  bool auto_layout_enabled;
} form_props_state_t;

if (st->auto_layout_enabled)
  doc->flags |= WINDOW_AUTO_LAYOUT;

// RIGHT: use the existing window flags as source of truth.
if (doc->flags & WINDOW_AUTO_LAYOUT) {
  // auto-layout behavior
}
```

```c
// Reuse built-ins instead of creating parallel mechanisms.
// WRONG: custom toolbar/palette click plumbing for standard toolbar behavior.
// RIGHT: WINDOW_TOOLBAR + tbAddButtons + tbButtonClick.
window_t *w = create_window("Doc", WINDOW_TOOLBAR, &rc, parent, proc, hinst, NULL);
send_message(w, tbAddButtons, count, buttons);
```

```c
// Scrollbars: use built-in flags for scrollable content windows.
// WRONG: create custom child scrollbars for normal scrolling needs.
// RIGHT: set WINDOW_HSCROLL/WINDOW_VSCROLL and handle evHScroll/evVScroll.
window_t *view = create_window("View",
    WINDOW_HSCROLL | WINDOW_VSCROLL,
    &rc, parent, view_proc, hinst, NULL);
```

### Declarative Forms Must Be Self-Contained (Critical)

**Principle:** When declarative forms use full path syntax (`field="db.table.field"`), they must be completely self-contained. Never require external context as function parameters when the form already knows that context.

**Wrong - leaky abstraction:**
```c
// Form already knows db_name="db" from field="db.posts.title"
// Why pass it again? This breaks the declarative model.
show_db_dialog(&form, "Edit Post", parent, db, post_id);
                                           ^^
                                           redundant!
```

**Correct - self-contained:**
```c
// Form looks up database by name internally - no external context needed
show_db_dialog(&form, "Edit Post", parent, post_id);
```

**Implementation pattern:**
- Use a **registry pattern** for declarative resources (databases, themes, locales, etc.)
- Forms with `field="db.table.field"` → store `db_name`, look up via `get_database_by_name()`
- Forms with `theme="dark"` → look up via `get_theme_by_name()`
- Forms with `locale="fr_FR"` → look up via `get_locale_by_name()`
- Never duplicate context that's already encoded in the declarative definition

**Application startup pattern:**
```c
// Register resources once at startup
database_t *db = create_database("db", "SimpleXMLDatabase", "data.xml");
register_database("db", db);

// Now all forms with field="db.table.field" are self-contained
// No need to pass database instances around
```

**Why this matters:**
- ✅ True declarative programming - forms are pure data, not code
- ✅ Impossible to pass wrong database instance (compile-time guarantee)
- ✅ Consistent with "field= knows everything" philosophy
- ✅ Reduces function parameters and API surface
- ✅ Forms can be serialized, transmitted, hot-reloaded without code changes
- ❌ Passing external context breaks the declarative model
- ❌ Creates opportunity for runtime mismatch bugs (wrong db passed)
- ❌ Makes forms dependent on caller context instead of self-describing

**When to deviate:** Only if the resource is genuinely dynamic and cannot be named at form definition time (extremely rare). If you can name it, register it.

### Scrollbars — Built-in vs. Standalone

**`WINDOW_HSCROLL` / `WINDOW_VSCROLL` — built-in scrollbars on a window**

Set these flags at creation time on the window whose **content** scrolls.  The
framework paints the bars automatically and intercepts mouse events in their
area before calling `win->proc`.  Call `set_scroll_info()` to describe the
content range; handle `evHScroll` / `evVScroll` for
position changes.

```c
// Correct: built-in scrollbars on the scrollable content window
window_t *view = create_window("View",
    WINDOW_HSCROLL | WINDOW_VSCROLL,
    MAKERECT(0, 0, w, h), parent, my_view_proc, NULL);

// In evCreate (or whenever content/zoom changes):
scroll_info_t si = {
    .fMask = SIF_RANGE | SIF_PAGE | SIF_POS,
    .nMin  = 0,
    .nMax  = content_w,   // total content size
    .nPage = view_w,      // visible viewport size
    .nPos  = pan_x,       // current offset
};
set_scroll_info(view, SB_HORZ, &si, false);

// Handle scroll notifications:
case evHScroll:
    state->pan_x = (int)wparam;
    sync_scrollbars(win, state);
    invalidate_window(win);
    return true;
case evVScroll:
    state->pan_y = (int)wparam;
    sync_scrollbars(win, state);
    invalidate_window(win);
    return true;
```

**`win_scrollbar` — standalone scrollbar control**

When a scrollbar needs to exist as an independent child window (custom
layouts), use `win_scrollbar`.  Orientation comes from `lparam`:
`(void *)0` = horizontal, `(void *)1` = vertical.  Do **not** set
`WINDOW_HSCROLL` or `WINDOW_VSCROLL` on the scrollbar window itself.

```c
window_t *vsb = create_window("", WINDOW_NOTITLE | WINDOW_NOFILL,
    MAKERECT(w - 8, 0, 8, h - 8),
    parent, win_scrollbar, (void *)1 /* SB_VERT */);

scrollbar_info_t info = { 0, content_h, view_h, pos };
send_message(vsb, sbSetInfo, 0, &info);
```

**Common scrollbar mistakes to avoid**

| ❌ Wrong | ✅ Correct |
|---|---|
| Setting `WINDOW_HSCROLL` on a `win_scrollbar` child to indicate orientation | Pass `(void *)0` or `(void *)1` as `lparam`; those flags are for the **parent** |
| Creating `win_scrollbar` children when you want built-in scrollbars | Add `WINDOW_HSCROLL`/`WINDOW_VSCROLL` to the scrollable window and call `set_scroll_info()` |
| Manually painting scrollbar children from the parent `evPaint` | The framework paints built-in bars automatically after calling `win->proc` |
| Forwarding mouse events to scrollbar children | Not needed; the framework intercepts clicks in the bar area before `win->proc` |
| Handling `sbChanged` for built-in scrollbars | Handle `evHScroll` / `evVScroll` |
| Forgetting scrollbar interdependence | If one bar appears it shrinks the viewport on the other axis — re-check both `need_h` / `need_v` after setting either |

### Toolbars and Bitmap-Strip Icon Buttons

**WINDOW_TOOLBAR — built-in toolbar strip above a window's client area**

Any window can have a toolbar strip by setting the `WINDOW_TOOLBAR` flag at creation time. The strip is painted automatically by the framework above the title bar. Buttons are added with `tbAddButtons`, each described by a `toolbar_button_t {icon, ident, active}` where `icon` is a `sysicon_*` value from `user/icons.h`.

**Built-in system icons (sysicon_\* / SYSICON_BASE)**

Orion ships a 20x20 grid PNG icon sheet.  In the source tree, the asset lives at `share/icon_sheet_16x16.png`; at runtime it is deployed and loaded from `share/orion/icon_sheet_16x16.png` automatically at startup.  All ~398 icons are listed in `user/icons.h` as `sysicon_<name>` enum values starting at `SYSICON_BASE` (0x10000).  When a toolbar button's `icon` field is `>= SYSICON_BASE` the engine draws it from the built-in sheet — **no `tbLoadStrip` call is needed**.

```c
#include "user/icons.h"

// Correct: use sysicon_* values directly — framework sources them from
//          the built-in PNG sheet automatically.
static const toolbar_button_t kDocToolbar[] = {
  { sysicon_add,    ID_FILE_NEW,  0 },
  { sysicon_folder, ID_FILE_OPEN, 0 },
  { sysicon_save,   ID_FILE_SAVE, 0 },
};
send_message(doc, tbAddButtons,
             sizeof(kDocToolbar)/sizeof(kDocToolbar[0]),
             (void *)kDocToolbar);
```

For `win_toolbar_button` windows, use `ui_get_sysicon_strip()` to obtain the pre-loaded strip and pass `sysicon_X - SYSICON_BASE` as the index:

```c
bitmap_strip_t *s = ui_get_sysicon_strip();
if (s)
    send_message(btn, btnSetImage,
                 (uint32_t)(sysicon_add - SYSICON_BASE), s);
```

**win_toolbar_button + bitmap_strip_t — sprite-sheet icon buttons (TB_ADDBITMAP style)**

When icons come from a *custom* PNG sprite sheet (not the built-in sheet), use `win_toolbar_button` and `bitmap_strip_t`. This is the Orion equivalent of WinAPI's `TB_ADDBITMAP` / `TBBUTTON.iBitmap`:
- Load the strip once; store a single `bitmap_strip_t {tex, icon_w, icon_h, cols, sheet_w, sheet_h}`.
- Each button stores only an integer **index** (iBitmap). The icon at index `n` occupies tile `(n % cols, n / cols)`.
- Send `btnSetImage(wparam=index, lparam=&strip)` — the button owns a private copy, the caller needs no lifetime guarantee.

```c
// Correct: one strip loaded once, each button gets only an index
bitmap_strip_t strip = { .tex=tex, .icon_w=16, .icon_h=16,
                         .cols=2, .sheet_w=32, .sheet_h=160 };
for (int i = 0; i < NUM_TOOLS; i++) {
    window_t *btn = create_window(tool_names[i], flags,
        MAKERECT(bx, by, bw, bh), parent, win_toolbar_button, NULL);
    send_message(btn, btnSetImage, (uint32_t)tool_icon_idx[i], &strip);
}
```

**Common mistakes to avoid**

| ❌ Wrong | ✅ Correct |
|---|---|
| Per-button `{col, row, sheet_w, sheet_h}` stored in a custom struct | Single `bitmap_strip_t` shared across all buttons; each button stores only an **index** |
| Adding a `BUTTON_BITMAP` flag to `win_button` | Use `win_toolbar_button` proc (separate class for bitmap buttons) |
| Handling icon clicks in a custom `WM_LBUTTONDOWN` in a palette window proc | Use `WINDOW_TOOLBAR` + `tbAddButtons`; clicks fire `tbButtonClick` |
| Inventing a bespoke floating-window class for a toolbar | Use `WINDOW_TOOLBAR` on the document window, or a separate toolbar window with `win_toolbar_button` children |
| Hard-coding texture dimensions (`TOOLS_TEX_W/H`) | Derive `cols` from the actually loaded PNG width: `cols = loaded_w / icon_w` |
| Calling `tbLoadStrip` just to use common icons | Use `sysicon_*` values directly — the engine loads the built-in sheet automatically |

### Naming Conventions
- Use snake_case for function names (e.g., `create_window`, `draw_text_small`)
- Use snake_case with _t suffix for type names (e.g., `window_t`, `irect16_t`, `winproc_t`)
- Use SCREAMING_SNAKE_CASE for constants and macros (e.g., `WM_CREATE`, `SCREEN_WIDTH`)

### TurboVision-Inspired Enum Style (Preferred)
- Prefer short event/message enums with `ev` prefix (e.g., `evCreate`, `evPaint`, `evLeftButtonDown`, `evCommand`) instead of introducing new `WM_*` names.
- Prefer brush/theme color enums with `br` prefix ("brush"), e.g., `brWindowBg`, `brTextDisabled`, when adding or renaming public color identifiers.
- Keep control command enums short and verb-oriented (e.g., `btnSetImage`, `cbAddString`, `tbSetItems`) rather than long WinAPI-style all-caps names.
- For control notifications, keep existing notification enums consistent with the local subsystem style (e.g., `btnClicked`, `edUpdate`).
- Do not mix naming families inside a single touched module: if a file is already in `ev*` / `br*` style, continue that style.
- If compatibility aliases are needed for legacy or external code, add thin aliases/wrappers instead of changing runtime behavior.

### Code Style
- Use K&R-style bracing with opening brace on same line
- Use 2-space indentation
- Functions should have minimal comments unless explaining complex logic
- Header files use include guards with pattern `#ifndef __UI_SUBSYSTEM_H__`
- Prefer standard C types (int, bool, etc.) with stdint.h types when size matters (uint32_t, uint16_t)
- Use forward declarations to minimize header dependencies
- Do not line-wrap function declarations/prototypes. Keep each declaration on one line.
- Do not introduce variables that are used only once; inline the expression/value at the use site.
- In message handlers, do not create one-use aliases for `wparam`/`lparam` just to switch/branch. Prefer `switch (wparam)` or `switch (LOWORD(wparam))` directly unless the value is reused.

### Struct Design

**Organizing Large Structs with Inline Anonymous Structs**

When a struct accumulates many related fields (20+), organize them into logical groups using inline anonymous structs. This dramatically improves readability and makes field relationships explicit.

❌ **Bad: Flat 40+ field struct**
```c
typedef struct {
  uint8_t *pixels;
  int canvas_w, canvas_h;
  layer_t **layers;
  int layer_count;
  int active_layer;
  bool editing_mask, mask_only_view;
  uint8_t *composite_buf;
  uint8_t *undo_states[UNDO_MAX];
  int undo_count;
  uint8_t *redo_states[UNDO_MAX];
  int redo_count;
  bool sel_active;
  int sel_start_x, sel_start_y;
  int sel_end_x, sel_end_y;
  uint8_t *sel_mask;
  GLuint sel_mask_tex;
  bool sel_mask_dirty;
  int sel_mask_offset_x, sel_mask_offset_y;
  bool sel_moving;
  bool sel_mask_moving;
  int sel_move_origin_x, sel_move_origin_y;
  int sel_floating_x, sel_floating_y;
  int sel_floating_w, sel_floating_h;
  uint8_t *sel_floating_pixels;
  // ... 20 more fields
} canvas_doc_t;
```

✅ **Good: Organized with inline structs and geometry types**
```c
typedef struct {
  // Minimal reusable undo/redo struct
  uint8_t *states[UNDO_MAX];
  int      count;
} undo_t;

typedef struct canvas_doc_s {
  // Core document fields
  uint8_t *pixels;
  int canvas_w, canvas_h;
  
  // Layer management (inline struct groups related fields)
  struct {
    layer_t **stack;
    int count, active;
    bool editing_mask, mask_only_view;
    uint8_t *composite_buf;
  } layer;
  
  // Separate undo/redo instances
  undo_t undo;
  undo_t redo;
  
  // Selection with nested substructs (3 levels deep is fine)
  struct {
    bool active;
    ipoint16_t start, end;    // Use geometry types, not separate x/y
    bool add_mode;
    
    struct {
      uint8_t *data;          // The mask bitmap
      GLuint tex;
      bool dirty;
      ipoint16_t offset;      // Mask position relative to canvas
    } mask;
    
    struct {
      bool active;            // Currently dragging selection
      bool mask_moving;       // Moving mask vs. pixels
      ipoint16_t origin;      // Drag start point
    } move;
    
    struct {
      ipoint16_t pos;         // Top-left position
      isize16_t size;         // Width and height
      uint8_t *pixels;        // RGBA data
      uint8_t *mask;          // Edit mask
      GLuint tex;
    } floating;
  } sel;
  
  // Shape and polygon tools
  struct { uint8_t *snapshot; ipoint16_t start; } shape;
  struct { ipoint16_t pts[256]; int count; bool active; } poly;
} canvas_doc_t;
```

**Field Access Updates**
When refactoring, update all field accesses systematically:
- `doc->layers` → `doc->layer.stack`
- `doc->layer_count` → `doc->layer.count`
- `doc->undo_states` → `doc->undo.states`
- `doc->sel_mask` → `doc->sel.mask.data`
- `doc->sel_floating_x` → `doc->sel.floating.pos.x`
- `doc->sel_floating_w` → `doc->sel.floating.size.w`

**Benefits of This Pattern**
- Instantly see conceptual groupings (layer state, selection state, etc.)
- Nested substructs make ownership clear (`sel.mask` belongs to selection)
- Geometry types eliminate parallel `_x/_y` and `_w/_h` fields
- Extractable components (undo_t can be reused in other documents)
- Much easier to reason about state changes and memory management

**Geometry Types for Coordinate Pairs**

- Prefer geometry structs wherever possible instead of loose coordinate fields: use `ipoint16_t { x, y }` for positions/deltas, `isize16_t { w, h }` for sizes, and `irect16_t { x, y, w, h }` for rectangles.
- `ipoint16_t`, `isize16_t`, and `irect16_t` are defined in `user/user.h` and available everywhere via `ui.h`.
- Do not scatter parallel `_x` / `_y`, `_w` / `_h`, or `_start` / `_end` fields across a struct when a point, size, or rect member would be cleaner.
- Prefer existing rectangle/geometry helper functions from `user/rect.h` (`rect_offset`, `rect_inset`, `rect_center`, `rect_split_*`, `rect_trim_*`, etc.) over open-coded coordinate math.
- If a needed geometry helper is missing, add a small reusable helper to the framework (usually `user/rect.h`) instead of duplicating ad hoc math in application code.

### State, Geometry, and Coordinate-Space Discipline
- Prefer tagged state over flat "all fields are live" structs.  If behavior has modes (`none`, `move`, `resize`, `place`, etc.), store a mode plus a union whose members contain only the fields valid for that mode.  Avoid parallel fields like `drag_mode`, `drag_handle`, `drag_start`, `snap_rect`, `rb`, and `placing_type` all living side-by-side.
- Make invalid states hard to represent.  Reset mode state with one aggregate assignment such as `state->drag = (drag_state_t){.mode = DRAG_NONE};` instead of clearing unrelated fields one by one.
- Use typed points for distinct coordinate spaces when a module mixes them.  For example, a design surface may define `canvas_pt_t` and `form_pt_t` even if both are `{x, y}`.  Convert through a small pair of helpers (`form_to_canvas_pt`, `canvas_to_form_pt`) and derive rect conversion from those helpers.
- Keep drawing and hit-testing in explicit coordinate spaces.  If child paints alter viewport/projection state, restore the parent/window draw space before drawing parent-owned adornments such as selection outlines, handles, grids, or rubber bands.
- Prefer rect-valued APIs over long coordinate parameter lists.  A helper like `canvas_update_preview(state, type, form_rc, text, flags)` is preferred over `canvas_update_preview(state, type, x, y, w, h, text, flags)`.
- Prefer a single conversion for a shape.  Compute one `irect16_t canvas_rc = form_to_canvas_rect(state, form_rc)` and pass it through drawing helpers; do not pass `fx, fy, fw, fh` as separate values unless the callee truly needs independent scalars.
- For resize handles or edge-affecting logic, use a small table describing which edges move (`left`, `top`, `right`, `bottom`) rather than repeating boolean chains and duplicated snap/clamp logic.
- Shared text/name generation should live in one helper.  For example, use `ctrl_make_caption(type, index, buf, len)` for both stored captions and previews instead of separate "preview label" functions with subtly duplicated rules.
- When finalizing mouse drags, derive the final rectangle from the actual mouse-up point.  Do not compute `lx`/`ly` and then suppress them with `(void)lx; (void)ly`; that is usually a sign the state machine is too implicit.

### Message-Based Architecture
- All UI interaction uses a Windows-style message system
- Window procedures follow the signature: `result_t (*winproc_t)(window_t *, uint32_t msg, uint32_t wparam, void *lparam)`
- Common messages include WM_CREATE, WM_DESTROY, WM_PAINT, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_KEYDOWN, WM_KEYUP, WM_COMMAND
- Return true from window proc if message was handled, false otherwise
- **Do not add dispatcher-level safety nets that call `default_winproc` when a proc returns unhandled.** Keep DefWindowProc behavior explicit in each window proc (`default: return default_winproc(...)`) and fix missing delegation at the source proc instead of adding global fallback hacks in `send_message`.

### Message Parameter Passing (Zero Wrapper Structs)

**Design Principle:** Pass values directly via wparam/lparam without wrapper structs. This matches WinAPI conventions and maximizes API simplicity.

**Rules:**
- **No parameter structs** — Pass simple values directly as wparam (uint32_t) or lparam (void*/intptr_t)
- **Pack multiple IDs** — Use `MAKEDWORD(lo, hi)` to pack two 16-bit values into wparam; unpack with `LOWORD(wparam)` / `HIWORD(wparam)`
- **Cast appropriately** — For integers: `(void *)(intptr_t)value` or `(int)(intptr_t)lparam`; for strings: `(const char *)lparam`
- **Return results directly** — Use `lresult_t` (pointer-sized like WinAPI `LRESULT`) to return pointers, booleans, or integers
- **Linked lists over arrays** — For variable-length results, return linked list head instead of array+count_out struct

**Example (Database API):**
```c
// ✅ Insert: wparam carries table_id, lparam carries record data directly
author_t *inserted = (author_t *)send_db_message(db, dbInsert, TABLE_AUTHORS, &author);

// ✅ Find: MAKEDWORD packs table_id + search_field, lparam is value or string
author_t *found = (author_t *)send_db_message(db, dbFind, 
  MAKEDWORD(TABLE_AUTHORS, 0), (void *)(intptr_t)author_id);

// ✅ Fetch: Returns linked list, lparam is filter value directly
result_node_t *results = (result_node_t *)send_db_message(db, dbFetch,
  MAKEDWORD(TABLE_COMMENTS, 2), (void *)(intptr_t)post_id);
int count = count_result_list(results);
free_result_list(results);

// ❌ Wrong: Unnecessary wrapper struct
typedef struct { int table_id; void *record_data; } insert_params_t;
insert_params_t params = { TABLE_AUTHORS, &author };
send_db_message(db, dbInsert, 0, &params);  // Don't do this!
```

**Benefits:**
- Zero boilerplate struct definitions and initializations
- Direct value passing matches WinAPI message conventions
- MAKEDWORD/LOWORD/HIWORD standard for packing related values
- Linked lists eliminate count_out output parameters
- Code is shorter, clearer, and faster

**When to deviate:** Only use a parameter struct if the message genuinely requires more than 3 values that cannot be decomposed into separate messages or packed into wparam. This should be extremely rare.

### Confirmation Dialogs
- Match the button set to the question being asked. A two-choice question such
  as "Close without saving?", "Discard changes?", or "Delete selected item?"
  must use `MB_YESNO`, where `IDYES` performs the destructive/continuing action
  and `IDNO` leaves state unchanged.
- Use `MB_YESNOCANCEL` only for a genuine three-way decision, such as
  "Save changes before closing?" where `IDYES` saves, `IDNO` discards, and
  `IDCANCEL` aborts the close.
- For window `evClose` handlers, return true to cancel/consume the close when
  the user chooses `IDNO` or `IDCANCEL`; only proceed with closing after the
  explicit affirmative action for the dialog wording.

### Drawing and Rendering
- Use OpenGL for all rendering (hardware accelerated)
- Text rendering uses small bitmap font (6x8 pixels)
- Drawing functions include: `draw_text_small()`, `draw_rect()`, `fill_rect()`, `draw_icon8()`, `draw_icon16()`
- Always call `init_text_rendering()` at startup and `shutdown_text_rendering()` at cleanup
- Colors are specified as RGBA uint32_t values
- Use `ui_begin_frame()` and `ui_end_frame()` to manage rendering context

### Memory Management
- Use malloc/free for dynamic allocations
- Window structures are managed by Orion - don't manually free them
- Always pair init functions with corresponding shutdown functions

## Common Tasks

### Adding a New Control
1. Create implementation file in `commctl/` directory (e.g., `newcontrol.c`)
2. Add window procedure function following pattern `win_newcontrol()`
3. Declare the window procedure in `commctl/commctl.h`
4. Handle at minimum: WM_CREATE, WM_PAINT, WM_DESTROY, and any control-specific messages
5. Add usage example to README.md if it's a major control

### Creating a Dialog or Panel with Multiple Controls (use forms)

**Always use `form_def_t` + `show_dialog_from_form()` for any dialog or panel
that contains two or more standard controls.**  Never build children imperatively
inside `evCreate` when a static form definition can express the
same layout.

**Prefer `.orion` XML files over in-code form definitions** — the orionc compiler generates clean C headers from declarative XML, making dialogs easier to maintain and preview.

#### Option 1: Declarative .orion Files (Recommended)

Create an `.orion` XML file in your example directory:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<orion version="1" name="myapp" title="My App Dialogs">
  <forms>
    <form name="my_dialog"
          title="My Dialog"
          frame="0 0 200 100"
          flags="WINDOW_DIALOG | WINDOW_NOTRAYBUTTON"
          auto_layout="1"
          spacing="8"
          padding="8">
      <grid name="fields" spacing="4">
        <column name="labels" width="48">
          <label name="lbl_name" text="Name:" />
        </column>
        <column name="inputs" flags="WINDOW_FLEXSPACE">
          <textedit name="edit_name" value="1" text="" />
        </column>
      </grid>
      <separator name="sep" />
      <stack name="actions" orientation="horizontal" spacing="6">
        <space name="flex" />
        <button name="ok" value="2" text="OK" flags="BUTTON_DEFAULT" />
        <button name="cancel" value="3" text="Cancel" />
      </stack>
    </form>
  </forms>
</orion>
```

Compile with orionc:
```bash
build/bin/orionc --input examples/myapp/myapp.orion \
                 --output build/generated/examples/myapp/myapp.h \
                 --prefix myapp
```

Use in code:
```c
#include "build/generated/examples/myapp/myapp.h"

// Children already exist and are laid out automatically
show_dialog_from_form(&myapp_form_my_dialog, "My Dialog", parent, my_dlg_proc, &st);
```

#### Option 2: In-Code Form Definitions (Legacy)

```c
// ── 1. Declare children (static, compile-time) ────────────────────
static const form_ctrl_def_t kMyDlgChildren[] = {
  { FORM_CTRL_TEXTEDIT, 1, {60, 8, 80, 13}, 0,              "",       "name"   },
  { FORM_CTRL_BUTTON,   2, {50,30, 40, 13}, BUTTON_DEFAULT, "OK",     "ok"     },
  { FORM_CTRL_BUTTON,   3, {94,30, 50, 13}, 0,              "Cancel", "cancel" },
};
static const form_def_t kMyDlg = {
  .name="My Dialog", .w=160, .h=52,
  .children=kMyDlgChildren, .child_count=3,
};

// ── 2. Window procedure — children already exist at evCreate ──
static result_t my_dlg_proc(window_t *win, uint32_t msg,
                             uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      win->userdata = lparam;
      set_window_item_text(win, 1, "default value"); // populate at runtime
      return true;
    case evPaint:
      draw_text_small("Name:", 4, 11, get_sys_color(brTextDisabled));
      return false;
    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (src->id == 2) { end_dialog(win, 1); return true; }
        if (src->id == 3) { end_dialog(win, 0); return true; }
      }
      return false;
    default: return false;
  }
}

// ── 3a. Show as modal dialog ────────────────────────────────────────
// show_dialog_from_form() auto-centers, adds WINDOW_DIALOG flags, runs loop.
show_dialog_from_form(&kMyDlg, "My Dialog", parent, my_dlg_proc, &st);

// ── 3b. Instantiate as modeless / embedded window ──────────────────
create_window_from_form(&kMyDlg, x, y, parent, my_dlg_proc, NULL);
```

**Key rules:**
- `form_ctrl_def_t` supports: `FORM_CTRL_BUTTON`, `FORM_CTRL_CHECKBOX`,
  `FORM_CTRL_LABEL`, `FORM_CTRL_TEXTEDIT`, `FORM_CTRL_LIST`, `FORM_CTRL_COMBOBOX`.
- `show_dialog_from_form()` handles centering and dialog flags — no
  `MAKERECT((sw-W)/2, ...)` boilerplate needed.
- Runtime values (initial edit text, checkbox states) are set inside
  `evCreate` via `set_window_item_text()` / `get_window_item()` —
  the children are already present when that message fires.
- For modeless top-level windows or custom form instantiation, use
  `center_window_rect()` after `adjust_window_rect()` instead of duplicating
  screen/owner centering logic locally.
- The form editor (see `examples/formeditor/`) can generate the struct literals
  directly in its saved `.h` output.

### Auto-Layout System

Orion provides a **WPF-inspired auto-layout system** for dynamic window sizing and positioning. Enable it on a form or container by including `WINDOW_AUTO_LAYOUT` in the window flags; children are then arranged automatically using stack or grid layouts.

#### Layout Properties

**Container type** is determined by `class_name` (or the element tag in `.orion` XML):
- `"stack"` — arranges children in a row or column
- `"grid"` — arranges children using explicit `<column>` elements (star sizing)
- `"column"` — single column of a grid container

**Orientation and flags on a stack control:**
- `WINDOW_STACK_HORIZONTAL` in `flags` — horizontal (left-to-right) layout
- Without `WINDOW_STACK_HORIZONTAL` — vertical (top-to-bottom) layout (default)
- `WINDOW_FLEXSPACE` — child expands to fill remaining space along the stack axis

**Per-window layout state** (`win->layout.*`):
- `win->layout.layout_spacing` — gap between direct children (pixels)
- `win->layout.layout_padding` — inner padding `irect16_t {l, t, r, b}`
- `win->layout.layout_margin` — outer margin when inside a layout container
- `win->layout.layout_fixed_w` / `win->layout.layout_fixed_h` — declarative size hints

**Child alignment** (`win->layout.h_align` / `win->layout.v_align`): `LAYOUT_ALIGN_STRETCH` (default), `LAYOUT_ALIGN_START`, `LAYOUT_ALIGN_CENTER`, `LAYOUT_ALIGN_END`

#### Stack Layout

Arranges children in a single row (horizontal) or column (vertical). Use `WINDOW_FLEXSPACE` on children that should expand to fill remaining space.

```xml
<stack name="toolbar" orientation="horizontal" spacing="4" padding="4">
  <button name="new" text="New" width="60" />
  <button name="open" text="Open" width="60" />
  <space name="flex" />  <!-- Expands to push buttons right -->
  <button name="quit" text="Quit" width="60" />
</stack>
```

**Stack semantics:**
- Children are measured first to determine their minimum size
- Fixed-size children keep their specified dimensions
- `WINDOW_FLEXSPACE` children divide remaining space equally
- Spacing is added between children (not before first or after last)

#### Grid Layout (WPF Grid Semantics)

Arranges children in rows and columns using explicit `<column>` elements. Each column contains child elements stacked vertically. **Columns automatically share available width equally** unless given explicit width — no flags needed.

```xml
<!-- Column-based grid with auto-width columns -->
<grid name="main" spacing="24">
  <column name="preview_col" spacing="6">
    <filter_preview name="preview" flags="WINDOW_NOTITLE | WINDOW_NOFILL" />
    <label name="filter_name" text="No filters loaded" />
  </column>
  <column name="filters_col" spacing="6">
    <reportview name="filters" flags="WINDOW_NOTITLE | WINDOW_NORESIZE" />
  </column>
</grid>
```

**Grid star sizing (Width="*" equivalent):**
- Columns **without** `layout_fixed_w` divide available horizontal space equally
- Example: Two auto-width columns → each gets 50% of container width (minus spacing)
- Fixed-width columns are allocated first, then remaining space is distributed to auto-width columns
- This matches WPF `<ColumnDefinition Width="*" />` behavior

**Common grid patterns:**
```xml
<!-- Two equal columns -->
<grid spacing="12">
  <column spacing="4">...</column>
  <column spacing="4">...</column>
</grid>

<!-- Fixed sidebar + flexible content -->
<grid spacing="8">
  <column spacing="4" width="200">...</column>  <!-- Fixed 200px -->
  <column spacing="4">...</column>              <!-- Gets remaining space -->
</grid>
```

**Grid mistakes to avoid:**
- ❌ Adding `WINDOW_FLEXSPACE` to grid columns (that's for stacks)
- ❌ Mixing `frame=` attributes on auto-layout children (conflicts with layout system)
- ❌ Using `columns="N"` attribute (removed - grids must use explicit `<column>` elements)
- ✅ Let grid handle column widths automatically
- ✅ Use `spacing` for consistent gaps between cells
- ✅ Use explicit `<column>` elements for all grid layouts

#### Critical Layout Rules (Lessons Learned)

**Grid Column Layout (REQUIRED):**
- Grid columns **must** be specified using explicit `<column>` child elements
- The `columns="N"` attribute is **removed** - attempting to use it will fail compilation
- Each `<column>` element contains children stacked vertically
- Columns without `width` attribute share available space equally (WPF star sizing)
- Columns with explicit `width` get fixed allocation, remaining space divided among auto-width columns
- Example: Label+Input forms should use `<column width="48">` for labels, `<column>` for inputs

**WINDOW_FLEXSPACE Usage:**
- Use `WINDOW_FLEXSPACE` on **direct children of stacks** that should expand along the stack axis
- Use `WINDOW_FLEXSPACE` on the **grid itself** when the grid should expand within its parent stack
- Do **not** use `WINDOW_FLEXSPACE` on individual controls inside grid columns
- Do **not** use `WINDOW_FLEXSPACE` on `<column>` elements (star sizing is automatic)
- For controls that need scrolling (reportview, multiedit), use `WINDOW_VSCROLL` on the control itself

**<space> vs <separator> (CRITICAL):**
- `<space />` **expands to fill all available space** along the stack axis — use only when you want flex expansion
- `<separator />` draws a visual divider line **without expansion** — use for visual separation between sections
- **In fixed-height dialogs**, always use `<separator>` before action buttons, never `<space>` (which pushes buttons off-screen)
- **In dynamic-height layouts**, use `<space>` in stacks with `WINDOW_FLEXSPACE` content to push elements apart

```xml
<!-- WRONG: <space> in fixed-height dialog pushes buttons too far down -->
<form frame="0 0 200 100" auto_layout="1">
  <grid>...</grid>
  <space name="spacer" />  <!-- Expands infinitely, buttons cut off! -->
  <stack name="actions">...</stack>
</form>

<!-- CORRECT: <separator> provides visual break without expansion -->
<form frame="0 0 200 100" auto_layout="1">
  <grid>...</grid>
  <separator name="sep" />  <!-- Just a line, buttons visible -->
  <stack name="actions">...</stack>
</form>

<!-- CORRECT: <space> in horizontal stack pushes buttons to edges -->
<stack name="actions" orientation="horizontal">
  <button text="Help" />
  <space />  <!-- Expands horizontally, pushes OK/Cancel right -->
  <button text="OK" />
</stack>
```

**Common Dialog Patterns:**

*Label-Input Dialog (2 columns):*
```xml
<form name="my_dialog" auto_layout="1" spacing="8" padding="8">
  <grid name="fields" spacing="4">
    <column name="labels" width="48">
      <label name="lbl_name" text="Name:" />
      <label name="lbl_email" text="Email:" />
    </column>
    <column name="inputs" flags="WINDOW_FLEXSPACE">
      <textedit name="name" />
      <textedit name="email" />
    </column>
  </grid>
  <separator name="sep" />
  <stack name="actions" orientation="horizontal" spacing="6">
    <space name="flex" />
    <button name="ok" text="OK" flags="BUTTON_DEFAULT" />
    <button name="cancel" text="Cancel" />
  </stack>
</form>
```

*Scrolling List Dialog:*
```xml
<form name="list_dialog" auto_layout="1" padding="8">
  <reportview name="list" flags="WINDOW_VSCROLL | WINDOW_FLEXSPACE" />
  <separator name="sep" />
  <stack name="actions" orientation="horizontal" spacing="6">
    <space />
    <button name="ok" text="OK" flags="BUTTON_DEFAULT" />
  </stack>
</form>
```

**Why These Rules Matter:**
- Fixed label column widths prevent "10px wide column" layout bugs
- Column-based grids provide explicit, predictable control over layout
- `WINDOW_FLEXSPACE` on grids allows them to fill parent containers properly
- Separating flex concerns (grid expansion vs. cell content) eliminates confusion
- These patterns match macOS HIG and WinAPI dialog conventions

#### Form Sizing Guidelines

**Standard Control Heights:**
- `button`: 19px (standard button height)
- `textedit`: 13px (single-line edit box)
- `label`: 13px (standard text label)
- `checkbox`: 13px (checkbox with label)
- `combobox`: 13px (dropdown control)
- `separator`: 1px (visual divider line)
- `slider`: 12-16px (depends on style)
- `reportview`, `multiedit`: Variable (use `WINDOW_FLEXSPACE` for expansion)

**Standard Spacing:**
- Dialog padding: 8-10px (outer margin around all content)
- Grid spacing: 4px (gap between label and input columns)
- Stack spacing (form sections): 6-8px (gap between control groups)
- Action button spacing: 6px (gap between OK/Cancel buttons)

**Calculating Fixed-Height Dialog Dimensions:**

For dialogs with no scrolling or expandable content, calculate height manually:

```xml
<!-- Example: "New Image" dialog height calculation -->
<form frame="0 0 180 84" auto_layout="1" spacing="8" padding="8">
  <!-- Top padding:        8px -->
  <grid spacing="4">     <!-- 2 rows: 13px + 4px + 13px = 30px -->
    <column width="48">
      <label />          <!-- 13px -->
      <label />          <!-- 13px -->
    </column>
    <column>
      <textedit />       <!-- 13px -->
      <textedit />       <!-- 13px -->
    </column>
  </grid>
  <!-- Spacing:            8px -->
  <separator />          <!-- 1px -->
  <!-- Spacing:            8px -->
  <stack>                <!-- 19px -->
    <button />           <!-- 19px -->
  </stack>
  <!-- Bottom padding:     8px -->
</form>
<!-- Total: 8 + 30 + 8 + 1 + 8 + 19 + 8 = 82px (rounds to 84px) -->
```

**When to Use Fixed vs. Flexible Sizing:**

| Dialog Type | Width | Height | Flags |
|---|---|---|---|
| Simple forms (2-5 inputs) | Fixed (180-220px) | Fixed (calculated) | None |
| Label+input grids | Fixed or flexible | Fixed (calculated) | None on columns (auto star sizing) |
| Scrolling lists | Fixed or flexible | Flexible | `WINDOW_VSCROLL` + `WINDOW_FLEXSPACE` on list |
| Multi-edit/console | Flexible | Flexible | `WINDOW_VSCROLL` + `WINDOW_FLEXSPACE` on control |
| Preview + controls | Flexible | Flexible | `WINDOW_FLEXSPACE` on preview/content area |

**Future Enhancement - Automatic Height Calculation:**
The framework could automatically detect when a form contains only fixed-height controls (no `WINDOW_FLEXSPACE` flags anywhere in the child tree) and auto-calculate the form height.

**Key insight**: The form definition (`form_def_t`) contains all child control definitions (`form_ctrl_def_t[]`) as a **static structure** before any windows are created. Each `form_ctrl_def_t` has a `flags` field, so we can scan for `WINDOW_FLEXSPACE` before positioning/creating the window:

```c
// In show_dialog_from_form_ex, before centering the window:
form_def_t dlg_def = *def;  // Local copy

// Auto-calculate height if no flexible content exists
if ((dlg_def.flags & WINDOW_AUTO_LAYOUT) && !form_has_flexspace(&dlg_def)) {
    dlg_def.height = calculate_form_height(&dlg_def);
}

// Now center/create with correct height
irect16_t dlg_rect = center_window_rect(..., dlg_def.height);
```

Helper function scans the static definition recursively:

```c
bool form_has_flexspace(const form_def_t *def) {
    return form_children_have_flexspace(def->children, def->child_count);
}

bool form_children_have_flexspace(const form_ctrl_def_t *children, int count) {
    for (int i = 0; i < count; i++) {
        if (children[i].flags & WINDOW_FLEXSPACE)
            return true;
        // Check nested children recursively
        if (children[i].children && 
            form_children_have_flexspace(children[i].children, children[i].child_count))
            return true;
    }
    return false;
}
```

This would mean:
- ✅ No explicit `auto_height="1"` flag needed
- ✅ Framework scans **static form definition** before creating windows
- ✅ Height calculation happens before positioning/centering
- ✅ Manual height calculation errors eliminated automatically
- ✅ Forms with `WINDOW_FLEXSPACE` still use specified height
- ✅ Simple forms just work: `frame="0 0 180 0"` with height auto-calculated

Currently, height must be specified manually even for fixed-content forms.

#### .orion XML Reference

**Form attributes:**
- `name`: Form identifier (becomes `<prefix>_form_<name>` in generated code)
- `title`: Window title
- `frame`: Initial client rect as "x y w h" (window size before chrome)
- `flags`: Window flags (e.g., `WINDOW_DIALOG | WINDOW_NOTRAYBUTTON`)
- `spacing`: Default gap between children in pixels
- `padding`: Inner padding as "left top right bottom" or single value

> **Note:** Auto-layout is always enabled for forms. The layout container type is set by
> the child element tag (`<stack>`, `<grid>`, `<column>`), not by a form-level `layout=` attribute.

**Control elements:**
```xml
<button name="id" value="123" text="Label" width="60" height="19" flags="BUTTON_DEFAULT" />
<label name="id" text="Label" color="text-disabled" font="system" />
<textedit name="id" value="123" text="initial" flags="WINDOW_FLEXSPACE" />
<checkbox name="id" value="123" text="Label" />
<combobox name="id" value="123" flags="WINDOW_FLEXSPACE" />
<reportview name="id" value="123" flags="WINDOW_NOTITLE | WINDOW_NORESIZE" />
<space name="id" />  <!-- Spacer that expands in stacks -->
```

**Layout containers:**
```xml
<stack name="id" orientation="horizontal|vertical" spacing="6" padding="4">
  <!-- Children arranged in a line -->
</stack>

<grid name="id" spacing="12">
  <column name="col1" spacing="4" width="200">
    <!-- Column children stacked vertically -->
  </column>
  <column name="col2" spacing="4">
    <!-- Auto-width column -->
  </column>
</grid>
```

**Compilation:**
The `orionc` compiler processes `.orion` files in your Makefile:
```make
build/generated/examples/myapp/myapp.h: examples/myapp/myapp.orion
	build/bin/orionc --input $< --output $@ --prefix myapp
```

Generated code creates `form_def_t` structures you reference directly:
```c
show_dialog_from_form(&myapp_form_my_dialog, "Title", parent, proc, state);
```

### Creating Example Programs
1. Place examples in `examples/` directory
2. Include `../ui.h` for all UI functionality
3. Follow the pattern in `examples/helloworld.c`
4. Document build and run instructions in `examples/README.md`
5. Always initialize with `ui_init_graphics()` and cleanup with `ui_shutdown_graphics()`

### MVC Split for Example Applications

Orion example apps use a **Model–View–Controller** split inspired by PHP backend patterns (Laravel / Symfony style), mapped onto the Orion message-loop frontend.

| Layer | PHP analogy | Orion convention |
|---|---|---|
| **Model** | Eloquent model / database layer | `model_*.c` — data structs + CRUD; no window handles, no draw calls |
| **View** | Blade template / response renderer | `view_*.c` — window procedures, dialog procs, `evPaint` handlers |
| **Controller** | HTTP controller / route handler | `controller_*.c` — `app_state_t`, command dispatch, glue between model and view |

**Rules:**
- Models own allocation, mutation, and free. They must not touch any window or draw API.
- Views own all window procedures (`evCreate`, `evPaint`, `evCommand`…). They read from the model through the controller; they never mutate model state directly — they call a controller function or a model CRUD function.
- The controller holds the single `app_state_t *g_app` global, assigns document IDs (analogous to Appwrite/database auto-increment), dispatches menu and toolbar commands, and manages global view references (`main_win`, `feed_win`, etc.).
- The entry-point file (`main.c` / `gem_init`) seeds initial data by calling the model's create functions through the controller's `app_add_*` helpers, then creates the top-level view windows.

```
examples/socialfeed/
  model_feed.c        ← post_t / comment_t CRUD (no UI)
  controller_app.c    ← app_state_t, app_add_post/comment/reply, handle_menu_command
  view_main.c         ← main window + feed list proc
  view_dlg_post.c     ← post detail dialog proc
  view_dlg_forms.c    ← new-post / new-comment dialog procs
  view_menubar.c      ← menu bar proc
  main.c              ← gem_init: seed data, wire up windows
```

### Designer/App Architecture for Complex Examples

For complex tools such as `examples/formeditor/`, follow an Xcode/Interface Builder-style split. Do not let one window file become the document model, canvas renderer, serializer, property inspector, layout engine, and command dispatcher at the same time.

**Required layers for designer-style apps:**
- **Project/workspace layer** (`project_*.c`): owns project metadata, document list, plugin references, open/save orchestration. It must not contain menu window procs or canvas hit-testing.
- **Document model layer** (`model_*.c`, `document_*.c`): owns persistent structs, IDs, validation, CRUD, dirty state, and serialization-ready data. Persistent model structs must not store `window_t *`, OpenGL resources, or design-time preview state.
- **Editor context/controller layer** (`controller_*.c`, `editor_context_*.c`): owns active document, active selection, active tool, command dispatch, and refresh/notification fan-out.
- **Canvas/view layer** (`view_canvas*.c`): owns painting, hit-testing, drag state, coordinate conversion, live design-time child windows, and visual adornments. It edits the document only through commands/controller helpers.
- **Inspector/library/palette views** (`view_property_*.c`, `view_library_*.c`, `view_palette_*.c`): render model metadata and send commands. They do not parse project files or directly rewrite document arrays.
- **Layout engine layer** (`layout_*.c`): owns measure/arrange/reflow/drop-target calculations. Loading a file should not contain layout algorithms inline.
- **Archiver layer** (`archive_*.c`, `project_io_*.c`): owns XML/binary read/write. UI files such as menubars and canvases must not contain project parsers.

**Represented model vs. live design-time view**
- Keep persistent document objects pure. A form/control model may store IDs, type/class tokens, text, flags, rects, padding, margins, bindings, and style properties.
- Never put live runtime/design-time handles in persistent structs. Avoid fields like `window_t *live_win` inside `form_element_t` or equivalent model types.
- The canvas/editor runtime owns mappings between represented objects and live windows/resources:
```c
typedef struct {
  uint32_t element_id;
  window_t *live_win;
} designer_live_view_ref_t;
```
- Treat live controls as an implementation detail of the editor surface. Destroy/rebuild them from the model whenever needed.

**Metadata-driven component system**
- Component behavior should come from a registry/descriptor, not switch statements scattered through canvas, property browser, project I/O, and palettes.
- A component descriptor should be the single source for class token, display name, default size, design-placeable flag, window proc/class name, property schema, and archive hooks.
- Adding a component should mostly add metadata and component-specific handlers, not edit five unrelated `switch(type)` blocks.

**Metadata-driven properties and inspectors**
- Property browsers must render from property descriptors, not hand-coded rows for every field and component type.
- Use table-driven get/set/parse/format helpers. The inspector sends a command such as `designer_cmd_set_property(...)`; it does not mutate model fields directly.
- Shared properties (name, id, text, frame, margins, padding, alignment) should be declared once and reused across component descriptors.

**Commands and notifications**
- All model mutations from UI go through named commands:
```c
designer_cmd_add_element(ctx, component_id, parent_id, frame);
designer_cmd_delete_selection(ctx);
designer_cmd_set_property(ctx, object_id, property_id, value);
designer_cmd_move_element(ctx, object_id, frame);
```
- Commands are responsible for marking documents dirty, updating titles, triggering layout reflow, syncing live views, and notifying panels.
- Prefer simple app notifications over manual refresh chains:
```c
designer_notify(ctx, DESIGNER_EVENT_ACTIVE_DOCUMENT_CHANGED);
designer_notify(ctx, DESIGNER_EVENT_SELECTION_CHANGED);
designer_notify(ctx, DESIGNER_EVENT_DOCUMENT_MUTATED);
designer_notify(ctx, DESIGNER_EVENT_COMPONENT_REGISTRY_CHANGED);
```
- Views subscribe/respond. Do not have low-level code directly call every panel refresh function it happens to know about.

**File organization rules**
- Menubar files should define menu resources, the menubar window proc, and command forwarding only. They must not own document creation/destruction, project XML parsing, layout algorithms, or property dialogs.
- Canvas files should not save/load projects, own global app state, or decide component metadata. They may call document commands and layout/canvas helpers.
- Header files should expose narrow module APIs. Avoid one mega-header that defines all app structs, all window states, and all cross-module functions.
- Prefer private structs in `.c` files. Put only stable public model/API types in headers.

**Good target shape:**
```text
examples/mydesigner/
  main.c
  app_controller.c/.h        // app state, active doc, commands, notifications
  project_io.c/.h            // load/save/archive only
  document_model.c/.h        // pure persistent document model + CRUD
  component_registry.c/.h    // component descriptors + property schemas
  layout_engine.c/.h         // measure/arrange/reflow/drop targets
  view_canvas.c/.h           // canvas proc, hit testing, live-view map
  view_menubar.c/.h          // menu resources + command forwarding
  view_property_browser.c/.h // metadata-driven inspector
  view_library.c/.h          // toolbox/library UI
```

When unsure, use this test: if a file both parses XML and handles `evLeftButtonDown`, or both draws selection handles and writes project files, split it.

### Working with Text Rendering
- For small fixed-width text: use `draw_text_small()` with `strwidth()` for measurements
- Font rendering is OpenGL-based using texture atlases for efficiency
- Always initialize the text system with `init_text_rendering()` before use

## Dependencies

- SDL2 (libsdl2-dev on Ubuntu/Debian)
- OpenGL 3.2 or later (mesa-libGL-devel on Fedora/RHEL)
- Standard C library

## Current Status

✅ Completed:
- Header files defining API structure
- Common controls (button, checkbox, edit, label, list, combobox, console)
- Text rendering module (bitmap and game fonts)
- Console module for message display
- Example hello world program

⏳ In Progress:
- Extracting core window management from mapview
- Extracting drawing primitives from mapview
- Additional example programs

## Testing and Building

- No automated testing infrastructure exists yet
- Build system integration is planned (Makefile to be added)
- The framework is designed to be integrated into existing build systems

## Debugging and Reproduction Logging

- When fixing interactive bugs, log **user actions and state transitions** so the exact repro path is visible from logs.
- Prefer platform logging (`axSetLogFile`, `axLog`, `axLogFlush`) over ad-hoc `printf`/`stderr` so logs are captured in a persistent file.
- Log at action boundaries (command dispatch, mouse down/up, tool change, dialog open/close), not every frame, to keep logs readable.
- Include enough context to replay the issue: active document/window id, command id/name, selected index/item, and key mode flags.
- Keep logging behind a debug toggle (`*_DEBUG`) so it can be enabled during investigation and disabled for normal runs.

## When Adding Features

- Maintain compatibility with the existing message-based architecture
- Follow Windows API patterns where applicable (familiar to many developers)
- Keep the layered architecture clean (user/kernel/commctl separation)
- **Required for menu actions**: when adding any File/Edit/View/Help command, also add an accelerator (`accel_t` + `load_accelerators` + `translate_accelerator`) that dispatches the same command id through `evCommand`.
- **Required for documentation**: when adding/changing menu actions or shortcuts in examples/apps, update the relevant README/docs section in the same change.
- **Extend the framework rather than making workarounds**: if something logically belongs in the framework (e.g., timers, clipboard, accelerators, drag-and-drop), add it to the appropriate layer (`user/`, `kernel/`, or `commctl/`) and expose a clean API
- **Search existing framework before implementing anything new**: grep the codebase for the concept first (e.g., "toolbar", "bitmap", "strip"). Orion already ships toolbars, toolbar buttons, bitmap strips, accelerators, dialogs, status bars, and form-based window creation. Reimplementing these as custom structs or flags is always wrong.
- **Use `form_def_t` + `show_dialog_from_form()` for all dialogs/panels**: any window with two or more standard child controls must be expressed as a static `form_ctrl_def_t[]` + `form_def_t` and instantiated with `create_window_from_form()` or `show_dialog_from_form()`. Never build children imperatively inside `evCreate` — children defined in a form already exist when that message fires.
- Add documentation to README.md for new public APIs
- Consider adding examples for non-trivial new functionality

### FormEditor Component Registration Policy

- FormEditor control exposure must be registry-driven, not switch-driven. Do not hardcode toolbox/component lists in `tool_to_ctrl_type`, `ctrl_type_name`, property-browser type tables, or save/load type switches.
- **No backward compatibility shims for the registry migration**: when moving a path to the component registry, remove legacy fallback code in the same change rather than keeping dual paths.
- Distinguish runtime windows from design-time toolbox components using explicit component metadata flags/role fields.
- A component is shown in the toolbox only when its metadata explicitly marks it as design-placeable (for example, `COMPONENT_PLACEABLE` / `show_in_toolbox=true`).
- Non-placeable windows (dialogs, palettes, color pickers, inspectors, transient popups) may still be registered for runtime/factory use, but must be hidden from the toolbox.

### Function-First, High-Level Code (Required)

Apply this to **all Orion code**, not only palettes/widgets.

1. **Extract intent into named functions early.**
  If logic does more than one thing (layout + draw, parse + validate, dispatch + mutate),
  split it into focused helpers.
2. **Prefer high-level composition over inline micromanagement.**
  Build behavior from existing abstractions (framework APIs, helpers, typed structs,
  message handlers) instead of long local blocks with ad-hoc step-by-step manipulation.
3. **Use domain helpers instead of manual low-level math.**
  For geometry use `rect_*` operations; for UI use existing control/message patterns;
  for resources use existing framework loading and ownership conventions.
4. **Keep state at the right granularity.**
  Store coarse, meaningful state (regions, modes, selections) and derive ephemeral
  details locally where they are used.
5. **Single-source behavior for each concern.**
  Paint, hit-test, and state transitions should share the same derived logic to avoid drift.
6. **Prefer configuration tables over if/else chains for field/action dispatch.**
  When mapping identifiers (field names, commands, actions) to behavior, use static arrays
  of structs plus shared dispatch helpers (DDX-style) so new mappings are data changes, not logic forks.
7. **Use Action-Message DDX for data object handlers.**
  Keep `msg` as the action verb (for example, get/set field data) and pass field identity + payload length in packed `wparam` (`MAKEDWORD(column_id, len)`). This keeps object handlers aligned with WinAPI-style message contracts and allows one proc to serve multiple models/tables.

Why this is now the standard:
- Improves readability by making control flow and intent obvious from function names.
- Improves structure by separating responsibilities and reducing cross-coupled code.
- Makes changes safer: edit one helper/constant instead of many duplicated pixel offsets.
- Reduces regressions caused by hidden side effects and repeated low-level logic.

UI-specific application of the same rule:
- For custom drawing/layout, define an outer `irect16_t` region per widget and derive internals
  with `rect_*` helpers inside dedicated draw/hit helpers rather than pixel-pushing inline.

### Anti-Patterns (learned from real mistakes)

1. **Don't store per-icon `{col, row}` in each button.** A sprite sheet is a strip of fixed-size tiles. Load it once, derive `cols = texture_w / icon_w`, and give each button only an integer index. WinAPI's `TBBUTTON.iBitmap` is the canonical model — follow it exactly.

2. **Don't add a new flag to an existing control class when a new control class is the right answer.** `win_button` is a text-label button. Icon-strip buttons are `win_toolbar_button`. These are distinct classes, just as `TBSTYLE_BUTTON` and `BS_PUSHBUTTON` are distinct WinAPI styles.

3. **Don't build a custom floating palette window from scratch when `WINDOW_TOOLBAR` already exists.** Any window gains a built-in toolbar strip via `WINDOW_TOOLBAR` + `tbAddButtons`. Only create a separate palette/floating window when the design genuinely requires it (e.g., Photoshop's detachable toolbox), and even then, use `win_toolbar_button` children rather than custom paint code.

4. **Don't hard-code texture dimensions.** When loading a PNG, always propagate the actual loaded `w`/`h` into the strip descriptor (`strip.sheet_w = loaded_w; strip.cols = loaded_w / icon_w`). Never assume a fixed size — the file found on a fallback path may differ.

5. **Don't put `WINDOW_HSCROLL` / `WINDOW_VSCROLL` on a `win_scrollbar` window.** Those flags mean "this window has built-in framework scrollbars" and are intercepted by `send_message()`. The orientation of a standalone `win_scrollbar` is set via `lparam` at create time: `(void *)0` = horizontal, `(void *)1` = vertical. See the [Scrollbars](#scrollbars----built-in-vs-standalone) section above.

6. **Don't create child controls imperatively in `evCreate` when a `form_def_t` can do it declaratively.** Dialogs and panels with standard controls (buttons, edit boxes, labels, checkboxes, lists, comboboxes) must use `form_ctrl_def_t[]` + `form_def_t`, passed to `create_window_from_form()` or `show_dialog_from_form()`. Writing `create_window(…, win_button, …)` inside a window proc is wrong. The form's children already exist when `evCreate` fires — use `get_window_item()` / `set_window_item_text()` to read or initialise them.

7. **Don't use `win->frame` dimensions for client-space paint/layout/hit-testing.** `win->frame` includes non-client chrome (title/borders), so using it for client math pushes rows and button strips into clipped areas. For widget geometry, always derive from `irect16_t cr = get_client_rect(win)` and use `cr.w` / `cr.h` (plus `rect_*` helpers) in both paint and hit paths. Use `win->frame` only for operations that explicitly need outer-frame metrics.

8. **Don't add `WINDOW_FLEXSPACE` to grid columns expecting them to expand.** Grid columns without explicit `layout_fixed_w` automatically share available width equally (WPF `Width="*"` semantics). `WINDOW_FLEXSPACE` is a **stack concept only** — it tells a stack child to expand along the stack axis. In grids, space distribution is automatic and uniform across auto-width columns.

9. **Don't add duplicate layout state when existing flags/classes already encode it.** For example, do not introduce separate `layout_orientation` fields if `WINDOW_STACK_HORIZONTAL` (plus class/proc identity) already defines orientation semantics.

10. **Don't centralize all layout kinds into one giant dispatcher function.** A large `layout.c` function with repeated `if (stack/grid/...)` branches is hard to extend and easy to break. Prefer small, type-specific measure/arrange helpers wired through each container's window procedure, with shared utilities only for common math.

### Window Class Enhancements (Planned)

**Goal**: Move control defaults (dimensions, flags, behavior) from scattered macros/code into the window class registry, enabling cleaner .orion files and eliminating redundant attributes.

**Architecture**:
Extend `window_class_t` (already exists in `user/window.c`) with default properties:
```c
typedef struct window_class_s {
  const char *name;           // "button", "textedit", "reportview", etc.
  winproc_t proc;             // window procedure
  
  // NEW: Default properties
  int16_t default_width;      // -1 = stretch, 0 = measure content, >0 = fixed
  int16_t default_height;     // natural height for this control type
  flags_t default_flags;      // WINDOW_FLEXSPACE, WINDOW_VSCROLL, etc.
  uint8_t default_h_align;    // LAYOUT_ALIGN_STRETCH, etc.
  uint8_t default_v_align;
} window_class_t;
```

**Benefits**:
1. **Single source of truth**: Button height (19px) defined once in button class, not scattered across macros, paint code, and .orion files
2. **Cleaner .orion files**: Flags like `WINDOW_VSCROLL` come from `reportview` class automatically
3. **HTML/CSS-like declarative UI**: Forms specify width, height flows from content; controls use class defaults unless overridden
4. **Type safety**: Can't accidentally create a button with wrong dimensions

**Form Height Rules**:
- **Fixed-content forms**: specify `width="X"` only, omit `height=` — it auto-calculates from children
  - Forms with only fixed-size controls (labels, buttons, textedit, checkboxes, separators, sliders)
  - Forms where `<space>` elements only expand **horizontally** (in horizontal stacks to push buttons)
  - Example: `<form name="new_image" width="180">` (no height needed)
  
- **Flex-content forms**: specify both `width="X" height="Y"` when they contain **vertically expanding** flex controls
  - `<multiedit flags="WINDOW_FLEXSPACE">` — text editor that expands/shrinks vertically
  - `<reportview flags="WINDOW_FLEXSPACE">` — scrolling list that expands/shrinks vertically  
  - `<grid flags="WINDOW_FLEXSPACE">` — grid container that expands/shrinks vertically
  - Example: `<form name="new_post" width="272" height="250">` (multiedit needs space to expand)

- **Key distinction**: horizontal `<space>` elements (used to push buttons left/right) don't count as "flex content" for height calculation — they expand horizontally, not vertically, so form height is still auto-calculable

**Example Transformation**:
```xml
<!-- BEFORE: Explicit everything -->
<form name="new_image" frame="0 0 180 84">
  <grid spacing="4">
    <column width="48" flags="0">
      <label text="Width:" flags="0" />
      <label text="Height:" flags="0" />
    </column>
    <column flags="WINDOW_FLEXSPACE">
      <textedit name="width" flags="0" />
      <textedit name="height" flags="0" />
    </column>
  </grid>
  <separator flags="0" />
  <stack orientation="horizontal">
    <space flags="WINDOW_FLEXSPACE" />
    <button text="OK" flags="BUTTON_DEFAULT" />
    <button text="Cancel" flags="0" />
  </stack>
</form>

<!-- AFTER: Class defaults + auto-height -->
<form name="new_image" width="180">  <!-- height auto-calculated -->
  <grid spacing="4">
    <column width="48">
      <label text="Width:" />   <!-- height=13 from label class -->
      <label text="Height:" />
    </column>
    <column>  <!-- auto-expands, no flag needed -->
      <textedit name="width" />  <!-- height=13 from textedit class -->
      <textedit name="height" />
    </column>
  </grid>
  <separator />  <!-- height=1 from separator class -->
  <stack orientation="horizontal">
    <space />  <!-- WINDOW_FLEXSPACE from space class -->
    <button text="OK" flags="BUTTON_DEFAULT" />  <!-- height=19 from button class -->
    <button text="Cancel" />
  </stack>
</form>
```

**Implementation Phases**:
1. ✅ **Phase 0** (complete): Auto-height measurement for fixed-content forms
2. **Phase 1**: Extend `window_class_t` with default property fields
3. **Phase 2**: Register built-in control classes with their defaults (button=19px, textedit=13px, separator=1px, etc.)
4. **Phase 3**: Update `evMeasure` handlers to use class defaults when instance doesn't override
5. **Phase 4**: Update `orionc` compiler to omit redundant attributes matching class defaults
6. **Phase 5**: Gradually migrate .orion files using tool-assisted refactoring

**Control Class Defaults** (to be registered):
```c
// commctl/button.c
window_class_t button_class = {
  .name = "button",
  .proc = win_button,
  .default_height = 19,
  .default_flags = 0,
};

// commctl/edit.c  
window_class_t textedit_class = {
  .name = "textedit",
  .proc = win_edit,
  .default_height = 13,
  .default_flags = 0,
};

// commctl/columnview.c
window_class_t reportview_class = {
  .name = "reportview",
  .proc = win_report_view,
  .default_height = 100,  // ~6 rows
  .default_flags = WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE,
};

// commctl/separator.c
window_class_t separator_class = {
  .name = "separator",
  .proc = win_separator,
  .default_height = 1,
  .default_flags = 0,
};

// Special: space element
window_class_t space_class = {
  .name = "space",
  .proc = win_spacer,  // minimal proc, just handles evMeasure
  .default_width = 0,
  .default_height = 0,
  .default_flags = WINDOW_FLEXSPACE,  // Always flexible
};
```

**Override Precedence**:
```
Class defaults < Form/parent hints < Instance attributes (.orion)
```

**Migration Strategy**:
- Keep backward compatibility: explicit `frame=` and `flags=` continue to work
- New forms can use cleaner syntax immediately
- Tool-assisted migration: script to remove redundant attributes from existing .orion files
- Deprecate (but don't remove) verbose syntax over time

**Status**: Design phase complete. Ready for implementation when prioritized.

### Recent Bug Fixes and Lessons

**Grid layout star sizing (May 2026)**
- **Issue**: Grid columns only expanded when marked with `WINDOW_FLEXSPACE`, leaving other columns at 1px minimum width
- **Root cause**: Implementation checked `WINDOW_FLEXSPACE` flag instead of absence of explicit width
- **Fix**: Changed `commctl/layout.c` to distribute space among all columns without `layout_fixed_w > 0`, ignoring flex flags for grids
- **Semantics**: Now matches WPF Grid where `<ColumnDefinition Width="*" />` columns automatically divide available space evenly
- **Lesson**: Grid and stack are different layout models — don't conflate their space-allocation flags

**Reportview scrolling in auto-layout containers (May 2026)**
- **Issue**: Mouse wheel didn't scroll; clicking after scrolling selected wrong items
- **Root cause**: Used `win->frame.h` directly instead of `get_client_rect()`, which gave wrong dimensions when reportview was inside grid/stack containers
- **Fix**: Updated `commctl/columnview.c` to consistently use `get_client_rect(win)` for all scroll calculations, paint dimensions, and content sizing
- **Affected functions**: `rv_content_width()`, `rv_sync_scroll()`, `rv_paint_report_view()`, `evVScroll` handler
- **Lesson**: Always use `get_client_rect()` for client-space dimensions; `win->frame` includes title bars, scrollbars, and other chrome that varies by context

**Toolbar internals moved to toolbar host userdata (May 2026)**
- **Issue**: `window_s` carried toolbar internals (`toolbar_children`, strip state, button size), coupling unrelated window logic to toolbar implementation details.
- **Fix**: Added a dedicated toolbar host window (`win_toolbar`) and moved toolbar runtime state to `toolbar_state_t` stored in `win->toolbar->userdata`.
- **How to access**: Use `window_toolbar_state(win)` and read toolbar children from `tb->children`; do not add fields back onto `window_s`.
- **Lesson**: Keep control/container internals in control-owned userdata, not in generic window core structs.

**Sidebar width ownership moved to sidebar child (May 2026)**
- **Issue**: Parent windows stored separate sidebar width state (`sidebar_width`) that drifted from child layout/frame state.
- **Fix**: Sidebar width now lives on the sidebar child (`win->sidebar`): desired width in `win->sidebar->layout.layout_fixed_w`, effective fallback from `win->sidebar->frame.w`.
- **Lesson**: Prefer child-owned layout state over duplicating parallel width fields on the parent.

**Child ID counter removed from window_s (May 2026)**
- **Issue**: `window_s.child_id` was mutable parent state used only as an incrementing allocator.
- **Fix**: Replaced with computed allocation (`next_child_id(parent)`) scanning existing `children` and toolbar children.
- **Lesson**: Avoid storing redundant counters when IDs can be derived from the live tree safely.
