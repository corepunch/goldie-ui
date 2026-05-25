// Database object browser for FormEditor.
//
// Displays project databases as a NeXTSTEP-style column browser:
//
//   [ Databases ] -> [ Tables ] -> [ Fields ] -> [ Related fields ... ]
//
// Each column is driven lazily: selecting a row reveals the next column.
// Relational fields (those with a relation_table_id) are non-leaf and expand
// into another field column for the related table, repeating indefinitely.
//
// Delegate pattern: load_cell stores a db_table_schema_t * in item->userdata
// for every table and relational field row. Subsequent callbacks retrieve it
// from the selected row in the parent column; no level enums, no index re-walking.

#include "formeditor.h"
#include "../../commctl/commctl.h"

#define FE_DBOBJ_LOG(...) axLog("[formeditor][dbobj] " __VA_ARGS__)

#define DBOBJ_COL_WIDTH 100

static window_t *g_db_objects_win = NULL;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static db_t *dbobj_db_at(int idx) {
  if (!g_app || idx < 0 || idx >= g_app->project.database_count)
    return NULL;
  return g_app->project.databases[idx];
}

static const db_schema_def_t *dbobj_schema_at(int db_idx) {
  db_t *db = dbobj_db_at(db_idx);
  return db ? (const db_schema_def_t *)send_db_message(db, dbGetSchema, 0, NULL) : NULL;
}

static const db_table_schema_t *dbobj_table_by_id(const db_schema_def_t *schema,
                                                   uint32_t table_id) {
  if (!schema || !table_id)
    return NULL;
  for (int i = 0; i < schema->table_count; i++) {
    if (schema->tables[i].table_id == table_id)
      return &schema->tables[i];
  }
  return NULL;
}

// Returns the table stored in the selected item of the parent column.
// Column 0 (databases) has no parent table; column 1 (tables) resolves from
// the selected database; column 2+ resolves from the selected field's relation.
static const db_table_schema_t *dbobj_parent_table(window_t *browser, int column) {
  if (column < 2)
    return NULL;
  window_t *parent_col = (window_t *)send_message(browser, CBM_GETCOLUMNWINDOW,
                                                   (uint32_t)(column - 1), NULL);
  if (!parent_col)
    return NULL;
  int row = (int)send_message(parent_col, RVM_GETSELECTION, 0, NULL);
  if (row < 0)
    return NULL;
  reportview_item_t item = {0};
  if (!send_message(parent_col, RVM_GETITEMDATA, (uint32_t)row, &item))
    return NULL;
  return (const db_table_schema_t *)item.userdata;
}

// ---------------------------------------------------------------------------
// Delegate callbacks
// ---------------------------------------------------------------------------

static int dbobj_number_of_rows(void *ctx, window_t *browser, int column) {
  (void)ctx;

  if (column == 0) {
    int n = g_app ? g_app->project.database_count : 0;
    return n > 0 ? n : 1;
  }

  if (column == 1) {
    int db_idx = (int)send_message(browser, CBM_GETSELECTION, 0, NULL);
    const db_schema_def_t *schema = dbobj_schema_at(db_idx);
    return (schema && schema->table_count > 0) ? schema->table_count : 1;
  }

  // column >= 2: field columns
  const db_table_schema_t *table = dbobj_parent_table(browser, column);
  return (table && table->field_count > 0) ? table->field_count : 1;
}

static bool dbobj_load_cell(void *ctx, window_t *browser, int column, int row,
                            reportview_item_t *item) {
  (void)ctx;
  if (!item)
    return false;

  item->color = get_sys_color(brTextNormal);

  // Column 0: databases
  if (column == 0) {
    db_t *db = dbobj_db_at(row);
    item->text = (db && db->name && db->name[0]) ? db->name : "Database";
    if (!db)
      item->text = "No databases loaded";
    item->userdata = 0;
    return true;
  }

  // Column 1: tables; store table pointer for child columns to read.
  if (column == 1) {
    int db_idx = (int)send_message(browser, CBM_GETSELECTION, 0, NULL);
    const db_schema_def_t *schema = dbobj_schema_at(db_idx);
    if (!schema || row >= schema->table_count) {
      item->text = "No tables";
      item->userdata = 0;
      return true;
    }
    const db_table_schema_t *table = &schema->tables[row];
    item->text = table->name ? table->name : "Table";
    item->userdata = (uintptr_t)table;
    return true;
  }

  // Column >= 2: fields; parent table is in the selected item of the previous column.
  const db_table_schema_t *table = dbobj_parent_table(browser, column);
  if (!table || row >= table->field_count) {
    item->text = "No fields";
    item->userdata = 0;
    return true;
  }

  const db_field_schema_t *field = &table->fields[row];

  // Use join name if available, otherwise field name
  item->text = field->name ? field->name : "Field";
  if (field->relation_table_id && table->joins) {
    for (int i = 0; i < table->join_count; i++) {
      if (table->joins[i].local_field_id == field->field_id && table->joins[i].name) {
        item->text = table->joins[i].name;
        break;
      }
    }
  }

  // Store the related table pointer so the next column can expand it
  if (field->relation_table_id) {
    int db_idx = (int)send_message(browser, CBM_GETSELECTION, 0, NULL);
    const db_schema_def_t *schema = dbobj_schema_at(db_idx);
    item->userdata = (uintptr_t)dbobj_table_by_id(schema, field->relation_table_id);
  } else {
    item->userdata = 0;
  }

  return true;
}

static bool dbobj_is_leaf(void *ctx, window_t *browser, int column, int row) {
  (void)ctx;

  if (column == 0)
    return dbobj_db_at(row) == NULL;

  if (column == 1) {
    int db_idx = (int)send_message(browser, CBM_GETSELECTION, 0, NULL);
    const db_schema_def_t *schema = dbobj_schema_at(db_idx);
    if (!schema || row >= schema->table_count)
      return true;
    return schema->tables[row].field_count <= 0;
  }

  // Field column: leaf when the field has no relation
  const db_table_schema_t *table = dbobj_parent_table(browser, column);
  if (!table || row >= table->field_count)
    return true;
  return table->fields[row].relation_table_id == 0;
}

static int dbobj_width_of_column(void *ctx, window_t *browser, int column) {
  (void)ctx;
  (void)browser;
  (void)column;
  return DBOBJ_COL_WIDTH;
}

static void dbobj_did_select(void *ctx, window_t *browser, int column, int row) {
  (void)ctx;
  (void)browser;

  if (column != 0)
    return;

  db_t *db = dbobj_db_at(row);
  FE_DBOBJ_LOG("selected database: row=%d db=%p", row, (void *)db);
  if (db)
    ui_set_database(db);
}

static bool dbobj_load_drag_payload(void *ctx, window_t *browser, int column, int row,
                                    const reportview_item_t *item,
                                    ui_drag_item_payload_t *payload) {
  (void)ctx;
  (void)item;
  if (!payload || row < 0)
    return false;

  memset(payload, 0, sizeof(*payload));

  if (column == 0) {
    db_t *db = dbobj_db_at(row);
    if (!db)
      return false;
    payload->item_type = UI_DRAG_ITEM_DATABASE;
    payload->item_id = (uint32_t)row;
    snprintf(payload->source_name, sizeof(payload->source_name), "%s", db->name ? db->name : "");
    return true;
  }

  if (column == 1) {
    int db_idx = (int)send_message(browser, CBM_GETSELECTION, 0, NULL);
    const db_schema_def_t *schema = dbobj_schema_at(db_idx);
    if (!schema || row >= schema->table_count)
      return false;
    db_t *db = dbobj_db_at(db_idx);
    if (!db)
      return false;
    const db_table_schema_t *table = &schema->tables[row];
    payload->item_type = UI_DRAG_ITEM_DATABASE_TABLE;
    payload->item_class = table->table_id;
    payload->item_id = table->table_id;
    snprintf(payload->source_name, sizeof(payload->source_name), "%s", db->name ? db->name : "");
    return true;
  }

  const db_table_schema_t *table = dbobj_parent_table(browser, column);
  if (!table || row >= table->field_count)
    return false;

  int db_idx = (int)send_message(browser, CBM_GETSELECTION, 0, NULL);
  db_t *db = dbobj_db_at(db_idx);
  if (!db)
    return false;
  const db_field_schema_t *field = &table->fields[row];
  payload->item_type = UI_DRAG_ITEM_DATABASE_FIELD;
  payload->item_class = table->table_id;
  payload->item_id = field->field_id;
  snprintf(payload->source_name, sizeof(payload->source_name), "%s", db->name ? db->name : "");
  return true;
}

static const column_browser_delegate_t g_dbobj_delegate = {
  .number_of_rows = dbobj_number_of_rows,
  .load_cell      = dbobj_load_cell,
  .is_leaf        = dbobj_is_leaf,
  .width_of_column = dbobj_width_of_column,
  .load_drag_payload = dbobj_load_drag_payload,
  .did_select     = dbobj_did_select,
};

// ---------------------------------------------------------------------------
// Window management
// ---------------------------------------------------------------------------

void formeditor_show_database_object_window(int db_index) {
  if (!g_app)
    return;

  db_t *db = dbobj_db_at(db_index);
  if (db)
    ui_set_database(db);

  if (!g_db_objects_win || !is_window(g_db_objects_win)) {
    g_db_objects_win = create_window("Databases", WINDOW_NOTRAYBUTTON | WINDOW_HSCROLL,
                                     MAKERECT(DOC_START_X + 20, DOC_START_Y + 20, 420, 260),
                                     NULL, win_column_browser, g_app->hinstance, NULL);
    if (!g_db_objects_win)
      return;
    show_window(g_db_objects_win, true);
  }

  send_message(g_db_objects_win, CBM_SETDELEGATE, 0, (void *)&g_dbobj_delegate);
  send_message(g_db_objects_win, CBM_SETMINCOLUMNWIDTH, DBOBJ_COL_WIDTH, NULL);
  send_message(g_db_objects_win, CBM_LOADCOLUMNZERO, 0, NULL);

  move_to_top(g_db_objects_win);
  invalidate_window(g_db_objects_win);
}

void formeditor_close_database_object_window(void) {
  if (g_db_objects_win && is_window(g_db_objects_win))
    destroy_window(g_db_objects_win);
  g_db_objects_win = NULL;
}
