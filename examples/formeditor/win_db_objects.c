// Database object window for FormEditor.
// Shows database objects using the shared ColumnBrowser control.

#include "formeditor.h"
#include "../../commctl/commctl.h"

#define FE_DBOBJ_LOG(...) axLog("[formeditor][dbobj] " __VA_ARGS__)

#define DBOBJ_COL_DB_W 120
#define DBOBJ_COL_TABLE_W 120
#define DBOBJ_COL_FIELD_W 140
#define DBOBJ_COL_RELATED_FIELD_W 160

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

static const db_schema_def_t *dbobj_selected_schema(window_t *browser) {
  int db_idx = (int)send_message(browser, CBM_GETSELECTION, DBOBJ_LEVEL_DATABASE, NULL);
  return db_schema_at(db_idx);
}

static const db_table_schema_t *dbobj_table_by_id(const db_schema_def_t *schema, uint32_t table_id) {
  if (!schema || !schema->tables || schema->table_count <= 0 || table_id == 0)
    return NULL;
  for (int i = 0; i < schema->table_count; i++) {
    if (schema->tables[i].table_id == table_id)
      return &schema->tables[i];
  }
  return NULL;
}

static const db_table_schema_t *dbobj_table_for_column(window_t *browser, int column) {
  const db_schema_def_t *schema = dbobj_selected_schema(browser);
  if (!schema || !schema->tables || column < DBOBJ_LEVEL_FIELD)
    return NULL;

  int table_idx = (int)send_message(browser, CBM_GETSELECTION, DBOBJ_LEVEL_TABLE, NULL);
  if (table_idx < 0 || table_idx >= schema->table_count)
    return NULL;

  const db_table_schema_t *table = &schema->tables[table_idx];
  for (int c = DBOBJ_LEVEL_FIELD; c < column; c++) {
    int field_idx = (int)send_message(browser, CBM_GETSELECTION, c, NULL);
    if (!table || !table->fields || field_idx < 0 || field_idx >= table->field_count)
      return NULL;
    const db_field_schema_t *field = &table->fields[field_idx];
    if (field->relation_table_id == 0)
      return NULL;
    table = dbobj_table_by_id(schema, field->relation_table_id);
  }
  return table;
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

  if (column >= DBOBJ_LEVEL_FIELD) {
    const db_table_schema_t *table = dbobj_table_for_column(browser, column);
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

  if (column >= DBOBJ_LEVEL_FIELD) {
    const db_table_schema_t *table = dbobj_table_for_column(browser, column);
    if (!table || !table->fields || row < 0 || row >= table->field_count) {
      item->text = "No columns";
      return true;
    }
    const db_field_schema_t *field = &table->fields[row];
    item->text = field->name ? field->name : "Column";
    if (field->relation_table_id != 0 && table->joins) {
      for (int i = 0; i < table->join_count; i++) {
        if (table->joins[i].local_field_id == field->field_id && table->joins[i].name) {
          item->text = table->joins[i].name;
          break;
        }
      }
    }
    return true;
  }

  return false;
}

static bool dbobj_is_leaf(void *ctx, window_t *browser, int column, int row) {
  (void)ctx;

  if (column == DBOBJ_LEVEL_DATABASE)
    return db_project_at(row) == NULL;
  if (column == DBOBJ_LEVEL_TABLE) {
    int db_idx = (int)send_message(browser, CBM_GETSELECTION, DBOBJ_LEVEL_DATABASE, NULL);
    const db_schema_def_t *schema = db_schema_at(db_idx);
    if (!schema || !schema->tables || row < 0 || row >= schema->table_count)
      return true;
    const db_table_schema_t *table = &schema->tables[row];
    return !table->fields || table->field_count <= 0;
  }
  if (column >= DBOBJ_LEVEL_FIELD) {
    const db_table_schema_t *table = dbobj_table_for_column(browser, column);
    if (!table || !table->fields || row < 0 || row >= table->field_count)
      return true;
    return table->fields[row].relation_table_id == 0;
  }
  return true;
}

static const char *dbobj_title_of_column(void *ctx, window_t *browser, int column) {
  (void)ctx;
  (void)browser;

  switch (column) {
    case DBOBJ_LEVEL_DATABASE: return "Database";
    case DBOBJ_LEVEL_TABLE:    return "Table";
    case DBOBJ_LEVEL_FIELD:    return "Column";
    default:
      if (column > DBOBJ_LEVEL_FIELD) {
        const db_table_schema_t *table = dbobj_table_for_column(browser, column);
        return table && table->name ? table->name : "Columns";
      }
      return "";
  }
}

static int dbobj_width_of_column(void *ctx, window_t *browser, int column) {
  (void)ctx;
  (void)browser;

  switch (column) {
    case DBOBJ_LEVEL_DATABASE: return DBOBJ_COL_DB_W;
    case DBOBJ_LEVEL_TABLE:    return DBOBJ_COL_TABLE_W;
    case DBOBJ_LEVEL_FIELD:    return DBOBJ_COL_FIELD_W;
    default:
      return column > DBOBJ_LEVEL_FIELD ? DBOBJ_COL_RELATED_FIELD_W
                                        : DBOBJ_COL_TABLE_W;
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
