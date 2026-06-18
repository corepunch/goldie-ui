# Dialog and Form API Reference

Complete API for creating dialogs, forms, and data-bound UI.

## Dialog Functions

### Simple Dialog (no form)
```c
uint32_t show_dialog(char const *title, int width, int height,
                     window_t *parent, winproc_t proc, void *param);
```

### Dialog with Form Definition
```c
// Basic form dialog
uint32_t show_dialog_from_form(form_def_t const *def, char const *title,
                               window_t *parent, winproc_t proc, void *param);

// Form dialog with flags
uint32_t show_dialog_from_form_ex(form_def_t const *def, char const *title,
                                  window_t *parent, uint32_t flags,
                                  winproc_t proc, void *param);
```

### DDX Dialog (Data Exchange)
```c
// Dialog with automatic data binding
uint32_t show_ddx_dialog(form_def_t const *def, const char *title,
                         window_t *parent, void *state,
                         const ctrl_binding_t *bindings, int binding_count,
                         winproc_t proc, void *param);
```

### Database Dialog (CRUD)
```c
// Database-aware dialog (auto-fetch, auto-save)
uint32_t show_db_dialog(form_def_t const *def, const char *title,
                        window_t *parent, int record_id);

// Database dialog with extra bindings
uint32_t show_db_dialog_ex(form_def_t const *def, const char *title,
                           window_t *parent, int record_id,
                           const char *extra_key, int extra_value);
```

### Dialog Control
```c
void end_dialog(window_t *win, uint32_t code);  // Close dialog with result code
```

## Form Definition Structure

```c
typedef struct {
    const char        *name;           // Form name
    int               width;           // Form width
    int               height;          // Form height
    uint32_t          flags;           // Window flags
    int               layout_spacing;  // Spacing between children
    irect16_t         padding;         // Internal padding {left, top, right, bottom}
    const form_ctrl_def_t *children;   // Child controls array
    int               child_count;     // Number of children
    const ctrl_binding_t *bindings;    // DDX bindings (optional)
    int               binding_count;   // Number of bindings
    uint32_t          ok_id;           // OK button ID (for DDX)
    uint32_t          cancel_id;       // Cancel button ID (for DDX)
    // Database metadata (filled by orionc or manually)
    const char        *db_name;        // Database name
    const char        *db_table;       // Table name
    uint32_t          db_table_id;     // Table ID
    const void        *db_fields;      // Field metadata array
    int               db_field_count;  // Number of fields
} form_def_t;
```

## Form Control Definition

```c
typedef struct form_ctrl_def_s {
    const char        *class_name;     // Control class (see Control Classes)
    uint32_t          id;              // Control ID (0 = auto-generate)
    irect16_t         size;            // Size hint {width, height}
    uint32_t          flags;           // Control flags
    const char        *text;           // Initial text
    const char        *name;           // Control name (for lookup)
    uint32_t          h_align;         // Horizontal alignment
    uint32_t          v_align;         // Vertical alignment
    int               layout_spacing;  // Spacing for containers
    // Nested children for containers
    const struct form_ctrl_def_s *children;
    int               child_count;
} form_ctrl_def_t;
```

## Control Classes

| Class | Description | Common Flags |
|-------|-------------|--------------|
| `"label"` | Static text | `WINDOW_FLEXSPACE` |
| `"textedit"` | Single-line text input | `WINDOW_FLEXSPACE` |
| `"multiedit"` | Multi-line text input | `WINDOW_FLEXSPACE` |
| `"button"` | Clickable button | `BUTTON_DEFAULT` |
| `"checkbox"` | Toggle checkbox | — |
| `"combobox"` | Dropdown selection | `WINDOW_FLEXSPACE` |
| `"listbox"` | List box | `WINDOW_VSCROLL` |
| `"stack"` | Stack container | `WINDOW_STACK_HORIZONTAL/VERTICAL` |
| `"grid"` | Grid container | — |
| `"space"` | Flexible space | `WINDOW_FLEXSPACE` |
| `"separator"` | Horizontal/vertical line | — |

## DDX Binding Structure

```c
typedef struct {
    uint32_t    ctrl_id;    // Control ID
    uint32_t    command;    // Binding command (0 for simple)
    uint32_t    getter;     // Getter message (edGetText, cbGetCurrentSelection, etc.)
    size_t      offset;     // offsetof(struct, field)
    size_t      wparam;     // Getter wparam (buffer size for edGetText)
    void        *push;      // Push callback (optional)
    void        *pull;      // Pull callback (optional)
} ctrl_binding_t;
```

## Binding Types

| Getter | Control | State Field | wparam |
|--------|---------|-------------|--------|
| `edGetText` | TextBox | `char[]` | `sizeof(field)` |
| `edSetText` | TextBox | `const char*` | 0 |
| `cbGetCurrentSelection` | ComboBox | `int` (index) | 0 |
| `cbGetCurrentValue` | ComboBox | `int` (value) | 0 |
| `btnGetCheck` | CheckBox | `bool` | 0 |

## Database Binding (in .orion)

```xml
<!-- Field binding format: "db.table.field" -->
<TextBox name="title" field="db.posts.title" />

<!-- ComboBox with display/value -->
<ComboBox name="author"
          field="db.posts.author_id"
          source="db.authors"
          display="name"
          value="id" />

<!-- Button with action -->
<Button name="ok" text="Save" action="db.posts.insert" />
```

## Actions for Buttons

| Action | Description |
|--------|-------------|
| `db.table.insert` | Insert new record |
| `db.table.update` | Update existing record |
| `db.table.delete` | Delete record |
| `db.table.fetch` | Fetch/refresh data |

## Dialog Flags

```c
#define WINDOW_DIALOG         0x0080    // Dialog window
#define WINDOW_NOTRAYBUTTON   0x0008    // No tray button
#define WINDOW_VSCROLL        0x0020    // Vertical scrollbar
#define WINDOW_HSCROLL        0x0040    // Horizontal scrollbar
#define WINDOW_NOFILL         0x8000    // Don't fill parent
```

## Common Dialog Patterns

### Simple Input Dialog
```c
static const form_ctrl_def_t kInputChildren[] = {
    { .class_name = "label", .text = "Enter value:", .size = {0, 13} },
    { .class_name = "textedit", .id = 1, .size = {200, 20}, .flags = WINDOW_FLEXSPACE },
    { .class_name = "button", .id = ID_OK, .text = "OK", .size = {60, 20}, .flags = BUTTON_DEFAULT },
    { .class_name = "button", .id = ID_CANCEL, .text = "Cancel", .size = {60, 20} },
};

static const form_def_t kInputForm = {
    .name = "Input Dialog",
    .width = 280,
    .height = 80,
    .padding = {8, 8, 8, 8},
    .children = kInputChildren,
    .child_count = 4,
};

// Show dialog
uint32_t result = show_dialog_from_form(&kInputForm, "Input", parent, input_proc, &state);
if (result == ID_OK) {
    // User clicked OK
}
```

### Database Edit Dialog
```c
// In .orion file:
// <form name="edit_post" width="300" height="150">
//   <TextBox field="db.posts.title" />
//   <MultiEdit field="db.posts.body" />
//   <Button action="db.posts.insert" />
// </form>

// In C code:
show_db_dialog(&edit_post_form, "Edit Post", parent, post_id);
// Dialog auto-fetches record, populates controls, saves on OK
```

### Dialog with State
```c
typedef struct {
    int item_id;
    char name[64];
} dialog_state_t;

static lresult_t my_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
    dialog_state_t *state = (dialog_state_t *)win->userdata;
    
    switch (msg) {
        case evCreate: {
            state = (dialog_state_t *)lparam;
            win->userdata = state;
            
            // Set initial values
            set_window_item_text(win, ID_NAME, "%s", state->name);
            return true;
        }
        
        case evCommand:
            if (HIWORD(wparam) == btnClicked && LOWORD(wparam) == ID_OK) {
                // Get values from controls
                send_message(get_window_item(win, ID_NAME), edGetText,
                            sizeof(state->name), (lParam_t)state->name);
                end_dialog(win, ID_OK);
                return true;
            }
            return false;
    }
    return default_winproc(win, msg, wparam, lparam);
}

// Show dialog
dialog_state_t state = { .item_id = 1, .name = "Test" };
uint32_t result = show_dialog_from_form(&form, "Edit", parent, my_proc, &state);
```

## DDX Push/Pull Pattern

```c
// Push state to controls (evCreate)
dialog_push(win, state, bindings, binding_count);

// Pull controls to state (OK handler)
dialog_pull(win, state, bindings, binding_count);

// Pull on specific command
dialog_pull_command(win, state, bindings, binding_count, HIWORD(wparam));
```

## Database Dialog with Extra Bindings

```c
// Pass extra context to database dialog
show_db_dialog_ex(&form, "Edit", parent, record_id,
                  "post_id", current_post_id);
// The extra binding allows filtering by post_id
```