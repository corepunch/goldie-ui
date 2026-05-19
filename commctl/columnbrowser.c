// NSBrowser-style multi-column hierarchical navigation control
// Analogous to NeXTSTEP's NSBrowser - left-to-right drill-down navigation

#include "../ui.h"
#include "commctl.h"
#include "../platform/platform.h"
#include <string.h>
#include <stdlib.h>

#define COLUMN_WIDTH 150
#define COLUMN_SPACING 0

// Column state is internal to this module

typedef struct {
  int *items;        // Item indices selected in previous columns
  int count;         // Number of items in path
  int selected_idx;  // Currently selected item in this column (-1 = none)
} column_state_t;

typedef struct {
  column_browser_datasource_t datasource;
  column_state_t *columns;    // Dynamic array of column states
  int column_count;           // Number of allocated columns
  int visible_columns;        // Number of columns currently visible
  window_t **column_lists;    // Reportview windows for each column
  int hscroll_pos;            // Horizontal scroll position
} column_browser_state_t;

static void cb_free_state(column_browser_state_t *cbs) {
  if (!cbs) return;
  
  // Destroy all column list windows
  for (int i = 0; i < cbs->column_count; i++) {
    if (cbs->column_lists && cbs->column_lists[i])
      destroy_window(cbs->column_lists[i]);
    if (cbs->columns && cbs->columns[i].items)
      free(cbs->columns[i].items);
  }
  
  free(cbs->column_lists);
  free(cbs->columns);
}

static int cb_get_parent_idx(column_browser_state_t *cbs, int column) {
  if (column <= 0 || !cbs->columns) return -1;
  return cbs->columns[column - 1].selected_idx;
}

static void cb_populate_column(column_browser_state_t *cbs, int column) {
  if (!cbs || column < 0 || column >= cbs->column_count) return;
  if (!cbs->datasource.get_child_count || !cbs->datasource.get_child_title) return;
  
  window_t *list = cbs->column_lists[column];
  if (!list) return;
  
  // Clear existing items
  send_message(list, RVM_CLEAR, 0, NULL);
  
  // Get parent index from previous column
  int parent_idx = cb_get_parent_idx(cbs, column);
  
  // Populate with children
  int child_count = cbs->datasource.get_child_count(cbs->datasource.userdata, column, parent_idx);
  for (int i = 0; i < child_count; i++) {
    const char *title = cbs->datasource.get_child_title(cbs->datasource.userdata, column, parent_idx, i);
    if (!title) continue;
    
    reportview_item_t item = {
      .text = title,
      .color = get_sys_color(brTextNormal),
      .userdata = (uint32_t)i,
      .subitems = { NULL },
      .subitem_count = 0,
    };
    send_message(list, RVM_ADDITEM, 0, &item);
  }
  
  cbs->columns[column].selected_idx = -1;
}

static void cb_ensure_column_capacity(column_browser_state_t *cbs, int needed) {
  if (!cbs) return;
  
  if (needed <= cbs->column_count) return;
  
  int new_count = (needed < 8) ? 8 : needed;
  
  cbs->columns = realloc(cbs->columns, new_count * sizeof(column_state_t));
  cbs->column_lists = realloc(cbs->column_lists, new_count * sizeof(window_t *));
  
  // Initialize new slots
  for (int i = cbs->column_count; i < new_count; i++) {
    cbs->columns[i].items = NULL;
    cbs->columns[i].count = 0;
    cbs->columns[i].selected_idx = -1;
    cbs->column_lists[i] = NULL;
  }
  
  cbs->column_count = new_count;
}

static void cb_create_column_list(window_t *win, column_browser_state_t *cbs, int column) {
  if (!win || !cbs) return;
  
  cb_ensure_column_capacity(cbs, column + 1);
  
  if (cbs->column_lists[column]) return;  // Already exists
  
  irect16_t cr = get_client_rect(win);
  int x = column * (COLUMN_WIDTH + COLUMN_SPACING) - cbs->hscroll_pos;
  
  window_t *list = create_window("",
    WINDOW_NOTITLE | WINDOW_VSCROLL | WINDOW_NORESIZE,
    MAKERECT(x, 0, COLUMN_WIDTH, cr.h),
    win, win_reportview, 0, NULL);
  
  if (!list) return;
  
  list->id = 1000 + column;  // Unique ID per column
  list->userdata = (void *)(intptr_t)column;
  
  cbs->column_lists[column] = list;
  cb_populate_column(cbs, column);
}

static void cb_selection_changed(window_t *win, column_browser_state_t *cbs, int column, int idx) {
  if (!cbs || column < 0 || column >= cbs->column_count) return;
  
  cbs->columns[column].selected_idx = idx;
  
  // Destroy columns to the right
  for (int i = column + 1; i < cbs->visible_columns; i++) {
    if (cbs->column_lists[i]) {
      destroy_window(cbs->column_lists[i]);
      cbs->column_lists[i] = NULL;
    }
  }
  
  // If selection is not a leaf, create next column
  if (idx >= 0 && cbs->datasource.is_leaf) {
    if (!cbs->datasource.is_leaf(cbs->datasource.userdata, column, idx)) {
      cb_create_column_list(win, cbs, column + 1);
      cbs->visible_columns = column + 2;
      
      // Update horizontal scroll if needed
      irect16_t cr = get_client_rect(win);
      int total_width = cbs->visible_columns * (COLUMN_WIDTH + COLUMN_SPACING);
      if (total_width > cr.w) {
        // Scroll to make new column visible
        int target_x = (column + 1) * (COLUMN_WIDTH + COLUMN_SPACING) - cr.w + COLUMN_WIDTH;
        if (target_x > cbs->hscroll_pos) {
          cbs->hscroll_pos = target_x;
          // Reposition all columns
          for (int j = 0; j < cbs->visible_columns; j++) {
            if (cbs->column_lists[j]) {
              int col_x = j * (COLUMN_WIDTH + COLUMN_SPACING) - cbs->hscroll_pos;
              move_window(cbs->column_lists[j], col_x, 0);
            }
          }
        }
      }
      
      invalidate_window(win);
    } else {
      cbs->visible_columns = column + 1;
    }
  } else {
    cbs->visible_columns = column + 1;
  }
  
  // Notify parent of selection change
  send_message(win->parent, evCommand, MAKEDWORD(win->id, 0), win);
}

static result_t column_list_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  window_t *parent = win->parent;
  if (!parent) return false;
  
  column_browser_state_t *cbs = (column_browser_state_t *)parent->userdata;
  if (!cbs) return false;
  
  int column = (int)(intptr_t)win->userdata;
  
  switch (msg) {
    case evCommand: {
      window_t *src = (window_t *)lparam;
      if (src && src->proc == win_reportview) {
        // Selection changed in reportview
        int idx = (int)send_message(src, RVM_GETSELECTION, 0, NULL);
        cb_selection_changed(parent, cbs, column, idx);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

result_t win_column_browser(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  column_browser_state_t *cbs = (column_browser_state_t *)win->userdata;
  
  switch (msg) {
    case evCreate: {
      cbs = calloc(1, sizeof(column_browser_state_t));
      if (!cbs) return false;
      
      win->userdata = cbs;
      
      // Start with first column
      cb_create_column_list(win, cbs, 0);
      cbs->visible_columns = 1;
      
      return true;
    }
    
    case evDestroy:
      if (cbs) {
        cb_free_state(cbs);
        free(cbs);
        win->userdata = NULL;
      }
      return true;
    
    case evPaint:
      // Columns draw themselves
      return true;
    
    case cbSetDataSource: {
      if (!cbs || !lparam) return false;
      
      column_browser_datasource_t *ds = (column_browser_datasource_t *)lparam;
      cbs->datasource = *ds;
      
      // Rebuild first column
      if (cbs->column_lists[0])
        cb_populate_column(cbs, 0);
      
      invalidate_window(win);
      return true;
    }
    
    case cbRefresh: {
      if (!cbs) return false;
      
      // Repopulate all visible columns
      for (int i = 0; i < cbs->visible_columns; i++) {
        if (cbs->column_lists[i])
          cb_populate_column(cbs, i);
      }
      
      invalidate_window(win);
      return true;
    }
    
    case cbGetSelection: {
      if (!cbs) return -1;
      int column = (int)wparam;
      if (column < 0 || column >= cbs->column_count) return -1;
      return cbs->columns[column].selected_idx;
    }
    
    case cbGetColumnCount:
      return cbs ? cbs->visible_columns : 0;
    
    case evCommand: {
      // Forward from column list
      window_t *src = (window_t *)lparam;
      if (src && src->parent == win) {
        return column_list_proc(src, msg, wparam, lparam);
      }
      return false;
    }
    
    default:
      return false;
  }
}
