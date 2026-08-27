---
layout: default
title: Tableview Migration Guide
nav_exclude: true
---

# Tableview Migration Guide

## From Manual Reportview Population to Declarative Tableviews

This guide documents the migration process from manual `reportview` population code to fully declarative `tableview` definitions in `.orion` files, as demonstrated in the socialfeed example.

---

## Overview

**Before:** Applications manually created reportviews, fetched database records in loops, formatted text for each row, and called `rvAddItem()` repeatedly.

**After:** Applications declare tableviews in `.orion` XML with database bindings. The `orionc` compiler generates all necessary glue code. The framework auto-populates everything at runtime.

**Result:** View code shrinks from 200+ lines to ~10 lines of configuration.

---

## Migration Steps

### 1. Declare Tableview in .orion XML

Replace manual reportview creation with declarative tableview:

```xml
<form name="main_window" 
      title="My App"
      frame="0 0 480 400">
  <Toolbar>
    <Button name="refresh" command="view.refresh" icon="refresh" />
  </Toolbar>

  <stack name="content" spacing="0" flags="WINDOW_FLEXSPACE">
    <tableview name="feed"
               database="posts"           <!-- Links to <table name="posts"> -->
               flags="WINDOW_NOTITLE | WINDOW_FLEXSPACE"
               columns="4">
      
      <!-- Column definitions with field bindings -->
      <column title="ID" width="40" field="id" />
      <column title="Author" width="80" field="author" />
      <column title="Title" width="200" field="title" />
      <column title="Likes" width="40" field="likes" />
    </tableview>
  </stack>
</form>
```

**Key attributes:**
- `database="posts"` — Links to `<table name="posts">` in your `<database>` section
- `field="fieldname"` — Binds column to database field (DDX-style interrogation)
- `flags="WINDOW_FLEXSPACE"` — Makes tableview expand to fill parent

### 2. Define Database Schema in .orion

Add database and table definitions (usually at top of .orion file):

```xml
<database name="db" class="SimpleXMLDatabase" source="myapp_seed.xml">
  <table name="posts" model="post">
    <field name="id" type="int" />
    <field name="author" type="string" />
    <field name="title" type="string" />
    <field name="body" type="string" />
    <field name="likes" type="int" />
  </table>
</database>
```

**orionc generates:**
- `TABLE_POSTS` enum constant
- `tableview_params_t` struct with field bindings
- Control IDs: `ID_MAIN_WINDOW_CONTENT`, `ID_MAIN_WINDOW_FEED`

### 3. Register Database at Application Startup

In your `gem_init()` or `main()`:

```c
// Create database instance
g_app->db = create_database("myapp", "db_simple_xml", db_path);
if (!g_app->db) {
  return false;
}

// Register with framework (NeXTSTEP-style singleton)
ui_set_database(g_app->db);
```

**Critical:** Call `ui_set_database()` once at startup. The framework retrieves this automatically for all tableviews.

### 4. Simplify Window Creation Code

**Before (manual approach - 280+ lines):**
```c
void create_main_window(void) {
  // Manual window creation
  window_t *win = create_window("Social Feed", 
                                WINDOW_TOOLBAR | WINDOW_VISIBLE,
                                MAKERECT(x, y, w, h),
                                NULL, main_win_proc, NULL);
  
  // Manual toolbar population
  static const toolbar_button_t buttons[] = { /* ... */ };
  send_message(win, tbAddButtons, COUNT, buttons);
  
  // Manual reportview creation
  window_t *rv = create_window("", WINDOW_NOTITLE,
                               MAKERECT(0, 0, w, h),
                               win, win_report_view, NULL);
  
  // Manual column setup
  static const report_column_t cols[] = { /* ... */ };
  send_message(rv, rvSetColumns, 4, cols);
  
  // Manual data population - MOST COMPLEX PART
  lresult_t res = send_db_message(g_app->db, dbFetch,
                                  MAKEDWORD(TABLE_POSTS, 0), NULL);
  result_node_t *node = (result_node_t *)res;
  while (node) {
    post_t *post = (post_t *)node->record;
    
    // Format each column manually
    char id_str[16], likes_str[16];
    snprintf(id_str, sizeof(id_str), "%d", post->id);
    snprintf(likes_str, sizeof(likes_str), "%d", post->likes);
    
    const char *cells[] = { id_str, post->author, post->title, likes_str };
    send_message(rv, rvAddItem, 4, (void *)cells);
    
    node = node->next;
  }
  free_result_list((result_node_t *)res);
  
  g_app->feed_win = rv;
  show_window(win, true);
}
```

**After (declarative approach - ~15 lines):**
```c
void create_main_window(void) {
  if (!g_app) return;
  int x = 40;
  int y = MENUBAR_HEIGHT + 40;

  // Form contains everything: toolbar + stack + tableview with bindings.
  // Database auto-registered at startup, auto-propagated to tableviews.
  window_t *win = create_window_from_form(&myapp_main_window_form,
                                          x, y,
                                          NULL, main_win_proc,
                                          g_app->hinstance, NULL);
  
  // Look up child windows by ID (robust, generated from .orion)
  g_app->content_win = get_window_item(win, ID_MAIN_WINDOW_CONTENT);
  g_app->feed_win = get_window_item(win, ID_MAIN_WINDOW_FEED);
  
  show_window(win, true);
}
```

**What changed:**
- ❌ No manual `create_window()` with position/size calculations
- ❌ No manual toolbar button array or `tbAddButtons` call
- ❌ No manual reportview creation
- ❌ No manual column setup
- ❌ No manual database fetch loop
- ❌ No manual text formatting per row
- ❌ No fragile child hierarchy traversal (`win->children->children`)
- ✅ One `create_window_from_form()` call
- ✅ ID-based child lookup with `get_window_item()`
- ✅ Everything auto-populated from `.orion` + database

### 5. Simplify Window Procedure

**Before:**
```c
result_t main_win_proc(window_t *win, uint32_t msg,
                       uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      g_app->main_win = win;
      
      // Manually find reportview child (fragile!)
      g_app->feed_win = win->children;
      while (g_app->feed_win && g_app->feed_win->proc != win_report_view)
        g_app->feed_win = g_app->feed_win->next;
      
      // Manually populate reportview (see 50+ lines above)
      populate_feed_view();
      return true;
    
    // ... rest of message handlers
  }
}
```

**After:**
```c
result_t main_win_proc(window_t *win, uint32_t msg,
                       uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      if (!g_app) return false;
      g_app->main_win = win;

      // Form creates hierarchy automatically.
      // Database auto-propagated during form creation.
      // Just look up windows by ID.
      g_app->content_win = get_window_item(win, ID_MAIN_WINDOW_CONTENT);
      g_app->feed_win = get_window_item(win, ID_MAIN_WINDOW_FEED);

      app_update_status();
      return true;
    
    // ... rest of message handlers unchanged
  }
}
```

**Key points:**
- Use `get_window_item(win, ID)` instead of traversing `win->children`
- No manual population code needed
- Tableview auto-populates from database during form creation

### 6. Refresh Pattern

To refresh tableview data after changes:

```c
void feed_refresh(void) {
  if (!g_app || !g_app->feed_win) return;
  
  // Tableview handles everything automatically
  send_message(g_app->feed_win, tvRefresh, 0, NULL);
}
```

Call `feed_refresh()` after:
- Adding/deleting records
- Updating record fields
- Changing filter criteria

---

## Complete Before/After Example

### Before: Manual Population (view_main.c - old approach)

```c
// 280+ lines of manual setup...

void populate_feed_view(void) {
  if (!g_app || !g_app->feed_win) return;
  
  // Clear existing items
  send_message(g_app->feed_win, rvClear, 0, NULL);
  
  // Fetch all posts from database
  lresult_t res = send_db_message(g_app->db, dbFetch,
                                  MAKEDWORD(TABLE_POSTS, 0), NULL);
  result_node_t *node = (result_node_t *)res;
  
  // Manually format and add each row
  while (node) {
    post_t *post = (post_t *)node->record;
    
    char id_buf[16];
    char likes_buf[16];
    snprintf(id_buf, sizeof(id_buf), "%d", post->id);
    snprintf(likes_buf, sizeof(likes_buf), "%d", post->likes);
    
    const char *cells[4] = {
      id_buf,
      post->author,
      post->title,
      likes_buf
    };
    
    send_message(g_app->feed_win, rvAddItem, 4, (void *)cells);
    node = node->next;
  }
  
  free_result_list((result_node_t *)res);
}
```

### After: Declarative (view_main.c - new approach)

```c
// 15 lines total...

void create_main_window(void) {
  if (!g_app) return;
  int x = 40;
  int y = MENUBAR_HEIGHT + 40;

  window_t *win = create_window_from_form(&socialfeed_main_window_form,
                                          x, y,
                                          NULL, main_win_proc,
                                          g_app->hinstance, NULL);
  show_window(win, true);
}

void feed_refresh(void) {
  if (!g_app || !g_app->feed_win) return;
  send_message(g_app->feed_win, tvRefresh, 0, NULL);
}
```

---

## Common Patterns

### Filtered Tableviews (e.g., Comments for a Post)

**In .orion:**
```xml
<tableview name="comments"
           database="comments"
           filter_field="post_id"    <!-- Filter by this field -->
           flags="WINDOW_NOTITLE | WINDOW_FLEXSPACE">
  <column title="Author" width="80" field="author" />
  <column title="Comment" width="200" field="text" />
</tableview>
```

**In code:**
```c
case evCreate:
  s->comments_win = get_window_item(win, ID_DIALOG_COMMENTS);
  if (s->comments_win) {
    // Database auto-propagated, just set filter value
    send_message(s->comments_win, tvSetFilter, 1, 
                 (void*)(intptr_t)s->post->id);
  }
  return true;
```

### Multiple Tableviews in One Window

```xml
<form name="split_view">
  <splitter orientation="horizontal">
    <tableview name="posts" database="posts" />
    <tableview name="comments" database="comments" filter_field="post_id" />
  </splitter>
</form>
```

Look up each by ID:
```c
g_app->posts_win = get_window_item(win, ID_SPLIT_VIEW_POSTS);
g_app->comments_win = get_window_item(win, ID_SPLIT_VIEW_COMMENTS);
```

---

## Key Architecture Principles

### 1. NeXTSTEP-Style Database Singleton

**Pattern:**
```c
// App startup:
ui_set_database(g_app->db);

// Framework auto-retrieves for all tableviews:
database_t *db = ui_get_database();
```

**Why:** Eliminates passing `db` parameter through every form/dialog API. Matches NeXTSTEP app delegate pattern where data sources live in application context.

### 2. ID-Based Window Lookup

**Pattern:**
```c
// Robust (uses generated IDs):
window_t *child = get_window_item(parent, ID_FORM_CHILD);

// Fragile (assumes structure):
window_t *child = parent->children;  // ❌ DON'T DO THIS
```

**Why:** 
- Works even if form structure changes
- Self-documenting (ID name indicates purpose)
- Recursive search through entire hierarchy
- Matches WinAPI `GetDlgItem()` pattern

### 3. Zero Wrapper Structs

Database and tableview APIs pass values directly via `wparam`/`lparam`:

```c
// Direct value passing (WinAPI style):
send_message(tv, tvSetFilter, field_id, (void*)(intptr_t)value);
send_message(tv, tvRefresh, 0, NULL);

// ❌ Don't create parameter structs:
// typedef struct { int field; int value; } filter_params_t;
```

### 4. DDX-Style Field Interrogation

Tableview uses `db_object_proc_t` to query field values:

```c
// Framework code (automatic):
const char *text = (const char *)obj_proc(record, column_id, NULL);
```

Your database implementation provides field getters. The tableview never knows the concrete record type.

---

## Troubleshooting

### Tableview shows empty even though database has records

**Check:**
1. Did you call `ui_set_database(db)` at startup?
2. Does `.orion` `database="table_name"` match `<table name="table_name">`?
3. Do `field="name"` attributes match actual database field names?
4. Is `dbGetObjectProc` implemented in your database class?
5. Does your object proc handle the field IDs correctly?

### Can't find child window

**Wrong:**
```c
window_t *child = win->children;  // Assumes first child
```

**Right:**
```c
window_t *child = get_window_item(win, ID_FORM_CHILD);
```

Check generated IDs in `build/generated/myapp/myapp.h`.

### Toolbar doesn't appear

Nest a `<Toolbar>` inside the form that owns it:

```xml
<form name="main" ...>
  <Toolbar>
    <Button icon="add" command="file.new" />
  </Toolbar>
</form>
```

---

## Migration Checklist

- [ ] Add `<database>` and `<table>` definitions to `.orion`
- [ ] Convert `<reportview>` elements to `<tableview>` with `database=` and `field=` attributes
- [ ] Nest `<Toolbar>` inside forms that own toolbars
- [ ] Rebuild with `make` to regenerate `build/generated/*/forms.h`
- [ ] Call `ui_set_database(db)` at app startup
- [ ] Replace `create_window()` calls with `create_window_from_form()`
- [ ] Remove database parameter from form/dialog calls
- [ ] Replace `win->children` traversal with `get_window_item(win, ID)`
- [ ] Delete manual population functions (`populate_*`, `refresh_*`)
- [ ] Replace manual refresh with `send_message(tv, tvRefresh, 0, NULL)`
- [ ] Test: Add/delete/update records, verify auto-refresh

---

## Files Changed in socialfeed Migration

**Added:**
- `commctl/tableview.c` — Generic tableview control
- `docs/database-codegen.md` — Database schema codegen docs
- `apps/socialfeed/socialfeed.orion` — Declarative UI

**Modified:**
- `tools/orionc.c` — Tableview generation
- `user/user.h` — Database singleton APIs
- `user/window.c` — Auto-propagation logic
- `user/dialog.c` — Removed db parameter
- `apps/socialfeed/view_main.c` — 280 lines → 15 lines
- `apps/socialfeed/view_dlg_post.c` — Similar reduction

**Deleted:**
- Manual population loops
- Manual column formatting
- Manual database fetch code
- Fragile child traversal

---

## Summary

**Old way:** 200+ lines of manual C code per view  
**New way:** 20 lines of declarative XML + 10 lines of C

**Result:** 95% less boilerplate, zero manual population, automatic database→UI binding.

This is the Orion framework philosophy: **If manual C code exists where `.orion` XML exists, that's a missing compiler feature.**
