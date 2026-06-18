# Window API Reference

Complete API for window creation, management, and messaging.

## Window Creation

```c
// Create window with proc (most common)
window_t *create_window(const char *title, flags_t flags, const irect16_t *frame,
                        window_t *parent, winproc_t proc, hinstance_t hinstance, void *ldata);

// Create from windef_t (for loading from arrays)
window_t *create_window2(windef_t const *def, irect16_t const *r, window_t *parent);

// Create from form definition (declarative UI)
window_t *create_window_from_form(form_def_t const *def, int x, int y,
                                  window_t *parent, winproc_t proc,
                                  hinstance_t hinstance, void *ldata);
```

**Parameters:**
- `title` — Window title (NULL for no title)
- `flags` — Window flags (see Window Flags)
- `frame` — Position and size as `irect16_t` or `MAKERECT(x, y, w, h)`
- `parent` — Parent window (NULL for top-level)
- `proc` — Window procedure callback
- `hinstance` — Application instance (0 for system)
- `ldata` — Extra data passed to evCreate as lparam

**Returns:** Window pointer or NULL on failure

## Window Lifecycle

```c
void show_window(window_t *win, bool visible);     // Show/hide window
void destroy_window(window_t *win);                 // Destroy window
void invalidate_window(window_t *win);              // Request repaint
void clear_window_children(window_t *win);          // Remove all children
void clear_toolbar_children(window_t *win);         // Remove toolbar children
```

## Window Position and Size

```c
void move_window(window_t *win, int x, int y);                    // Move window
void resize_window(window_t *win, int new_w, int new_h);          // Resize window
int window_screen_x(window_t const *win);                         // Get screen X
int window_screen_y(window_t const *win);                         // Get screen Y
int titlebar_height(window_t const *win);                         // Get titlebar height
int statusbar_height(window_t const *win);                        // Get statusbar height
void adjust_window_rect(irect16_t *r, flags_t flags);             // Adjust rect for flags
```

## Window Layout

```c
void layout_measure_window(window_t *win, layout_measure_t *m);   // Measure window
void layout_arrange_window(window_t *win, const irect16_t *rect); // Arrange layout
void window_layout_sync(window_t *win);                           // Sync layout
void set_default_window_position(int x, int y);                   // Set default position
```

## Window Items

```c
window_t *get_window_item(window_t const *win, uint32_t id);      // Get child by ID
void set_window_item_text(window_t *win, uint32_t id, const char *fmt, ...); // Set child text
void load_window_children(window_t *win, windef_t const *def);    // Load children from def
```

**Getting text from controls (use send_message):**
```c
char buffer[256];
send_message(get_window_item(win, ID_EDIT), edGetText, sizeof(buffer), (lParam_t)buffer);
```

## Window State

```c
bool window_has_state(const window_t *win, uint32_t state_flag);  // Check state flag
void window_set_state(window_t *win, uint32_t state_flag, bool enabled); // Set state flag
bool is_window(window_t *win);                                     // Check if valid window
bool window_has_focus(const window_t *win);                        // Check if focused
window_t *get_root_window(window_t *window);                       // Get root window
window_t *find_window(int x, int y);                               // Find window at point
window_t *find_default_button(window_t *win);                      // Find default button
bool window_in_drag_area(window_t const *win, int sy);             // Check drag area
```

## Window Focus and Input

```c
void set_focus(window_t *win);                                     // Set keyboard focus
void set_capture(window_t *win);                                   // Capture mouse input
void track_mouse(window_t *win);                                   // Track mouse events
void move_to_top(window_t *win);                                   // Move to top of Z-order
void enable_window(window_t *win, bool enable);                    // Enable/disable window
```

## Messaging

```c
lresult_t send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
void post_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
```

**send_message** — Synchronous, waits for handler to complete
**post_message** — Asynchronous, returns immediately

## Scrolling

```c
void set_scroll_info(window_t *win, int bar, scroll_info_t const *info, bool redraw);
void get_scroll_info(window_t *win, int bar, scroll_info_t *info);
int  get_scroll_pos(window_t *win, int bar);
void enable_scroll_bar(window_t *win, int bar, bool enable);
void show_scroll_bar(window_t *win, int bar, bool show);
void reset_scroll_bar_auto(window_t *win, int bar);
```

**bar parameter:** `SB_HORZ` (horizontal) or `SB_VERT` (vertical)

## Window Data

```c
void *allocate_window_data(window_t *win, size_t size);            // Allocate userdata
toolbar_state_t *window_toolbar_state(window_t *win);              // Get toolbar state
```

## Window Hooks

```c
void register_window_hook(uint32_t msg, winhook_func_t func, void *userdata);
void deregister_window_hook(uint32_t msg, winhook_func_t func, void *userdata);
void remove_from_global_hooks(window_t *win);
void cleanup_all_hooks(void);
```

## Drag and Drop

```c
void ui_drag_item_set(const char *text, const ui_drag_item_payload_t *payload);
void ui_drag_item_set_text_origin(const char *text,
                                  const ui_drag_item_payload_t *payload,
                                  int screen_x, int screen_y);
void ui_drag_item_move(int sx, int sy);
void ui_drag_item_clear(void);
```

## Tooltips

```c
void tooltip_update(window_t *src_win, const char *text, int sx, int sy);
void tooltip_cancel(void);
```

## Window Flags

```c
#define WINDOW_NOTITLE        0x0001    // No title bar
#define WINDOW_NORESIZE       0x0002    // Not resizable
#define WINDOW_NOMOVE         0x0004    // Not movable
#define WINDOW_NOTRAYBUTTON   0x0008    // No tray button
#define WINDOW_TRANSPARENT    0x0010    // Transparent background
#define WINDOW_VSCROLL        0x0020    // Vertical scrollbar
#define WINDOW_HSCROLL        0x0040    // Horizontal scrollbar
#define WINDOW_DIALOG         0x0080    // Dialog window
#define WINDOW_TOOLBAR        0x0100    // Has toolbar
#define WINDOW_STATUSBAR      0x0200    // Has statusbar
#define WINDOW_AUTO_LAYOUT    0x0400    // Auto-layout enabled
#define WINDOW_STACK_HORIZONTAL 0x0800  // Horizontal stack
#define WINDOW_STACK_VERTICAL   0x1000  // Vertical stack
#define WINDOW_FLEXSPACE      0x2000    // Flexible space
#define WINDOW_NOTITLEBAR     0x4000    // No titlebar (different from NOTITLE)
#define WINDOW_NOFILL         0x8000    // Don't fill parent
```

## MAKERECT Macro

```c
// Create rectangle from coordinates
irect16_t rect = MAKERECT(x, y, width, height);

// Example:
window_t *win = create_window("Title", 0, MAKERECT(100, 100, 400, 300),
                              NULL, my_proc, 0, NULL);
```

## Common Patterns

### Create Window with Child Controls
```c
case evCreate: {
    // Create child controls
    create_window("Label:", WINDOW_NOTITLE, MAKERECT(10, 10, 60, 14),
                  win, win_label, 0, NULL);
    
    window_t *edit = create_window("", WINDOW_NOTITLE, MAKERECT(80, 10, 200, 20),
                                   win, win_textedit, 0, NULL);
    edit->id = ID_MY_EDIT;
    
    return true;
}
```

### Handle Button Click
```c
case evCommand:
    if (HIWORD(wparam) == btnClicked) {
        window_t *source = (window_t *)lparam;
        if (source->id == ID_MY_BUTTON) {
            // Handle click
            return true;
        }
    }
    return false;
```

### Set Text in evCreate
```c
case evCreate: {
    // Set initial text
    set_window_item_text(win, ID_LABEL, "Value: %d", initial_value);
    return true;
}
```

## Window Procedure Return Values

- Return `true` (1) — Message handled, don't process further
- Return `false` (0) — Message not handled, call default_winproc
- Return specific value — Depends on message (see message docs)