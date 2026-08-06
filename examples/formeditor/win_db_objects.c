// Database object browser opened by double-clicking a database in Objects:
//   [Tables] -> [Fields and relationships] -> [Related fields ...]

#include "formeditor.h"
#include "../../commctl/commctl.h"

#define DB_DRAG_THRESHOLD 2

typedef struct {
  window_t *browser;
  int database_index;
} db_source_t;

typedef struct {
  bool pending, dragging;
  window_t *source;
  ipoint16_t start;
  fe_database_field_ref_t field;
} db_drag_t;

typedef struct {
  window_t *browser;
  db_source_t source;
  db_drag_t drag;
} db_browser_state_t;

static window_t *g_db_objects_win;

static database_t *db_at(int index) {
  return g_app && index >= 0 && index < g_app->project.database_count
      ? g_app->project.databases[index] : NULL;
}

static const db_schema_def_t *db_schema(db_source_t *source) {
  database_t *db = source ? db_at(source->database_index) : NULL;
  return db ? (const db_schema_def_t *)send_db_message(db, dbGetSchema, 0, NULL) : NULL;
}

static const db_table_schema_t *table_named(const db_schema_def_t *schema, const char *name) {
  if (!schema || !name || !*name) return NULL;
  for (int i = 0; i < schema->table_count; i++)
    if (schema->tables[i].name && strcmp(schema->tables[i].name, name) == 0)
      return &schema->tables[i];
  return NULL;
}

static const db_table_schema_t *table_for_column(db_source_t *source, int column) {
  const db_schema_def_t *schema = db_schema(source);
  if (!schema || !source->browser || column < 1) return NULL;
  int row = (int)send_message(source->browser, cbGetSelection, 0, NULL);
  if (row < 0 || row >= schema->table_count) return NULL;
  const db_table_schema_t *table = &schema->tables[row];
  for (int c = 1; c < column; c++) {
    row = (int)send_message(source->browser, cbGetSelection, (uint32_t)c, NULL);
    const char *related = NULL;
    if (row >= 0 && row < table->field_count)
      related = table->fields[row].relation_table;
    else if (row >= table->field_count && row < table->field_count + table->join_count)
      related = table->joins[row - table->field_count].foreign_table;
    table = table_named(schema, related);
    if (!table) return NULL;
  }
  return table;
}

static int child_count(void *ctx, int column, int parent) {
  db_source_t *source = (db_source_t *)ctx;
  const db_schema_def_t *schema = db_schema(source);
  (void)parent;
  if (!schema) return 0;
  if (column == 0) return schema->table_count;
  const db_table_schema_t *table = table_for_column(source, column);
  return table ? table->field_count + table->join_count : 0;
}

static const char *child_title(void *ctx, int column, int parent, int row) {
  static char title[192];
  db_source_t *source = (db_source_t *)ctx;
  const db_schema_def_t *schema = db_schema(source);
  (void)parent;
  if (!schema) return NULL;
  if (column == 0)
    return row >= 0 && row < schema->table_count ? schema->tables[row].name : NULL;
  const db_table_schema_t *table = table_for_column(source, column);
  if (!table) return NULL;
  if (row >= 0 && row < table->field_count) {
    const db_field_schema_t *field = &table->fields[row];
    if (!field->relation_table || !field->relation_table[0]) return field->name;
    snprintf(title, sizeof(title), "%s -> %s", field->name, field->relation_table);
    return title;
  }
  int join = row - table->field_count;
  if (join < 0 || join >= table->join_count) return NULL;
  snprintf(title, sizeof(title), "%s -> %s", table->joins[join].name,
           table->joins[join].foreign_table);
  return title;
}

static bool is_leaf(void *ctx, int column, int row) {
  db_source_t *source = (db_source_t *)ctx;
  if (column == 0) return false;
  const db_table_schema_t *table = table_for_column(source, column);
  if (!table) return true;
  if (row >= table->field_count) return row >= table->field_count + table->join_count;
  const char *related = table->fields[row].relation_table;
  return !related || !*related;
}

static bool field_at(db_source_t *source, int column, int row,
                     fe_database_field_ref_t *out) {
  database_t *db = source ? db_at(source->database_index) : NULL;
  const db_table_schema_t *table = table_for_column(source, column);
  if (!db || !table || !out || row < 0 || row >= table->field_count) return false;
  snprintf(out->database, sizeof(out->database), "%s", db->name ? db->name : "");
  snprintf(out->table, sizeof(out->table), "%s", table->name ? table->name : "");
  snprintf(out->field, sizeof(out->field), "%s",
           table->fields[row].name ? table->fields[row].name : "");
  return out->database[0] && out->table[0] && out->field[0];
}

static ipoint16_t point_to_screen(window_t *win, uint32_t packed) {
  return (ipoint16_t){
    (int16_t)(window_screen_x(win) + (int16_t)LOWORD(packed) - win->hscroll.pos),
    (int16_t)(window_screen_y(win) + (int16_t)HIWORD(packed) - win->vscroll.pos),
  };
}

static bool handle_drag(db_browser_state_t *st, parent_notify_t *pn) {
  if (!st || !pn || !pn->child || pn->child->parent != st->browser) return false;
  int column = pn->child->id - 1000;
  switch (pn->child_msg) {
    case evLeftButtonDown: {
      int row = (int)send_message(pn->child, RVM_HITTEST, pn->child_wparam, NULL);
      if (column < 1 || !field_at(&st->source, column, row, &st->drag.field)) return false;
      st->drag.pending = true; st->drag.dragging = false; st->drag.source = pn->child;
      st->drag.start = (ipoint16_t){(int16_t)LOWORD(pn->child_wparam),
                                   (int16_t)HIWORD(pn->child_wparam)};
      set_capture(pn->child);
      return false;
    }
    case evMouseMove: {
      if (!st->drag.pending || st->drag.source != pn->child) return false;
      int dx = (int16_t)LOWORD(pn->child_wparam) - st->drag.start.x;
      int dy = (int16_t)HIWORD(pn->child_wparam) - st->drag.start.y;
      if (!st->drag.dragging && (abs(dx) >= DB_DRAG_THRESHOLD || abs(dy) >= DB_DRAG_THRESHOLD))
        st->drag.dragging = true;
      return false;
    }
    case evLeftButtonUp:
      if (!st->drag.pending || st->drag.source != pn->child) return false;
      if (st->drag.dragging && g_app && g_app->active_form) {
        ipoint16_t screen = point_to_screen(pn->child, pn->child_wparam);
        canvas_bind_database_field(g_app->active_form, &st->drag.field, screen.x, screen.y);
      }
      st->drag = (db_drag_t){0};
      set_capture(NULL);
      return false;
    default: return false;
  }
}

static result_t db_browser_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  db_browser_state_t *st = (db_browser_state_t *)win->userdata;
  switch (msg) {
    case evCreate: {
      st = allocate_window_data(win, sizeof(*st));
      if (!st) return false;
      irect16_t cr = get_client_rect(win);
      st->browser = create_window("", WINDOW_NOTITLE | WINDOW_NOFILL,
          MAKERECT(0, 0, cr.w, cr.h), win, win_column_browser, 0, NULL);
      if (!st->browser) return false;
      st->source.browser = st->browser;
      st->source.database_index = -1;
      column_browser_datasource_t ds = {
        .get_child_count = child_count, .get_child_title = child_title,
        .is_leaf = is_leaf, .userdata = &st->source,
      };
      send_message(st->browser, cbSetDataSource, 0, &ds);
      return true;
    }
    case evParentNotify: return handle_drag(st, (parent_notify_t *)lparam);
    case evResize:
      if (st && st->browser) {
        irect16_t cr = get_client_rect(win);
        resize_window(st->browser, cr.w, cr.h);
      }
      return true;
    case evDestroy:
      if (g_db_objects_win == win) g_db_objects_win = NULL;
      return false;
    case evCommand: return st && lparam == st->browser;
    default: return false;
  }
}

void formeditor_show_database_object_window(int database_index) {
  database_t *db = db_at(database_index);
  if (!g_app || !db) return;
  ui_set_database(db);
  if (!g_db_objects_win || !is_window(g_db_objects_win)) {
    g_db_objects_win = create_window("Databases", WINDOW_NOTRAYBUTTON | WINDOW_HSCROLL,
        MAKERECT(DOC_START_X + 20, DOC_START_Y + 20, 420, 260), NULL,
        db_browser_proc, g_app->hinstance, NULL);
    if (!g_db_objects_win) return;
    show_window(g_db_objects_win, true);
  }
  db_browser_state_t *st = (db_browser_state_t *)g_db_objects_win->userdata;
  if (st) {
    st->source.database_index = database_index;
    send_message(st->browser, cbRefresh, 0, NULL);
  }
  move_to_top(g_db_objects_win);
  invalidate_window(g_db_objects_win);
}

void formeditor_close_database_object_window(void) {
  if (g_db_objects_win && is_window(g_db_objects_win)) destroy_window(g_db_objects_win);
  g_db_objects_win = NULL;
}
