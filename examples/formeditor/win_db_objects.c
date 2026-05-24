// Database object window for FormEditor.
// Shows database objects using the shared ColumnBrowser control.

#include "formeditor.h"
#include "../../commctl/commctl.h"

#define FE_DBOBJ_LOG(...) axLog("[formeditor][dbobj] " __VA_ARGS__)

#define DBOBJ_COL_DB_W 120
#define DBOBJ_COL_TABLE_W 120
#define DBOBJ_COL_FIELD_W 140

enum {
  DBOBJ_LEVEL_DATABASE = 0,
  DBOBJ_LEVEL_TABLE,
  DBOBJ_LEVEL_FIELD,
};

static window_t *g_db_objects_win = NULL;

static db_t *db_project_at(int idx) {
  if (!g_app || idx < 0 || idx >= g_app->project.database_count)
    return NULL;
  return g_app->project.databases[idx];
}

static const db_schema_def_t *db_schema_at(int idx) {
  db_t *db = db_project_at(idx);
  if (!db)
    return NULL;
  return (const db_schema_def_t *)send_db_message(db, dbGetSchema, 0, NULL);
}

static const db_table_schema_t *dbobj_selected_table(window_t *browser) {
  int db_idx = (int)send_message(browser, CBM_GETSELECTION, DBOBJ_LEVEL_DATABASE, NULL);
  int table_idx = (int)send_message(browser, CBM_GETSELECTION, DBOBJ_LEVEL_TABLE, NULL);
  const db_schema_def_t *schema = db_schema_at(db_idx);
  if (!schema || !schema->tables || table_idx < 0 || table_idx >= schema->table_count)
    return NULL;
  return &schema->tables[table_idx];
}

static int dbobj_number_of_rows(void *ctx, window_t *browser, int column) {
  (void)ctx;

  if (column == DBOBJ_LEVEL_DATABASE) {
    int count = g_app ? g_app->project.database_count : 0;
    return count > 0 ? count : 1;
  }

  if (column == DBOBJ_LEVEL_TABLE) {
    int db_idx = (int)send_message(browser, CBM_GETSELECTION, DBOBJ_LEVEL_DATABASE, NULL);
    const db_schema_def_t *schema = db_schema_at(db_idx);
    return (schema && schema->tables && schema->table_count > 0) ? schema->table_count : 1;
  }

  if (column == DBOBJ_LEVEL_FIELD) {
    const db_table_schema_t *table = dbobj_selected_table(browser);
    return (table && table->fields && table->field_count > 0) ? table->field_count : 1;
  }

  return 0;
}

static bool dbobj_load_cell(void *ctx, window_t *browser, int column, int row,
                            reportview_item_t *item) {
  (void)ctx;
  if (!item)
    return false;

  item->color = get_sys_color(brTextNormal);
  item->userdata = (uint32_t)row;

  if (column == DBOBJ_LEVEL_DATABASE) {
    db_t *db = db_project_at(row);
    item->text = (db && db->name && db->name[0]) ? db->name : "Database";
    if (!db)
      item->text = "DB list alive (no databases loaded)";
    return true;
  }

  if (column == DBOBJ_LEVEL_TABLE) {
    int db_idx = (int)send_message(browser, CBM_GETSELECTION, DBOBJ_LEVEL_DATABASE, NULL);
    const db_schema_def_t *schema = db_schema_at(db_idx);
    if (!schema || !schema->tables || row < 0 || row >= schema->table_count) {
      item->text = "No tables";
      return true;
    }
    item->text = schema->tables[row].name ? schema->tables[row].name : "Table";
    return true;
  }

  if (column == DBOBJ_LEVEL_FIELD) {
    const db_table_schema_t *table = dbobj_selected_table(browser);
    if (!table || !table->fields || row < 0 || row >= table->field_count) {
      item->text = "No columns";
      return true;
    }
    item->text = table->fields[row].name ? table->fields[row].name : "Column";
    return true;
  }

  return false;
}

static bool dbobj_is_leaf(void *ctx, window_t *browser, int column, int row) {
  (void)ctx;
  (void)browser;

  if (column == DBOBJ_LEVEL_DATABASE)
    return db_project_at(row) == NULL;
  if (column == DBOBJ_LEVEL_TABLE)
    return dbobj_selected_table(browser) == NULL;
  return true;
}

static const char *dbobj_title_of_column(void *ctx, window_t *browser, int column) {
  (void)ctx;
  (void)browser;

  switch (column) {
    case DBOBJ_LEVEL_DATABASE: return "Database";
    case DBOBJ_LEVEL_TABLE:    return "Table";
    case DBOBJ_LEVEL_FIELD:    return "Column";
    default:                   return "";
  }
}

static int dbobj_width_of_column(void *ctx, window_t *browser, int column) {
  (void)ctx;
  (void)browser;

  switch (column) {
    case DBOBJ_LEVEL_DATABASE: return DBOBJ_COL_DB_W;
    case DBOBJ_LEVEL_TABLE:    return DBOBJ_COL_TABLE_W;
    case DBOBJ_LEVEL_FIELD:    return DBOBJ_COL_FIELD_W;
    default:                   return DBOBJ_COL_TABLE_W;
  }
}

static void dbobj_did_select(void *ctx, window_t *browser, int column, int row) {
  (void)ctx;
  (void)browser;

  if (column != DBOBJ_LEVEL_DATABASE)
    return;

  db_t *db = db_project_at(row);
  FE_DBOBJ_LOG("db list selection changed: row=%d db=%p", row, (void *)db);
  if (db)
    ui_set_database(db);
}

static const column_browser_delegate_t g_dbobj_delegate = {
  .number_of_rows = dbobj_number_of_rows,
  .load_cell = dbobj_load_cell,
  .is_leaf = dbobj_is_leaf,
  .title_of_column = dbobj_title_of_column,
  .width_of_column = dbobj_width_of_column,
  .did_select = dbobj_did_select,
};

void formeditor_show_database_object_window(int db_index) {
  if (!g_app)
    return;

  if (db_index >= 0 && db_index < g_app->project.database_count) {
    db_t *db = g_app->project.databases[db_index];
    if (db)
      ui_set_database(db);
  }

  if (!g_db_objects_win || !is_window(g_db_objects_win)) {
    g_db_objects_win = create_window("Databases", WINDOW_NOTRAYBUTTON | WINDOW_HSCROLL,
                                     MAKERECT(DOC_START_X + 20, DOC_START_Y + 20, 420, 260),
                                     NULL, win_column_browser, g_app->hinstance, NULL);
    if (!g_db_objects_win)
      return;
    show_window(g_db_objects_win, true);
  }

  snprintf(g_db_objects_win->title, sizeof(g_db_objects_win->title), "Databases");
  send_message(g_db_objects_win, CBM_SETDELEGATE, 0, (void *)&g_dbobj_delegate);
  send_message(g_db_objects_win, CBM_SETMINCOLUMNWIDTH, DBOBJ_COL_DB_W, NULL);
  send_message(g_db_objects_win, CBM_LOADCOLUMNZERO, 0, NULL);

  move_to_top(g_db_objects_win);
  invalidate_window(g_db_objects_win);
}

void formeditor_close_database_object_window(void) {
  if (g_db_objects_win && is_window(g_db_objects_win))
    destroy_window(g_db_objects_win);
  g_db_objects_win = NULL;
}
