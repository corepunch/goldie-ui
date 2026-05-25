// tableview.c — Database-backed table view control
//
// Automatically populates a reportview from database records using the Zero
// Wrapper Structs API. No manual population code needed.
//
// Creation parameters (lparam):
//   tableview_params_t (see commctl/commctl.h)
//
// Messages:
//   tvRefresh - Refresh from database (wparam=0, lparam=0)
//   tvSetFilter - Change filter (wparam=filter_field, lparam=filter_value)
//   tvSetColumnBinding - Change column binding (wparam=column, lparam=tableview_column_binding_t*)
//
// Example usage:
//   tableview_params_t params = {
//     .db = g_db,
//     .table_id = ID_DB_POSTS,
//     .filter_field = 0,
//     .filter_value = 0,
//     .field_ids = (const uint32_t[]){ ID_DB_POSTS_TITLE, ID_DB_AUTHORS_NAME, ID_DB_POSTS_LIKE_COUNT },
//     .column_titles = (const char *[]){"Title", "Author", "Likes"},
//     .column_widths = (const int[]){0, 80, 50},
//     .column_count = 3,
//   };
//   window_t *tv = create_window("", WINDOW_NOTITLE | WINDOW_VSCROLL,
//                                MAKERECT(0, 0, w, h), parent,
//                                win_tableview, &params);

#include "../ui.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
  database_t *db;
  int table_id;
  int filter_field;
  intptr_t filter_value;
  
  // Column metadata (copied from params)
  uint32_t *field_ids;
  char **column_titles;
  int *column_widths;
  int column_count;
} tableview_state_t;

// Forward declarations
static void tv_refresh(window_t *win, tableview_state_t *s);
static void tv_sync_columns(window_t *win, tableview_state_t *s);
static bool tv_set_column_title(tableview_state_t *s, int column, const char *title);

// ══════════════════════════════════════════════════════════════════════════
// Helper: Copy string array
// ══════════════════════════════════════════════════════════════════════════

static char **copy_string_array(const char **src, int count) {
  if (!src || count <= 0) return NULL;
  char **dst = calloc((size_t)(count + 1), sizeof(char *));
  if (!dst) return NULL;
  for (int i = 0; i < count; i++) {
    dst[i] = src[i] ? strdup(src[i]) : NULL;
  }
  dst[count] = NULL;
  return dst;
}

// ══════════════════════════════════════════════════════════════════════════
// Helper: Copy int array
// ══════════════════════════════════════════════════════════════════════════

static int *copy_int_array(const int *src, int count) {
  if (count <= 0) return NULL;
  int *dst = calloc((size_t)count, sizeof(int));
  if (!dst) return NULL;
  if (src) {
    memcpy(dst, src, (size_t)count * sizeof(int));
  }
  return dst;
}

// ══════════════════════════════════════════════════════════════════════════
// Helper: Copy uint32 array
// ══════════════════════════════════════════════════════════════════════════

static uint32_t *copy_u32_array(const uint32_t *src, int count) {
  if (!src || count <= 0) return NULL;
  uint32_t *dst = calloc((size_t)count, sizeof(uint32_t));
  if (!dst) return NULL;
  memcpy(dst, src, (size_t)count * sizeof(uint32_t));
  return dst;
}

// ══════════════════════════════════════════════════════════════════════════
// Helper: Free string array
// ══════════════════════════════════════════════════════════════════════════

static void free_string_array(char **arr) {
  if (!arr) return;
  for (int i = 0; arr[i]; i++)
    free(arr[i]);
  free(arr);
}

static bool tv_set_column_title(tableview_state_t *s, int column, const char *title) {
  if (!s || column < 0 || column >= s->column_count)
    return false;
  if (!s->column_titles) {
    s->column_titles = calloc((size_t)(s->column_count + 1), sizeof(char *));
    if (!s->column_titles)
      return false;
  }
  char *copy = strdup(title ? title : "");
  if (!copy)
    return false;
  free(s->column_titles[column]);
  s->column_titles[column] = copy;
  return true;
}

// ══════════════════════════════════════════════════════════════════════════
// Helper: Get field text from record via DDX-style field interrogation
// ══════════════════════════════════════════════════════════════════════════

static bool tv_get_field_text(tableview_state_t *s, void *record,
                                 uint32_t field_id, char *buf, size_t buf_sz) {
  if (!s || !s->db || !record || !buf || buf_sz == 0 || field_id == 0) return false;
  return db_get_schema_field_text(s->db, (uint32_t)s->table_id, record, field_id, buf, buf_sz);
}

static void tv_sync_columns(window_t *win, tableview_state_t *s) {
  if (!win || !s)
    return;

  int existing_cols = (int)send_message(win, RVM_GETCOLUMNCOUNT, 0, NULL);
  if (existing_cols == s->column_count)
    return;

  send_message(win, RVM_CLEARCOLUMNS, 0, NULL);
  for (int i = 0; i < s->column_count; i++) {
    reportview_column_t col = {
      .title = s->column_titles && s->column_titles[i] ? s->column_titles[i] : "",
      .width = (uint32_t)((s->column_widths && s->column_widths[i] > 0) ? s->column_widths[i] : 0),
    };
    send_message(win, RVM_ADDCOLUMN, 0, &col);
  }
}

// ══════════════════════════════════════════════════════════════════════════
// Refresh: Fetch records from database and populate reportview
// ══════════════════════════════════════════════════════════════════════════

static void tv_refresh(window_t *win, tableview_state_t *s) {
  if (!win || !s || !s->db) return;
  
  // Clear existing items
  send_message(win, RVM_CLEAR, 0, NULL);
  
  tv_sync_columns(win, s);
  
  // Fetch records from database (Zero Wrapper Structs API!)
  result_node_t *results = (result_node_t *)send_db_message(s->db, dbFetch,
    MAKEDWORD(s->table_id, s->filter_field), (void *)s->filter_value);
  
  if (!results) return;
  
  // Allocate cell buffers
  char (*cell_buf)[256] = calloc((size_t)s->column_count, sizeof(*cell_buf));
  if (!cell_buf) {
    free_result_list(results);
    return;
  }
  
  // Add each record as a row
  int row_idx = 0;
  for (result_node_t *n = results; n; n = (result_node_t *)n->next) {
    void *record = *(void **)n->data;
    if (!record) continue;
    
    // Extract field values for all columns
    for (int col = 0; col < s->column_count; col++) {
      if (!tv_get_field_text(s, record, s->field_ids[col],
                             cell_buf[col], sizeof(cell_buf[col]))) {
        cell_buf[col][0] = '\0';
      }
    }
    
    // Create row
    reportview_item_t item = {
      .text = cell_buf[0],
      .icon = -1,
      .color = get_sys_color(brTextNormal),
      .userdata = (uint32_t)row_idx,
      .subitem_count = (uint32_t)((s->column_count > 1) ? (s->column_count - 1) : 0),
    };
    
    for (int col = 1; col < s->column_count; col++) {
      item.subitems[col - 1] = cell_buf[col];
    }
    
    send_message(win, RVM_ADDITEM, 0, &item);
    row_idx++;
  }
  
  free(cell_buf);
  free_result_list(results);
}

// ══════════════════════════════════════════════════════════════════════════
// Window Procedure
// ══════════════════════════════════════════════════════════════════════════

lresult_t win_tableview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  tableview_state_t *s = (tableview_state_t *)win->userdata;
  
  switch (msg) {
    case evCreate: {
      if (!win_reportview(win, evCreate, 0, NULL)) {
        return false;
      }

      // Parse creation parameters from form_ctrl_def_t or raw tableview_params_t*
      tableview_params_t *params = NULL;
      if (lparam && (uintptr_t)lparam > 0x1000) {
        // Check if it's a form_ctrl_def_t* (has class_name field at offset 0)
        const form_ctrl_def_t *cd = (const form_ctrl_def_t *)lparam;
        if (cd->lparam) {
          params = (tableview_params_t *)cd->lparam;
        }
      } else {
        // Legacy: direct tableview_params_t* (for non-form creation)
        params = (tableview_params_t *)lparam;
      }
      
      if (!params || !params->field_ids || params->column_count <= 0) {
        // Keep the control alive as a plain reportview to avoid forwarding
        // messages into an uninitialized base state.
        return true;
      }
      
      // Allocate state
      s = calloc(1, sizeof(tableview_state_t));
      if (!s) {
        return true;
      }
      
      win->userdata = s;
      s->db = params->db;  // May be NULL - set later via evSetDatabase
      s->table_id = params->table_id;
      s->filter_field = params->filter_field;
      s->filter_value = params->filter_value;
      
      // Copy column metadata
      s->column_count = params->column_count;
      if (s->column_count <= 0) {
        free(s);
        win->userdata = NULL;
        return true;
      }
      
      s->field_ids = copy_u32_array(params->field_ids, s->column_count);
      s->column_titles = params->column_titles
        ? copy_string_array(params->column_titles, s->column_count)
        : NULL;
      s->column_widths = copy_int_array(params->column_widths, s->column_count);
      
      if (!s->field_ids || (params->column_titles && !s->column_titles)) {
        free(s->field_ids);
        free_string_array(s->column_titles);
        free(s->column_widths);
        free(s);
        win->userdata = NULL;
        return true;
      }
      
      // Setup as reportview
      send_message(win, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
      tv_sync_columns(win, s);
      
      // Initial refresh (only if db is set)
      if (s->db) {
        tv_refresh(win, s);
      }
      
      return true;
    }
    
    case evDestroy:
      if (s) {
        free(s->field_ids);
        free_string_array(s->column_titles);
        free(s->column_widths);
        free(s);
        win->userdata = NULL;
      }
      win_reportview(win, evDestroy, 0, NULL);
      return false;
    
    case tvRefresh:
      if (s) {
        tv_refresh(win, s);
      }
      return true;
    
    case evSetDatabase:
      if (s && lparam) {
        s->db = (database_t *)lparam;
        // Refresh from database
        if (s->db) {
          tv_refresh(win, s);
        }
      }
      return true;
    
    case tvSetFilter:
      if (s) {
        s->filter_field = (int)wparam;
        s->filter_value = (intptr_t)lparam;
        tv_refresh(win, s);
      }
      return true;

    case tvSetColumnBinding: {
      tableview_column_binding_t *binding = (tableview_column_binding_t *)lparam;
      uint32_t column = (uint32_t)wparam;
      if (!s || !binding || column >= (uint32_t)s->column_count || !binding->field_id)
        return false;
      s->field_ids[column] = binding->field_id;
      if (!tv_set_column_title(s, (int)column, binding->title))
        return false;
      send_message(win, RVM_CLEARCOLUMNS, 0, NULL);
      tv_refresh(win, s);
      return true;
    }

    // evArrange and evResize now inherited from reportview
    // (reportview evResize automatically recalculates column widths)
    
    default:
      return win_reportview(win, msg, wparam, lparam);
  }
}
