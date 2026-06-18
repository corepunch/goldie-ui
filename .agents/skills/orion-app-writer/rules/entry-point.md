# Entry Point

Apps use `gem_init()`/`gem_shutdown()` pattern with `GEM_DEFINE()` macro.

## Correct Entry Point

```c
#include "myapp.h"
#include "../../gem_magic.h"
#include "../../commctl/commctl.h"
#include "../../platform/platform.h"

// Initialize application
bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
    (void)argc; (void)argv;

    // Register database class
    DB_CLASS(db_simple_xml);

    // Create app state
    g_app = app_init();
    if (!g_app) return false;
    g_app->hinstance = hinstance;

    // Register common controls
    register_commctl_classes();

    // Create database
    g_app->db = create_database("myapp", "db_simple_xml", "path/to/seed.xml");
    if (!g_app->db) {
        app_shutdown(g_app);
        g_app = NULL;
        return false;
    }

    // Register database with framework
    ui_set_database(g_app->db);
    
    // Register database for declarative forms
    register_database("db", g_app->db);

    // Create windows
    create_menubar();
    create_main_window();

    return true;
}

// Shutdown application
void gem_shutdown(void) {
    if (!g_app) return;
    
    if (g_app->db) {
        destroy_database(g_app->db);
        g_app->db = NULL;
    }
    
    app_shutdown(g_app);
    g_app = NULL;
}

// Register application
GEM_DEFINE("My App", "1.0", gem_init, gem_shutdown, NULL)

// Standalone entry point
GEM_STANDALONE_MAIN("My App", UI_INIT_DESKTOP, SCREEN_W, SCREEN_H,
                    g_app->menubar_win, g_app->accel)
```

## Incorrect Entry Point

```c
// WRONG: Not registering database class
bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
    // Missing DB_CLASS(db_simple_xml);
    g_app->db = create_database("myapp", "db_simple_xml", "seed.xml");
    return true;
}

// WRONG: Not cleaning up in gem_shutdown
void gem_shutdown(void) {
    // Missing database cleanup
    free(g_app); // Should call app_shutdown()
}

// WRONG: Not using GEM_DEFINE macro
// App won't be registered with framework

// WRONG: Missing GEM_STANDALONE_MAIN
// No standalone entry point
```

## Database Registration

```c
// Register database class (before creating instances)
DB_CLASS(db_simple_xml);

// Create database instance
g_app->db = create_database("myapp", "db_simple_xml", "seed.xml");

// Register with framework (for automatic view population)
ui_set_database(g_app->db);

// Register for declarative forms (field="db.table.field")
register_database("db", g_app->db);
```

## Window Creation

```c
// Create menubar
void create_menubar(void) {
    g_app->menubar_win = set_app_menu(app_menubar_proc, kMenus, kNumMenus,
                                      handle_menu_command, g_app->hinstance);
    
    g_app->accel = load_accelerators(kAccelEntries,
        (int)(sizeof(kAccelEntries)/sizeof(kAccelEntries[0])));
    
    if (g_app->menubar_win)
        send_message(g_app->menubar_win, kMenuBarMessageSetAccelerators,
                     0, g_app->accel);
}

// Create main window
void create_main_window(void) {
    int x = 40;
    int y = MENUBAR_HEIGHT + 40;
    
    window_t *win = create_window_from_form(&myapp_main_window_form,
                                            x, y, NULL, main_win_proc,
                                            g_app->hinstance, NULL);
    show_window(win, true);
}
```

## Standalone vs Gem Mode

### Standalone Mode
```c
// Builds as executable
GEM_STANDALONE_MAIN("My App", UI_INIT_DESKTOP, SCREEN_W, SCREEN_H,
                    g_app->menubar_win, g_app->accel)
```

### Gem Mode
```c
// Builds as shared library (.gem)
GEM_DEFINE("My App", "1.0", gem_init, gem_shutdown, NULL)
```

## Common Mistakes

1. **Not registering database class** — `create_database()` fails
2. **Not cleaning up in `gem_shutdown()`** — memory leaks
3. **Missing `GEM_DEFINE()` macro** — app not registered
4. **Missing `GEM_STANDALONE_MAIN()`** — no standalone entry point
5. **Not calling `register_commctl_classes()`** — controls not available