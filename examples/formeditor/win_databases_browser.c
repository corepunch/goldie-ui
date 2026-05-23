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
  const db_schema_def_t *schema;
  const db_api_def_t *api;
  int selected_db_idx;
  int selected_table_id;
} db_datasource_ctx_t;

static db_datasource_ctx_t g_db_ctx = {0};

static void db_ctx_refresh_schema(db_datasource_ctx_t *ds) {
  if (!ds || !ds->db) {
    if (ds) {
      ds->schema = NULL;
      ds->api = NULL;
    }
    return;
  }
  ds->schema = (const db_schema_def_t *)send_db_message(ds->db, dbGetSchema, 0, NULL);
  ds->api = (const db_api_def_t *)send_db_message(ds->db, dbGetApi, 0, NULL);
}

static int db_project_database_count(const db_datasource_ctx_t *ds) {
  return (ds && ds->app) ? ds->app->project.database_count : 0;
}

static const form_project_database_t *db_project_database(const db_datasource_ctx_t *ds) {
  if (!ds || !ds->app || ds->selected_db_idx < 0 ||
      ds->selected_db_idx >= ds->app->project.database_count)
    return NULL;
  return &ds->app->project.databases[ds->selected_db_idx];
}

static int db_schema_table_count(const db_datasource_ctx_t *ds) {
  const form_project_database_t *pdb = db_project_database(ds);
  if (pdb)
    return pdb->table_count;
  return (ds && ds->schema) ? ds->schema->table_count : 0;
}

static bool db_has_actions(const db_datasource_ctx_t *ds) {
  return ds && ds->api && ds->api->action_count > 0;
}

static bool db_has_outlets(const db_datasource_ctx_t *ds) {
  return ds && ds->api && ds->api->outlet_count > 0;
}

static int db_actions_parent_index(const db_datasource_ctx_t *ds) {
  return db_has_actions(ds) ? db_schema_table_count(ds) : -1;
}

static int db_outlets_parent_index(const db_datasource_ctx_t *ds) {
  int idx = db_schema_table_count(ds);
  if (db_has_actions(ds)) idx++;
  return db_has_outlets(ds) ? idx : -1;
}

// Column browser data source callbacks
static int db_get_child_count(void *ctx, int column, int parent_idx) {
  db_datasource_ctx_t *ds = (db_datasource_ctx_t *)ctx;
  
  if (column == 0) {
    int project_count = db_project_database_count(ds);
    return project_count > 0 ? project_count : ((ds && ds->db) ? 1 : 0);
  } else if (column == 1) {
    if (ds)
      ds->selected_db_idx = parent_idx;
    int count = db_schema_table_count(ds);
    if (db_has_actions(ds)) count++;
    if (db_has_outlets(ds)) count++;
    return count;
  } else if (column == 2) {
    if (!ds)
      return 0;
    const form_project_database_t *pdb = db_project_database(ds);
    if (pdb && parent_idx >= 0 && parent_idx < pdb->table_count) {
      const form_project_db_table_t *table = &pdb->tables[parent_idx];
      return table->field_count + table->join_count;
    }
    if (ds->schema && parent_idx >= 0 && parent_idx < ds->schema->table_count) {
      const db_table_schema_t *table = &ds->schema->tables[parent_idx];
      return table->field_count + table->join_count;
    }
    if (parent_idx == db_actions_parent_index(ds))
      return ds->api->action_count;
    if (parent_idx == db_outlets_parent_index(ds))
      return ds->api->outlet_count;
    return 0;
  }
  
  return 0;
}

static const char *db_get_child_title(void *ctx, int column, int parent_idx, int child_idx) {
  db_datasource_ctx_t *ds = (db_datasource_ctx_t *)ctx;
  (void)parent_idx;
  
  if (column == 0) {
    // Column 0: Database names
    if (ds && ds->app && child_idx >= 0 && child_idx < ds->app->project.database_count)
      return ds->app->project.databases[child_idx].name;
    if (child_idx == 0 && ds && ds->db)
      return ds->db->name;
    return "?";
  } else if (column == 1) {
    if (ds)
      ds->selected_db_idx = parent_idx;
    const form_project_database_t *pdb = db_project_database(ds);
    if (pdb && child_idx >= 0 && child_idx < pdb->table_count)
      return pdb->tables[child_idx].name;
    if (ds && ds->schema && child_idx >= 0 && child_idx < ds->schema->table_count)
      return ds->schema->tables[child_idx].name;
    if (child_idx == db_actions_parent_index(ds))
      return "Actions";
    if (child_idx == db_outlets_parent_index(ds))
      return "Outlets";
    return "?";
  } else if (column == 2) {
    static char title[160];
    if (!ds)
      return "?";
    const form_project_database_t *pdb = db_project_database(ds);
    if (pdb && parent_idx >= 0 && parent_idx < pdb->table_count) {
      const form_project_db_table_t *table = &pdb->tables[parent_idx];
      if (child_idx >= 0 && child_idx < table->field_count) {
        const form_project_db_field_t *field = &table->fields[child_idx];
        if (field->relation_table[0] && field->relation_field[0]) {
          snprintf(title, sizeof(title), "%s -> %s.%s",
                   field->name, field->relation_table, field->relation_field);
          return title;
        }
        return field->name;
      }
      int join_idx = child_idx - table->field_count;
      if (join_idx >= 0 && join_idx < table->join_count) {
        const form_project_db_join_t *join = &table->joins[join_idx];
        snprintf(title, sizeof(title), "%s.* via %s -> %s.%s",
                 join->name, join->local_field,
                 join->foreign_table, join->foreign_field);
        return title;
      }
      return "?";
    }
    if (ds->schema && parent_idx >= 0 && parent_idx < ds->schema->table_count) {
      const db_table_schema_t *table = &ds->schema->tables[parent_idx];
      if (child_idx >= 0 && child_idx < table->field_count) {
        const db_field_schema_t *field = &table->fields[child_idx];
        if (field->relation_table && field->relation_field) {
          snprintf(title, sizeof(title), "%s -> %s.%s",
                   field->name, field->relation_table, field->relation_field);
          return title;
        }
        return field->name;
      }
      int join_idx = child_idx - table->field_count;
      if (join_idx >= 0 && join_idx < table->join_count) {
        const db_join_schema_t *join = &table->joins[join_idx];
        snprintf(title, sizeof(title), "%s.* via %s -> %s.%s",
                 join->name, join->local_field,
                 join->foreign_table, join->foreign_field);
        return title;
      }
      return "?";
    }
    if (parent_idx == db_actions_parent_index(ds) && ds->api &&
        child_idx >= 0 && child_idx < ds->api->action_count) {
      const db_action_def_t *action = &ds->api->actions[child_idx];
      snprintf(title, sizeof(title), "%s -> %s",
               action->name ? action->name : "?",
               action->target ? action->target : "");
      return title;
    }
    if (parent_idx == db_outlets_parent_index(ds) && ds->api &&
        child_idx >= 0 && child_idx < ds->api->outlet_count) {
      const db_outlet_def_t *outlet = &ds->api->outlets[child_idx];
      snprintf(title, sizeof(title), "%s -> %s",
               outlet->name ? outlet->name : "?",
               outlet->target ? outlet->target : "");
      return title;
    }
    return "?";
  }
  
  return "?";
}

static bool db_is_leaf(void *ctx, int column, int idx) {
  (void)ctx;
  (void)idx;
  
  if (column == 0) return false;  // Databases have tables
  if (column == 1) return false;  // Tables have fields
  if (column == 2) return true;   // Fields are leaves
  
  return true;
}

static void db_browser_refresh(window_t *win) {
  db_browser_state_t *dbs = (db_browser_state_t *)win->userdata;
  if (!dbs || !dbs->browser_win) return;
  
  // Refresh the database reference (in case it changed)
  if (g_app) {
    g_db_ctx.db = ui_get_database();
    g_db_ctx.app = g_app;
    db_ctx_refresh_schema(&g_db_ctx);
  }
  
  // Refresh browser
  send_message(dbs->browser_win, cbRefresh, 0, NULL);
}

static void db_browser_observer(fe_event_type_t event, window_t *doc, void *ctx) {
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

static lresult_t db_browser_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
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
      g_db_ctx.db = ui_get_database();
      g_db_ctx.app = g_app;
      g_db_ctx.selected_table_id = -1;
      g_db_ctx.selected_db_idx = -1;
      db_ctx_refresh_schema(&g_db_ctx);
      
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
        free(dbs);
        win->userdata = NULL;
      }
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
      return default_winproc(win, msg, wparam, lparam);
  }
}

// Public API: Create database browser window
window_t *create_database_browser(const irect16_t *frame, window_t *parent,
                                  hinstance_t hinstance) {
  window_t *win = create_window("Databases",
    WINDOW_NOTRAYBUTTON,
    frame,
    parent, db_browser_proc, hinstance, NULL);
  if (win) show_window(win, true);
  return win;
}

void databases_browser_refresh(void) {
  if (g_app && g_app->windows[FE_WIN_DATABASES] && is_window(g_app->windows[FE_WIN_DATABASES]))
    db_browser_refresh(g_app->windows[FE_WIN_DATABASES]);
}
