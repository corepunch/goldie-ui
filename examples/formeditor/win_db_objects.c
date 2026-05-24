// Database object window for FormEditor.
// Shows database objects using a horizontal set of single-column lists.

#include "formeditor.h"
#include "../../commctl/commctl.h"

#define FE_DBOBJ_LOG(...) axLog("[formeditor][dbobj] " __VA_ARGS__)

#define DBOBJ_COL_DB_W 120
#define DBOBJ_COL_TABLE_W 120

typedef struct {
  window_t *db_list;
  window_t *table_list;
  db_t *seed_db;
  int selected_db_idx;
} db_objects_state_t;

static window_t *g_db_objects_win = NULL;

static void dbobj_layout_columns(window_t *win, db_objects_state_t *st) {
  if (!win || !st)
    return;

  irect16_t cr = get_client_rect(win);
  if (cr.w < 1 || cr.h < 1)
    return;

  int x = -(int)win->hscroll.pos;
  int total_w = 0;

  if (st->db_list && is_window(st->db_list)) {
    int w = st->db_list->layout.layout_fixed_w > 0 ? st->db_list->layout.layout_fixed_w : DBOBJ_COL_DB_W;
    st->db_list->frame = R(x, 0, w, cr.h);
    x += w;
    total_w += w;
  }

  if (st->table_list && is_window(st->table_list)) {
    int w = st->table_list->layout.layout_fixed_w > 0 ? st->table_list->layout.layout_fixed_w : DBOBJ_COL_TABLE_W;
    st->table_list->frame = R(x, 0, w, cr.h);
    x += w;
    total_w += w;
  }

  if (total_w < cr.w)
    total_w = cr.w;

  int max_pos = total_w - cr.w;
  if (max_pos < 0)
    max_pos = 0;
  if ((int)win->hscroll.pos > max_pos)
    win->hscroll.pos = (uint32_t)max_pos;

  scroll_info_t si;
  si.fMask = SIF_ALL;
  si.nMin = 0;
  si.nMax = total_w;
  si.nPage = (uint32_t)cr.w;
  si.nPos = (int)win->hscroll.pos;
  set_scroll_info(win, SB_HORZ, &si, false);

  invalidate_window(win);
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

static void dbobj_rebuild_table_list(db_objects_state_t *st) {
  if (!st || !st->table_list)
    return;

  send_message(st->table_list, RVM_CLEAR, 0, NULL);

  const db_schema_def_t *schema = db_schema_at(st->selected_db_idx);
  if (!schema) {
    dbobj_add_row(st->table_list, "No tables", 0);
    return;
  }

  FE_DBOBJ_LOG("rebuild table list: db_idx=%d table_count=%d", st->selected_db_idx, schema->table_count);
  for (int i = 0; i < schema->table_count; i++) {
    const char *name = schema->tables[i].name ? schema->tables[i].name : "Table";
    dbobj_add_row(st->table_list, name, (uint32_t)i);
  }
}

static void dbobj_rebuild_lists(db_objects_state_t *st) {
  if (!st || !st->db_list)
    return;

  send_message(st->db_list, RVM_CLEAR, 0, NULL);

  int count = g_app ? g_app->project.database_count : 0;
  FE_DBOBJ_LOG("rebuild db list: database_count=%d", count);
  if (count <= 0) {
    dbobj_add_row(st->db_list, "DB list alive (no databases loaded)", 0);
    st->selected_db_idx = -1;
    dbobj_rebuild_table_list(st);
    return;
  }

  for (int i = 0; i < count; i++) {
    db_t *db = db_project_at(i);
    const char *name = (db && db->name && db->name[0]) ? db->name : "Database";
    dbobj_add_row(st->db_list, name, (uint32_t)i);
  }

  if (st->selected_db_idx < 0 || st->selected_db_idx >= count)
    st->selected_db_idx = 0;
  send_message(st->db_list, RVM_SETSELECTION, (uint32_t)st->selected_db_idx, NULL);
  dbobj_rebuild_table_list(st);
}

static lresult_t win_db_objects_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  db_objects_state_t *st = (db_objects_state_t *)win->userdata;
  (void)wparam;
  switch (msg) {
    case evCreate: {
      st = allocate_window_data(win, sizeof(db_objects_state_t));
      if (!st)
        return false;
      st->seed_db = (db_t *)lparam;
      st->selected_db_idx = -1;

      irect16_t cr = get_client_rect(win);
      st->db_list = create_window("", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
                                  MAKERECT(0, 0, DBOBJ_COL_DB_W, cr.h), win, win_reportview, 0, NULL);
      if (!st->db_list)
        return false;
      st->db_list->layout.layout_fixed_w = DBOBJ_COL_DB_W;

      send_message(st->db_list, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
      send_message(st->db_list, RVM_SETCOLUMNTITLESVISIBLE, 1, NULL);
      {
        reportview_column_t c0 = { "Database", DBOBJ_COL_DB_W };
        send_message(st->db_list, RVM_ADDCOLUMN, 0, &c0);
      }

      st->table_list = create_window("", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
                                     MAKERECT(DBOBJ_COL_DB_W, 0, DBOBJ_COL_TABLE_W, cr.h), win, win_reportview, 0, NULL);
      if (!st->table_list)
        return false;
      st->table_list->layout.layout_fixed_w = DBOBJ_COL_TABLE_W;

      send_message(st->table_list, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
      send_message(st->table_list, RVM_SETCOLUMNTITLESVISIBLE, 1, NULL);
      {
        reportview_column_t c0 = { "Table", DBOBJ_COL_TABLE_W };
        send_message(st->table_list, RVM_ADDCOLUMN, 0, &c0);
      }

      FE_DBOBJ_LOG("window create: seed_db=%p", (void *)st->seed_db);

      if (st->seed_db && g_app) {
        for (int i = 0; i < g_app->project.database_count; i++) {
          if (g_app->project.databases[i] == st->seed_db) {
            st->selected_db_idx = i;
            break;
          }
        }
      }

      dbobj_rebuild_lists(st);
      dbobj_layout_columns(win, st);
      return true;
    }

    case evCommand:
      if (st && lparam == st->db_list && HIWORD(wparam) == RVN_SELCHANGE) {
        int row = (int)LOWORD(wparam);
        db_t *db = db_project_at(row);
        FE_DBOBJ_LOG("db list selection changed: row=%d db=%p", row, (void *)db);
        if (db) {
          st->selected_db_idx = row;
          ui_set_database(db);
          dbobj_rebuild_table_list(st);
        }
        return true;
      }
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
  db_t *seed_db = NULL;
  if (!g_app)
    return;

  if (db_index >= 0 && db_index < g_app->project.database_count) {
    db_t *db = g_app->project.databases[db_index];
    if (db) {
      seed_db = db;
      ui_set_database(db);
    }
  }

  if (!g_db_objects_win || !is_window(g_db_objects_win)) {
    g_db_objects_win = create_window("Databases", WINDOW_NOTRAYBUTTON | WINDOW_HSCROLL,
                                     MAKERECT(DOC_START_X + 20, DOC_START_Y + 20, 420, 260),
                                     NULL, win_db_objects_proc, g_app->hinstance, seed_db);
    if (!g_db_objects_win)
      return;
    show_window(g_db_objects_win, true);
  }

  snprintf(g_db_objects_win->title, sizeof(g_db_objects_win->title), "Databases");
  db_objects_state_t *st = (db_objects_state_t *)g_db_objects_win->userdata;
  if (st) {
    dbobj_rebuild_lists(st);
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
