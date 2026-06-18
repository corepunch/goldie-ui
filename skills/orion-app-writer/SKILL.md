---
name: orion-app-writer
description: Writes Orion UI framework apps — defining .orion files, databases, views, controllers, and tests. Provides project context, framework docs, and usage examples. Applies when working with Orion apps, .orion files, or any project using the Orion UI framework.
user-invocable: false
allowed-tools: Bash(make *), Bash(gcc *), Bash(./build/*)
---

# Orion App Writer

A framework for writing retro-styled desktop apps using C and declarative XML definitions. Apps follow MVC pattern with message-based architecture similar to Win32 API.

> **IMPORTANT:** Orion uses a message-based architecture. Windows use `winproc_t` callbacks, databases use `dbproc_t` callbacks. All communication happens via messages with wparam/lparam.

## Current Project Context

The Orion framework is at `/Users/igor/Developer/mapview/ui`. Examples are in `examples/` directory.

## Principles

1. **Declarative first.** Define UI in `.orion` XML files, not C code. Use `form_def_t` for dialogs.
2. **MVC separation.** Model (database), View (windows/dialogs), Controller (business logic).
3. **Message-based communication.** Use `send_message()` for windows, `send_db_message()` for databases.
4. **Resource management.** Use `gem_init()`/`gem_shutdown()` pattern with `GEM_DEFINE()` macro.

## Critical Rules

These rules are **always enforced**. Each links to a file with Incorrect/Correct code pairs.

### .orion File Structure → [orion-file.md](rules/orion-file.md)

-   **Root element must be `<orion>`** with version, name, title, root attributes.
-   **Menus use `var` and `count` attributes** for generated C arrays.
-   **Toolbars contain `<Button>` elements** with name, menu, icon, text attributes.
-   **Databases define tables with fields** using type, length, relation attributes.
-   **Forms define UI layout** using StackView, GridView, TableView, etc.

### Database Schema → [database-schema.md](rules/database-schema.md)

-   **Field types: string, integer, bool, float, relationship** — never use unsupported types.
-   **String fields require `length` attribute** — always specify buffer size.
-   **Relationships use `relation` and `many` attributes** — define foreign keys properly.
-   **Table names must be unique** within a database definition.

### View Layer → [view-layer.md](rules/view-layer.md)

-   **Window procedures handle messages** via switch statements — always call `default_winproc()` for unhandled messages.
-   **Use `evCreate` for initialization** — create controls, allocate state, set initial values.
-   **Use `evPaint` for custom drawing** — return false to let framework draw controls.
-   **Use `evCommand` for notifications** — handle `btnClicked`, `RVN_SELCHANGE`, etc.

### Controller Layer → [controller-layer.md](rules/controller-layer.md)

-   **App state in global `g_app` pointer** — allocate in `app_init()`, free in `app_shutdown()`.
-   **Database operations use `send_db_message()`** — never access database internals directly.
-   **Use `MAKEDWORD()` for table/field IDs** — combine table ID and filter field.
-   **Free result lists after `dbFetch()`** — call `free_result_list()` to avoid leaks.

### Entry Point → [entry-point.md](rules/entry-point.md)

-   **Use `GEM_DEFINE()` macro** — registers app with framework.
-   **Use `GEM_STANDALONE_MAIN()` for standalone apps** — provides event loop.
-   **Register database classes with `DB_CLASS()`** — before creating database instances.
-   **Register databases with `register_database()`** — for declarative form bindings.

### Testing → [testing.md](rules/testing.md)

-   **Test database operations** — insert, find, update, delete, fetch.
-   **Test dialogs with `show_db_dialog()`** — verify form bindings work.
-   **Clean up test databases** — call `destroy_database()` after tests.
-   **Check return values** — assert database operations succeed.

## Key Patterns

These are the most common patterns that differentiate correct Orion code.

```c
// App state structure
typedef struct {
    database_t *db;
    window_t *main_win;
    window_t *menubar_win;
    hinstance_t hinstance;
    accel_table_t *accel;
} app_state_t;

app_state_t *g_app = NULL;

// Window procedure pattern
lresult_t my_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
    switch (msg) {
        case evCreate:    // Initialize
        case evPaint:     // Draw custom content
        case evCommand:   // Handle notifications
        case evDestroy:   // Cleanup
        default:          // Call default_winproc()
    }
}

// Database operations
item_t *inserted = (item_t *)send_db_message(db, dbInsert, ID_DB_ITEMS, &item);
item_t *found = (item_t *)send_db_message(db, dbFind, MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)id);
send_db_message(db, dbUpdate, ID_DB_ITEMS, found);
send_db_message(db, dbDelete, ID_DB_ITEMS, (void *)(intptr_t)id);
result_node_t *results = (result_node_t *)send_db_message(db, dbFetch, MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)0);
free_result_list(results);
```

## Component Selection

| Need | Use |
|------|-----|
| Main window | `create_window_from_form()` with form from .orion |
| Modal dialog | `show_dialog_from_form()` or `show_db_dialog()` |
| Data display | `TableView` with `source="db.table"` binding |
| Form inputs | `TextBox`, `ComboBox`, `MultiEdit` with `field="db.table.field"` |
| Menu bar | `set_app_menu()` with menu definitions from .orion |
| Toolbar | `toolbar` attribute on form, Button elements in .orion |

## Workflow

1. **Create .orion file** — define menus, toolbars, databases, forms.
2. **Implement database** — create `db_simple_xml.c` with message handlers.
3. **Create header file** — include generated IDs from .orion.
4. **Implement views** — window procedures for main window, dialogs.
5. **Implement controller** — app state, business logic functions.
6. **Create entry point** — `gem_init()`, `gem_shutdown()`, `GEM_DEFINE()`.
7. **Add seed data** — create XML file with initial database records.
8. **Write tests** — database operations, dialog functionality.
9. **Build and test** — `make examples`, `make test`.

## Detailed References

-   [rules/orion-file.md](rules/orion-file.md) — .orion XML structure, menus, toolbars, databases, forms
-   [rules/database-schema.md](rules/database-schema.md) — Field types, relationships, table definitions
-   [rules/view-layer.md](rules/view-layer.md) — Window procedures, messages, controls, dialogs
-   [rules/controller-layer.md](rules/controller-layer.md) — App state, database operations, business logic
-   [rules/entry-point.md](rules/entry-point.md) — GEM_DEFINE, gem_init, GEM_STANDALONE_MAIN
-   [rules/testing.md](rules/testing.md) — Database tests, dialog tests, cleanup
-   [reference/messages.md](reference/messages.md) — Window and database message reference
-   [reference/controls.md](reference/controls.md) — Available controls and their usage
-   [reference/database-api.md](reference/database-api.md) — Database message API reference