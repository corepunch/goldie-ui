# Quick Reference Guide

Fast lookup for common tasks when building Orion apps.

## Create App Skeleton

```
examples/myapp/
├── myapp.orion          # App definition
├── myapp.h              # Header with IDs
├── main.c               # Entry point
├── controller_app.c     # Business logic
├── view_main.c          # Main window
├── view_menubar.c       # Menu handling
├── view_dialogs.c       # Dialogs
├── db_simple_xml.c      # Database
└── share/
    └── seed.xml         # Seed data
```

## .orion File Template

```xml
<?xml version="1.0" encoding="UTF-8"?>
<orion version="1" name="myapp" title="My App" root="examples/myapp">
    <menus var="kMenus" count="kNumMenus">
        <menu name="file" label="File">
            <item name="quit" label="Quit" />
        </menu>
    </menus>
    <toolbars>
        <toolbar name="main">
            <Button name="new" icon="sysicon_add" text="New" />
        </toolbar>
    </toolbars>
    <databases>
        <database name="db" class="SimpleXMLDatabase" source="examples/myapp/share/seed.xml">
            <table name="items">
                <field name="name" type="string" length="64"/>
            </table>
        </database>
    </databases>
    <forms>
        <form name="main_window" title="My App" width="400" height="300">
            <!-- Controls -->
        </form>
    </forms>
</orion>
```

## Header File Template

```c
#ifndef __MYAPP_H__
#define __MYAPP_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../ui.h"
#include "../../commctl/menubar.h"
#include "../../user/accel.h"
#include "../../user/icons.h"

// Generated IDs (use orionc to generate)
#include "build/generated/examples/myapp/myapp.h"

// Database implementation
lresult_t db_simple_xml(database_t *db, uint32_t msg, uint32_t wparam, void *lparam);

// App state
typedef struct {
    database_t  *db;
    window_t    *main_win;
    window_t    *menubar_win;
    hinstance_t  hinstance;
    accel_table_t *accel;
} app_state_t;

extern app_state_t *g_app;

// Controller functions
app_state_t *app_init(void);
void app_shutdown(app_state_t *app);

// View functions
void create_menubar(void);
void create_main_window(void);
void handle_menu_command(uint16_t id);

#endif
```

## Main Entry Point

```c
#include "myapp.h"
#include "../../gem_magic.h"

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
    DB_CLASS(db_simple_xml);  // Register database class
    g_app = app_init();
    g_app->hinstance = hinstance;
    register_commctl_classes();
    
    g_app->db = create_database("myapp", "db_simple_xml", "seed.xml");
    register_database("db", g_app->db);
    
    create_menubar();
    create_main_window();
    return true;
}

GEM_DEFINE("My App", "1.0", gem_init, gem_shutdown, NULL)
GEM_STANDALONE_MAIN("My App", UI_INIT_DESKTOP, SCREEN_W, SCREEN_H,
                    g_app->menubar_win, g_app->accel)
```

## Window Procedure

```c
lresult_t my_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
    switch (msg) {
        case evCreate: {
            // Initialize, create children
            return true;
        }
        case evCommand: {
            if (HIWORD(wparam) == btnClicked) {
                window_t *src = (window_t *)lparam;
                if (src->id == ID_MY_BUTTON) {
                    // Handle click
                    return true;
                }
            }
            return false;
        }
        case evClose:
            ui_request_quit();
            return true;
        default:
            return default_winproc(win, msg, wparam, lparam);
    }
}
```

## Database Operations

```c
// Insert
item_t item = { .name = "New" };
item_t *inserted = (item_t *)send_db_message(db, dbInsert, ID_DB_ITEMS, &item);

// Find
item_t *found = (item_t *)send_db_message(db, dbFind,
    MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)id);

// Update
found->value = 42;
send_db_message(db, dbUpdate, ID_DB_ITEMS, found);

// Delete
send_db_message(db, dbDelete, ID_DB_ITEMS, (void *)(intptr_t)id);

// Fetch all
result_node_t *list = (result_node_t *)send_db_message(db, dbFetch,
    MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)0);
int count = count_result_list(list);
free_result_list(list);

// Fetch with filter
result_node_t *filtered = (result_node_t *)send_db_message(db, dbFetch,
    MAKEDWORD(ID_DB_ITEMS, ID_DB_ITEMS_CATEGORY), (void *)(intptr_t)cat_id);
free_result_list(filtered);
```

## Create Controls

```c
// Label
create_window("Name:", WINDOW_NOTITLE, MAKERECT(x, y, 60, 14), parent, win_label, 0, NULL);

// Text input
window_t *edit = create_window("", WINDOW_NOTITLE, MAKERECT(x, y, 200, 20),
                               parent, win_textedit, 0, NULL);
edit->id = ID_EDIT;

// Button
create_window("OK", WINDOW_NOTITLE, MAKERECT(x, y, 60, 20),
              parent, win_button, BUTTON_DEFAULT, NULL)->id = ID_OK;

// Checkbox
create_window("Enable", WINDOW_NOTITLE, MAKERECT(x, y, 120, 20),
              parent, win_checkbox, 0, NULL)->id = ID_CHECK;
```

## Get/Set Text

```c
// Set text
set_window_item_text(win, ID_LABEL, "Value: %d", value);
set_window_item_text(win, ID_EDIT, "%s", text);

// Get text (use send_message)
char buffer[256];
send_message(get_window_item(win, ID_EDIT), edGetText, sizeof(buffer), (lParam_t)buffer);
```

## Show Dialogs

```c
// Simple dialog
uint32_t result = show_dialog_from_form(&form, "Title", parent, my_proc, &state);

// Database dialog
show_db_dialog(&form, "Edit", parent, record_id);

// Database dialog with extra binding
show_db_dialog_ex(&form, "Edit", parent, record_id, "post_id", post_id);
```

## Menu Commands

```c
void handle_menu_command(uint16_t id) {
    switch (id) {
        case ID_FILE_QUIT:
            ui_request_quit();
            break;
        case ID_FILE_NEW:
            show_new_dialog(g_app->main_win);
            break;
    }
}
```

## Build and Run

```bash
# Generate header from .orion
./build/bin/orionc --input myapp.orion --output build/generated/examples/myapp.h --prefix myapp

# Build app
make examples

# Run
./build/bin/myapp
```

## Common Includes

```c
#include "../../ui.h"                    // Core UI
#include "../../gem_magic.h"             // GEM_DEFINE macros
#include "../../commctl/menubar.h"       // Menu bar
#include "../../commctl/columnview.h"    // Column view
#include "../../user/accel.h"            // Accelerators
#include "../../user/icons.h"            // System icons
#include "../../platform/platform.h"     // Platform utils
```

## System Icons

```c
sysicon_add       // Add/plus icon
sysicon_save      // Save icon
sysicon_delete    // Delete/trash icon
sysicon_folder    // Folder icon
sysicon_file      // File icon
sysicon_refresh   // Refresh icon
sysicon_search    // Search icon
sysicon_settings  // Settings icon
sysicon_help      // Help icon
sysicon_info      // Info icon
sysicon_warning   // Warning icon
sysicon_error     // Error icon
sysicon_accept    // Checkmark icon
sysicon_cancel    // X icon
```

## Debug Logging

```c
// Define debug macro
#if MYAPP_DEBUG
#define MY_DEBUG(...) do { axLog("[myapp] " __VA_ARGS__); } while (0)
#else
#define MY_DEBUG(...) ((void)0)
#endif

// Use in code
MY_DEBUG("created item id=%d", item_id);
```