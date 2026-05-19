// Database browser with NSBrowser-style column navigation
// Shows databases → tables → fields hierarchy
// Subscribes to project notifications for automatic refresh

#include "formeditor.h"
#include "../../commctl/commctl.h"
#include <string.h>

typedef struct {
  window_t *browser_win;
  window_t *toolbar_win;
  int subscription_id;
} db_browser_state_t;

// Data source context for column browser
typedef struct {
  app_state_t *app;
  database_t *db;
  table_name_node_t *tables;
  table_schema_t *current_schema;
  int selected_table_id;
} db_datasource_ctx_t;

static db_datasource_ctx_t g_db_ctx = {0};

// Column browser data source callbacks
static int db_get_child_count(void *ctx, int column, int parent_idx) {
  db_datasource_ctx_t *ds = (db_datasource_ctx_t *)ctx;
  (void)parent_idx;
  
  if (column == 0) {
    // Column 0: List of registered databases (for now, just 1)
    return 1;  // Single "db" database
  } else if (column == 1) {
    // Column 1: Tables in selected database
    if (!ds->tables && ds->db) {
      ds->tables = (table_name_node_t *)send_db_message(ds->db, dbGetTables, 0, NULL);
    }
    int count = 0;
    for (table_name_node_t *t = ds->tables; t; t = t->next)
      count++;
    return count;
  } else if (column == 2) {
    // Column 2: Fields in selected table
    if (ds->current_schema)
      return ds->current_schema->field_count;
    return 0;
  }
  
  return 0;
}

static const char *db_get_child_title(void *ctx, int column, int parent_idx, int child_idx) {
  db_datasource_ctx_t *ds = (db_datasource_ctx_t *)ctx;
  static char buf[128];
  (void)parent_idx;
  
  if (column == 0) {
    // Column 0: Database names
    if (child_idx == 0 && ds->db)
      return ds->db->name;
    return "?";
  } else if (column == 1) {
    // Column 1: Table names
    table_name_node_t *t = ds->tables;
    for (int i = 0; i < child_idx && t; i++)
      t = t->next;
    if (t) {
      ds->selected_table_id = t->table_id;  // Cache for column 2
      return t->name;
    }
    return "?";
  } else if (column == 2) {
    // Column 2: Field names with types
    if (ds->current_schema && child_idx < ds->current_schema->field_count) {
      const db_field_meta_t *field = &ds->current_schema->fields[child_idx];
      const char *type_name = "?";
      switch (field->type) {
        case DB_TYPE_INT: type_name = "int"; break;
        case DB_TYPE_STRING: type_name = "string"; break;
        case DB_TYPE_BOOL: type_name = "bool"; break;
        case DB_TYPE_FLOAT: type_name = "float"; break;
        case DB_TYPE_DOUBLE: type_name = "double"; break;
      }
      snprintf(buf, sizeof(buf), "%s (%s)", field->name, type_name);
      return buf;
    }
    return "?";
  }
  
  return "?";
}

static bool db_is_leaf(void *ctx, int column, int idx) {
  db_datasource_ctx_t *ds = (db_datasource_ctx_t *)ctx;
  (void)idx;
  
  if (column == 0) return false;  // Databases have tables
  if (column == 1) {
    // Tables have fields - fetch schema when selected
    table_name_node_t *t = ds->tables;
    for (int i = 0; i < idx && t; i++)
      t = t->next;
    if (t) {
      ds->current_schema = (table_schema_t *)send_db_message(ds->db, dbGetTableSchema, t->table_id, NULL);
    }
    return false;  // Tables have fields
  }
  if (column == 2) return true;   // Fields are leaves
  
  return true;
}

static void db_browser_refresh(window_t *win) {
  db_browser_state_t *dbs = (db_browser_state_t *)win->userdata;
  if (!dbs || !dbs->browser_win) return;
  
  // Free old table list
  if (g_db_ctx.tables) {
    free_result_list(g_db_ctx.tables);
    g_db_ctx.tables = NULL;
  }
  g_db_ctx.current_schema = NULL;
  
  // Refresh the database reference (in case it changed)
  if (g_app) {
    // For now, hardcode to socialfeed's database
    // TODO: Make this dynamic based on loaded plugins
    g_db_ctx.db = get_database_by_name("db");
    g_db_ctx.app = g_app;
  }
  
  // Refresh browser
  send_message(dbs->browser_win, cbRefresh, 0, NULL);
}

static void db_browser_observer(fe_event_type_t event, form_doc_t *doc, void *ctx) {
  (void)doc;
  window_t *win = (window_t *)ctx;
  
  switch (event) {
    case FE_EVENT_DOCUMENT_CREATED:
    case FE_EVENT_DOCUMENT_CLOSED:
    case FE_EVENT_PROJECT_MODIFIED:
      db_browser_refresh(win);
      break;
    default:
      break;
  }
}

static result_t db_browser_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  db_browser_state_t *dbs = (db_browser_state_t *)win->userdata;
  
  switch (msg) {
    case evCreate: {
      dbs = calloc(1, sizeof(db_browser_state_t));
      if (!dbs) return false;
      
      win->userdata = dbs;
      
      // Create toolbar (future: add refresh button)
      dbs->toolbar_win = create_window("",
        WINDOW_TOOLBAR,
        MAKERECT(0, 0, win->frame.w, 24),
        win, NULL, 0, NULL);
      
      // Create column browser
      irect16_t cr = get_client_rect(win);
      dbs->browser_win = create_window("",
        WINDOW_NOTITLE | WINDOW_NOFILL,
        MAKERECT(0, 24, cr.w, cr.h - 24),
        win, win_column_browser, 0, NULL);
      
      if (!dbs->browser_win) {
        free(dbs);
        return false;
      }
      
      // Setup data source
      g_db_ctx.db = get_database_by_name("db");  // Hardcoded for now
      g_db_ctx.app = g_app;
      g_db_ctx.tables = NULL;
      g_db_ctx.current_schema = NULL;
      g_db_ctx.selected_table_id = -1;
      
      column_browser_datasource_t datasource = {
        .get_child_count = db_get_child_count,
        .get_child_title = db_get_child_title,
        .is_leaf = db_is_leaf,
        .userdata = &g_db_ctx,
      };
      
      send_message(dbs->browser_win, cbSetDataSource, 0, &datasource);
      
      // Subscribe to project events
      dbs->subscription_id = fe_subscribe(db_browser_observer, win);
      
      return true;
    }
    
    case evDestroy:
      if (dbs) {
        if (dbs->subscription_id > 0)
          fe_unsubscribe(dbs->subscription_id);
        if (g_db_ctx.tables) {
          free_result_list(g_db_ctx.tables);
          g_db_ctx.tables = NULL;
        }
        free(dbs);
        win->userdata = NULL;
      }
      return true;
    
    case evPaint:
      // Browser draws itself
      return true;
    
    case evResize: {
      if (!dbs || !dbs->browser_win) return false;
      
      irect16_t cr = get_client_rect(win);
      
      if (dbs->toolbar_win)
        resize_window(dbs->toolbar_win, cr.w, 24);
      
      resize_window(dbs->browser_win, cr.w, cr.h - 24);
      move_window(dbs->browser_win, 0, 24);
      
      return true;
    }
    
    case evCommand: {
      // Handle column browser selection changes
      window_t *src = (window_t *)lparam;
      if (src == dbs->browser_win) {
        // TODO: Implement drag & drop or double-click to bind
        // For now, just log selection
        return true;
      }
      return false;
    }
    
    default:
      return false;
  }
}

// Public API: Create database browser window
window_t *create_database_browser(const irect16_t *frame, window_t *parent) {
  return create_window("Databases",
    WINDOW_NOTRAYBUTTON,
    frame,
    parent, db_browser_proc, 0, NULL);
}
