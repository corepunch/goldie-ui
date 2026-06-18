# View Layer

Views handle UI rendering and user interaction via window procedures.

## Correct Window Procedure

```c
lresult_t my_window_proc(window_t *win, uint32_t msg,
                         uint32_t wparam, void *lparam) {
    switch (msg) {
        case evCreate: {
            // Initialize window state
            my_state_t *state = (my_state_t *)lparam;
            win->userdata = state;
            
            // Get child windows by ID
            state->list_win = get_window_item(win, ID_MY_LIST);
            
            // Set initial values
            set_window_item_text(win, ID_MY_EDIT, "Initial text");
            
            return true;
        }

        case evPaint: {
            // Custom drawing (return false to let framework draw controls)
            my_state_t *state = (my_state_t *)win->userdata;
            draw_text_small("Custom text", 10, 10, get_sys_color(brTextNormal));
            return false;
        }

        case evCommand: {
            uint16_t notification = HIWORD(wparam);
            window_t *source = (window_t *)lparam;

            // Handle button clicks
            if (notification == btnClicked && source->id == ID_MY_BUTTON) {
                // Handle button click
                return true;
            }

            // Handle list selection changes
            if (notification == RVN_SELCHANGE && source->id == ID_MY_LIST) {
                int index = (int)(int16_t)LOWORD(wparam);
                // Handle selection change
                return true;
            }

            return false;
        }

        case evResize: {
            // Let framework handle layout
            window_layout_sync(win);
            return default_winproc(win, msg, wparam, lparam);
        }

        case evDestroy: {
            // Cleanup
            my_state_t *state = (my_state_t *)win->userdata;
            free(state);
            win->userdata = NULL;
            return true;
        }

        default:
            return default_winproc(win, msg, wparam, lparam);
    }
}
```

## Incorrect Window Procedure

```c
// WRONG: Not calling default_winproc for unhandled messages
lresult_t bad_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
    switch (msg) {
        case evCreate:
            return true;
        // Missing default case - framework won't process other messages
    }
    return 0; // Should call default_winproc()
}

// WRONG: Not returning true for handled messages
case evCommand:
    if (HIWORD(wparam) == btnClicked) {
        handle_click();
        return false; // Should return true
    }
    return false;

// WRONG: Not cleaning up state in evDestroy
case evDestroy:
    return true; // Should free win->userdata
```

## Creating Windows

### From Form Definition
```c
// Create window from .orion form
window_t *win = create_window_from_form(&my_form, x, y, parent, my_proc, hinstance, NULL);
show_window(win, true);
```

### Manual Creation
```c
// Create window manually
window_t *win = create_window("Title", 0, MAKERECT(x, y, w, h), parent, my_proc, hinstance, NULL);
show_window(win, true);
```

## Dialogs

### Modal Dialog
```c
// Show modal dialog
uint32_t result = show_dialog_from_form(&my_form, "Dialog Title", parent, my_proc, &state);
if (result == 1) {
    // User clicked OK
}
```

### Database-Aware Dialog
```c
// Show dialog with automatic database binding
uint32_t result = show_db_dialog(&my_form, "Edit Item", parent, item_id);
if (result == 1) {
    // Item saved to database automatically
}
```

## Controls

### Getting Child Windows
```c
window_t *child = get_window_item(win, ID_MY_CONTROL);
```

### Setting Text
```c
set_window_item_text(win, ID_MY_LABEL, "Text: %d", value);
```

### Getting Text
```c
char buffer[256];
get_window_item_text(win, ID_MY_EDIT, buffer, sizeof(buffer));
```

## Common Messages

| Message | Description | wparam | lparam |
|---------|-------------|--------|--------|
| `evCreate` | Window created | 0 | State pointer |
| `evDestroy` | Window destroyed | 0 | NULL |
| `evPaint` | Window needs redraw | 0 | NULL |
| `evCommand` | Control notification | HIWORD=notification, LOWORD=id | Control pointer |
| `evResize` | Window resized | 0 | NULL |

## Common Mistakes

1. **Not calling `default_winproc()`** — breaks framework behavior
2. **Not returning `true` for handled messages** — messages not processed
3. **Not cleaning up state in `evDestroy`** — memory leaks
4. **Using wrong message IDs** — check generated constants
5. **Not checking `lparam` before casting** — null pointer crashes