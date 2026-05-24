// Database object window for FormEditor.
// Shows database objects using a horizontal set of single-column lists.

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
  DBOBJ_LEVEL_COUNT
};

typedef struct {
  window_t *columns[DBOBJ_LEVEL_COUNT];
  int selected_db_idx;
  int selected_table_idx;
} db_objects_state_t;

static window_t *g_db_objects_win = NULL;

static int dbobj_column_width(int level) {
  switch (level) {
    case DBOBJ_LEVEL_DATABASE: return DBOBJ_COL_DB_W;
    case DBOBJ_LEVEL_TABLE:    return DBOBJ_COL_TABLE_W;
    case DBOBJ_LEVEL_FIELD:    return DBOBJ_COL_FIELD_W;
    default:                   return DBOBJ_COL_TABLE_W;
  }
}

static const char *dbobj_column_title(int level) {
  switch (level) {
    case DBOBJ_LEVEL_DATABASE: return "Database";
    case DBOBJ_LEVEL_TABLE:    return "Table";
    case DBOBJ_LEVEL_FIELD:    return "Column";
    default:                   return "";
  }
}

static void dbobj_layout_columns(window_t *win, db_objects_state_t *st) {
  if (!win || !st)
    return;

  irect16_t cr = get_client_rect(win);
  if (cr.w < 1 || cr.h < 1)
    return;

  int total_w = 0;
  for (int level = 0; level < DBOBJ_LEVEL_COUNT; level++) {
    window_t *col = st->columns[level];
    if (col) {
      int w = col->layout.layout_fixed_w > 0 ? col->layout.layout_fixed_w : dbobj_column_width(level);
      total_w += w;
    }
  }

  if (total_w < cr.w)
    total_w = cr.w;

  int max_pos = total_w - cr.w;
  if (max_pos < 0)
    max_pos = 0;
  if ((int)win->hscroll.pos > max_pos)
    win->hscroll.pos = (uint32_t)max_pos;

  int x = -(int)win->hscroll.pos;
  for (int level = 0; level < DBOBJ_LEVEL_COUNT; level++) {
    window_t *col = st->columns[level];
    if (col) {
      int w = col->layout.layout_fixed_w > 0 ? col->layout.layout_fixed_w : dbobj_column_width(level);
      col->frame = R(x, 0, w, cr.h);
      x += w;
    }
  }

  scroll_info_t si;
  si.fMask = SIF_ALL;
  si.nMin = 0;
  si.nMax = total_w;
  si.nPage = (uint32_t)cr.w;
  si.nPos = (int)win->hscroll.pos;
  set_scroll_info(win, SB_HORZ, &si, false);

  invalidate_window(win);
}

static window_t *dbobj_ensure_column(window_t *win, db_objects_state_t *st, int level) {
  if (!win || !st || level < 0 || level >= DBOBJ_LEVEL_COUNT)
    return NULL;

  window_t *col = st->columns[level];
  if (col)
    return col;

  int w = dbobj_column_width(level);
  col = create_window("", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
                      MAKERECT(0, 0, w, get_client_rect(win).h),
                      win, win_reportview, 0, NULL);
  if (!col)
    return NULL;
  col->layout.layout_fixed_w = w;

  send_message(col, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
  send_message(col, RVM_SETCOLUMNTITLESVISIBLE, 1, NULL);
  reportview_column_t c0 = { dbobj_column_title(level), w };
  send_message(col, RVM_ADDCOLUMN, 0, &c0);

  st->columns[level] = col;
  return col;
}

static void dbobj_remove_columns_from(db_objects_state_t *st, int first_level) {
  if (!st)
    return;
  if (first_level < 0)
    first_level = 0;
  for (int level = DBOBJ_LEVEL_COUNT - 1; level >= first_level; level--) {
    window_t *col = st->columns[level];
    st->columns[level] = NULL;
    if (col)
      destroy_window(col);
  }
}

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

static void dbobj_add_row(window_t *list, const char *text, uint32_t userdata) {
  if (!list)
    return;
  reportview_item_t item = {
    .text = text ? text : "",
    .color = get_sys_color(brTextNormal),
    .userdata = userdata,
    .subitem_count = 0,
  };
  send_message(list, RVM_ADDITEM, 0, &item);
  FE_DBOBJ_LOG("add row: '%s' (userdata=%u)", item.text, userdata);
}

static void dbobj_rebuild_field_list(window_t *win, db_objects_state_t *st) {
  if (!win || !st)
    return;

  window_t *field_list = dbobj_ensure_column(win, st, DBOBJ_LEVEL_FIELD);
  if (!field_list)
    return;

  send_message(field_list, RVM_CLEAR, 0, NULL);

  const db_schema_def_t *schema = db_schema_at(st->selected_db_idx);
  if (!schema || st->selected_table_idx < 0 || st->selected_table_idx >= schema->table_count) {
    dbobj_add_row(field_list, "No columns", 0);
    return;
  }

  const db_table_schema_t *table = &schema->tables[st->selected_table_idx];
  FE_DBOBJ_LOG("rebuild field list: db_idx=%d table_idx=%d field_count=%d",
               st->selected_db_idx, st->selected_table_idx, table->field_count);
  if (!table->fields || table->field_count <= 0) {
    dbobj_add_row(field_list, "No columns", 0);
    return;
  }

  for (int i = 0; i < table->field_count; i++) {
    const char *name = table->fields[i].name ? table->fields[i].name : "Column";
    dbobj_add_row(field_list, name, (uint32_t)i);
  }
}

static void dbobj_rebuild_table_list(window_t *win, db_objects_state_t *st) {
  if (!win || !st)
    return;

  dbobj_remove_columns_from(st, DBOBJ_LEVEL_FIELD);

  window_t *table_list = dbobj_ensure_column(win, st, DBOBJ_LEVEL_TABLE);
  if (!table_list)
    return;

  send_message(table_list, RVM_CLEAR, 0, NULL);

  const db_schema_def_t *schema = db_schema_at(st->selected_db_idx);
  if (!schema) {
    dbobj_add_row(table_list, "No tables", 0);
    st->selected_table_idx = -1;
    return;
  }

  FE_DBOBJ_LOG("rebuild table list: db_idx=%d table_count=%d", st->selected_db_idx, schema->table_count);
  if (!schema->tables || schema->table_count <= 0) {
    dbobj_add_row(table_list, "No tables", 0);
    st->selected_table_idx = -1;
    return;
  }

  for (int i = 0; i < schema->table_count; i++) {
    const char *name = schema->tables[i].name ? schema->tables[i].name : "Table";
    dbobj_add_row(table_list, name, (uint32_t)i);
  }

  if (st->selected_table_idx < 0 || st->selected_table_idx >= schema->table_count)
    st->selected_table_idx = -1;
  if (st->selected_table_idx >= 0) {
    send_message(table_list, RVM_SETSELECTION, (uint32_t)st->selected_table_idx, NULL);
    dbobj_rebuild_field_list(win, st);
  }
}

static void dbobj_rebuild_lists(window_t *win, db_objects_state_t *st) {
  if (!win || !st)
    return;

  dbobj_remove_columns_from(st, DBOBJ_LEVEL_TABLE);

  window_t *db_list = dbobj_ensure_column(win, st, DBOBJ_LEVEL_DATABASE);
  if (!db_list)
    return;

  send_message(db_list, RVM_CLEAR, 0, NULL);

  int count = g_app ? g_app->project.database_count : 0;
  FE_DBOBJ_LOG("rebuild db list: database_count=%d", count);
  if (count <= 0) {
    dbobj_add_row(db_list, "DB list alive (no databases loaded)", 0);
    st->selected_db_idx = -1;
    st->selected_table_idx = -1;
    return;
  }

  for (int i = 0; i < count; i++) {
    db_t *db = db_project_at(i);
    const char *name = (db && db->name && db->name[0]) ? db->name : "Database";
    dbobj_add_row(db_list, name, (uint32_t)i);
  }

  if (st->selected_db_idx < 0 || st->selected_db_idx >= count) {
    st->selected_db_idx = -1;
    st->selected_table_idx = -1;
    return;
  }

  send_message(db_list, RVM_SETSELECTION, (uint32_t)st->selected_db_idx, NULL);
  dbobj_rebuild_table_list(win, st);
}

static lresult_t win_db_objects_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  db_objects_state_t *st = (db_objects_state_t *)win->userdata;
  (void)wparam;
  switch (msg) {
    case evCreate: {
      st = allocate_window_data(win, sizeof(db_objects_state_t));
      if (!st)
        return false;
      st->selected_db_idx = -1;
      st->selected_table_idx = -1;

      if (!dbobj_ensure_column(win, st, DBOBJ_LEVEL_DATABASE))
        return false;

      dbobj_rebuild_lists(win, st);
      dbobj_layout_columns(win, st);
      return true;
    }

    case evCommand:
      if (st && lparam == st->columns[DBOBJ_LEVEL_DATABASE] && HIWORD(wparam) == RVN_SELCHANGE) {
        int row = (int)LOWORD(wparam);
        db_t *db = db_project_at(row);
        FE_DBOBJ_LOG("db list selection changed: row=%d db=%p", row, (void *)db);
        if (db) {
          st->selected_db_idx = row;
          st->selected_table_idx = -1;
          ui_set_database(db);
          dbobj_rebuild_table_list(win, st);
          dbobj_layout_columns(win, st);
        }
        return true;
      }
      if (st && lparam == st->columns[DBOBJ_LEVEL_TABLE] && HIWORD(wparam) == RVN_SELCHANGE) {
        int row = (int)LOWORD(wparam);
        const db_schema_def_t *schema = db_schema_at(st->selected_db_idx);
        FE_DBOBJ_LOG("table list selection changed: row=%d", row);
        if (schema && row >= 0 && row < schema->table_count) {
          st->selected_table_idx = row;
          dbobj_rebuild_field_list(win, st);
          dbobj_layout_columns(win, st);
        }
        return true;
      }
      if (st && lparam == st->columns[DBOBJ_LEVEL_FIELD] && HIWORD(wparam) == RVN_SELCHANGE)
        return true;
      return false;

    case evResize:
      dbobj_layout_columns(win, st);
      return true;

    case evHScroll:
      win->hscroll.pos = (uint32_t)wparam;
      dbobj_layout_columns(win, st);
      return true;

    case evDestroy:
      if (g_db_objects_win == win)
        g_db_objects_win = NULL;
      return false;

    default:
      return default_winproc(win, msg, wparam, lparam);
  }
}

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
                                     NULL, win_db_objects_proc, g_app->hinstance, NULL);
    if (!g_db_objects_win)
      return;
    show_window(g_db_objects_win, true);
  }

  snprintf(g_db_objects_win->title, sizeof(g_db_objects_win->title), "Databases");
  db_objects_state_t *st = (db_objects_state_t *)g_db_objects_win->userdata;
  if (st) {
    st->selected_db_idx = -1;
    st->selected_table_idx = -1;
    dbobj_rebuild_lists(g_db_objects_win, st);
    dbobj_layout_columns(g_db_objects_win, st);
  }

  move_to_top(g_db_objects_win);
  invalidate_window(g_db_objects_win);
}

void formeditor_close_database_object_window(void) {
  if (g_db_objects_win && is_window(g_db_objects_win))
    destroy_window(g_db_objects_win);
  g_db_objects_win = NULL;
}
