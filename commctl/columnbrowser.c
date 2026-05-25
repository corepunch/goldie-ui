#include <stdlib.h>
#include <string.h>

#include "commctl.h"
#include "../user/messages.h"
#include "../user/theme.h"

#define CB_MAX_COLUMNS 16
#define CB_DEFAULT_COLUMN_WIDTH 100

typedef struct {
  window_t *columns[CB_MAX_COLUMNS];
  int widths[CB_MAX_COLUMNS];
  column_browser_delegate_t delegate;
  int min_column_width;
  int max_visible_columns;
  int last_column;
} column_browser_state_t;

static int cb_column_count(column_browser_state_t *st) {
  return st && st->last_column >= 0 ? st->last_column + 1 : 0;
}

static int cb_column_width(window_t *win, column_browser_state_t *st, int column) {
  if (!win || !st || column < 0 || column >= CB_MAX_COLUMNS)
    return CB_DEFAULT_COLUMN_WIDTH;

  int w = st->widths[column];
  if (w <= 0 && st->delegate.width_of_column)
    w = st->delegate.width_of_column(st->delegate.userdata, win, column);
  if (w <= 0)
    w = st->min_column_width;
  if (w <= 0)
    w = CB_DEFAULT_COLUMN_WIDTH;
  if (st->min_column_width > 0 && w < st->min_column_width)
    w = st->min_column_width;
  return w;
}

static void cb_sync_hscroll(window_t *win, int total_w, int page_w) {
  scroll_info_t si = {
    .fMask = SIF_ALL,
    .nMin = 0,
    .nMax = total_w,
    .nPage = (uint32_t)page_w,
    .nPos = (int)win->hscroll.pos,
  };
  set_scroll_info(win, SB_HORZ, &si, false);
}

static void cb_layout_columns(window_t *win, column_browser_state_t *st) {
  if (!win || !st)
    return;

  irect16_t cr = get_client_rect(win);
  if (cr.w < 1 || cr.h < 1)
    return;

  int total_w = 0;
  for (int column = 0; column <= st->last_column; column++) {
    if (st->columns[column])
      total_w += cb_column_width(win, st, column);
  }
  if (total_w < cr.w)
    total_w = cr.w;

  cb_sync_hscroll(win, total_w, cr.w);

  cr = get_client_rect(win);
  if (total_w < cr.w)
    total_w = cr.w;

  int max_pos = total_w - cr.w;
  if (max_pos < 0)
    max_pos = 0;
  if ((int)win->hscroll.pos > max_pos)
    win->hscroll.pos = (uint32_t)max_pos;

  int x = -(int)win->hscroll.pos;
  for (int column = 0; column <= st->last_column; column++) {
    window_t *col = st->columns[column];
    if (!col)
      continue;
    int w = cb_column_width(win, st, column);
    col->frame = R(x, 0, w, cr.h);
    x += w;
  }

  cb_sync_hscroll(win, total_w, cr.w);
  invalidate_window(win);
}

static window_t *cb_ensure_column(window_t *win, column_browser_state_t *st, int column) {
  if (!win || !st || column < 0 || column >= CB_MAX_COLUMNS)
    return NULL;

  if (st->columns[column])
    return st->columns[column];

  int w = cb_column_width(win, st, column);
  window_t *col = create_window("", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
                                MAKERECT(0, 0, w, get_client_rect(win).h),
                                win, win_reportview, 0, NULL);
  if (!col)
    return NULL;

  col->layout.layout_fixed_w = w;
  show_scroll_bar(col, SB_VERT, true);
  send_message(col, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
  send_message(col, RVM_SETCOLUMNTITLESVISIBLE, 0, NULL);
  reportview_column_t c0 = { "", 0 };
  send_message(col, RVM_ADDCOLUMN, 0, &c0);

  st->columns[column] = col;
  if (column > st->last_column)
    st->last_column = column;
  return col;
}

static void cb_remove_columns_after(column_browser_state_t *st, int column) {
  if (!st)
    return;
  for (int i = CB_MAX_COLUMNS - 1; i > column; i--) {
    window_t *col = st->columns[i];
    st->columns[i] = NULL;
    if (col)
      destroy_window(col);
  }
  st->last_column = column;
}

static bool cb_populate_column(window_t *win, column_browser_state_t *st, int column) {
  if (!win || !st || column < 0 || column >= CB_MAX_COLUMNS)
    return false;

  window_t *col = cb_ensure_column(win, st, column);
  if (!col)
    return false;

  int w = cb_column_width(win, st, column);
  col->layout.layout_fixed_w = w;
  send_message(col, RVM_CLEAR, 0, NULL);
  send_message(col, RVM_CLEARCOLUMNS, 0, NULL);
  reportview_column_t c0 = { "", 0 };
  send_message(col, RVM_ADDCOLUMN, 0, &c0);

  int count = st->delegate.number_of_rows
            ? st->delegate.number_of_rows(st->delegate.userdata, win, column)
            : 0;
  if (count < 0)
    count = 0;

  for (int row = 0; row < count; row++) {
    reportview_item_t item = {0};
    item.color = get_sys_color(brTextNormal);
    item.userdata = (uintptr_t)row;
    if (st->delegate.load_cell)
      st->delegate.load_cell(st->delegate.userdata, win, column, row, &item);
    if (!item.text)
      item.text = "";
    if (st->delegate.is_leaf &&
        !st->delegate.is_leaf(st->delegate.userdata, win, column, row)) {
      item.flags |= RVI_DISCLOSURE;
    }
    send_message(col, RVM_ADDITEM, 0, &item);
  }

  return true;
}

static int cb_selected_column(column_browser_state_t *st) {
  if (!st)
    return -1;
  for (int column = st->last_column; column >= 0; column--) {
    window_t *col = st->columns[column];
    if (col && (int)send_message(col, RVM_GETSELECTION, 0, NULL) >= 0)
      return column;
  }
  return -1;
}

static int cb_column_of_window(column_browser_state_t *st, window_t *candidate) {
  if (!st || !candidate)
    return -1;
  for (int column = 0; column < CB_MAX_COLUMNS; column++) {
    if (st->columns[column] == candidate)
      return column;
  }
  return -1;
}

static void cb_notify(window_t *win, uint16_t code, int column) {
  if (!win)
    return;
  window_t *target = win->parent ? get_root_window(win->parent) : win;
  send_message(target, evCommand, MAKEDWORD(column, code), win);
}

static void cb_handle_selection(window_t *win, column_browser_state_t *st,
                                int column, int row, uint16_t code) {
  if (!win || !st || column < 0 || row < 0)
    return;

  if (st->delegate.did_select)
    st->delegate.did_select(st->delegate.userdata, win, column, row);

  bool leaf = st->delegate.is_leaf
            ? st->delegate.is_leaf(st->delegate.userdata, win, column, row)
            : true;

  cb_remove_columns_after(st, column);
  if (!leaf)
    cb_populate_column(win, st, column + 1);

  cb_layout_columns(win, st);
  cb_notify(win, code == RVN_DBLCLK ? CBN_DBLCLK : CBN_SELCHANGE, column);
}

lresult_t win_column_browser(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  column_browser_state_t *st = (column_browser_state_t *)win->userdata;

  switch (msg) {
    case evCreate:
      st = allocate_window_data(win, sizeof(column_browser_state_t));
      if (!st)
        return false;
      st->min_column_width = CB_DEFAULT_COLUMN_WIDTH;
      st->max_visible_columns = 0;
      st->last_column = -1;
      if (lparam) {
        st->delegate = *(const column_browser_delegate_t *)lparam;
        cb_populate_column(win, st, 0);
        cb_layout_columns(win, st);
      }
      return true;

    case CBM_SETDELEGATE:
      if (!st)
        return false;
      memset(&st->delegate, 0, sizeof(st->delegate));
      if (lparam)
        st->delegate = *(const column_browser_delegate_t *)lparam;
      return true;

    case CBM_LOADCOLUMNZERO:
      if (!st)
        return false;
      cb_remove_columns_after(st, -1);
      cb_populate_column(win, st, 0);
      cb_layout_columns(win, st);
      return true;

    case CBM_ADDCOLUMN:
      if (!st || st->last_column + 1 >= CB_MAX_COLUMNS)
        return false;
      if (!cb_populate_column(win, st, st->last_column + 1))
        return false;
      cb_layout_columns(win, st);
      return true;

    case CBM_RELOADCOLUMN:
      if (!st || (int)wparam < 0 || (int)wparam >= CB_MAX_COLUMNS)
        return false;
      cb_remove_columns_after(st, (int)wparam);
      if (!cb_populate_column(win, st, (int)wparam))
        return false;
      cb_layout_columns(win, st);
      return true;

    case CBM_DISPLAYCOLUMN:
    case CBM_DISPLAYALLCOLUMNS:
      cb_layout_columns(win, st);
      return true;

    case CBM_SETLASTCOLUMN:
      if (!st || (int)wparam < -1 || (int)wparam >= CB_MAX_COLUMNS)
        return false;
      cb_remove_columns_after(st, (int)wparam);
      cb_layout_columns(win, st);
      return true;

    case CBM_GETLASTCOLUMN:
      return st ? st->last_column : -1;

    case CBM_GETSELECTEDCOLUMN:
      return cb_selected_column(st);

    case CBM_GETSELECTION:
      if (!st || (int)wparam < 0 || (int)wparam >= CB_MAX_COLUMNS || !st->columns[wparam])
        return -1;
      return send_message(st->columns[wparam], RVM_GETSELECTION, 0, NULL);

    case CBM_GETCOLUMNWINDOW:
      if (!st || (int)wparam < 0 || (int)wparam >= CB_MAX_COLUMNS)
        return 0;
      return (lresult_t)st->columns[wparam];

    case CBM_SETCOLUMNWIDTH:
      if (!st || (int)wparam < 0 || (int)wparam >= CB_MAX_COLUMNS)
        return false;
      st->widths[wparam] = (int)(intptr_t)lparam;
      if (st->columns[wparam])
        st->columns[wparam]->layout.layout_fixed_w = st->widths[wparam];
      cb_layout_columns(win, st);
      return true;

    case CBM_GETCOLUMNWIDTH:
      return cb_column_width(win, st, (int)wparam);

    case CBM_SETMINCOLUMNWIDTH:
      if (!st)
        return false;
      st->min_column_width = (int)wparam;
      cb_layout_columns(win, st);
      return true;

    case CBM_GETMINCOLUMNWIDTH:
      return st ? st->min_column_width : 0;

    case CBM_SETMAXVISIBLECOLUMNS:
      if (!st)
        return false;
      st->max_visible_columns = (int)wparam;
      cb_layout_columns(win, st);
      return true;

    case CBM_GETMAXVISIBLECOLUMNS:
      return st ? st->max_visible_columns : 0;

    case CBM_GETCOLUMNCOUNT:
      return cb_column_count(st);

    case CBM_VALIDATEVISIBLECOLUMNS:
      if (!st)
        return false;
      for (int column = 0; column <= st->last_column; column++) {
        if (st->delegate.column_is_valid &&
            !st->delegate.column_is_valid(st->delegate.userdata, win, column)) {
          cb_populate_column(win, st, column);
        }
      }
      cb_layout_columns(win, st);
      return true;

    case evCommand:
      if (st && (HIWORD(wparam) == RVN_SELCHANGE || HIWORD(wparam) == RVN_DBLCLK)) {
        int column = cb_column_of_window(st, (window_t *)lparam);
        if (column >= 0) {
          cb_handle_selection(win, st, column, (int)LOWORD(wparam), HIWORD(wparam));
          return true;
        }
      }
      return false;

    case evResize:
      cb_layout_columns(win, st);
      return true;

    case evHitTest: {
      uint16_t x = LOWORD(wparam), y = HIWORD(wparam);
      for (window_t *item = win->children; item; item = item->next) {
        irect16_t r = item->frame;
        if ((item->flags & WINDOW_NOTABSTOP) ||
            x < r.x || y < r.y || x >= r.x + r.w || y >= r.y + r.h)
          continue;
        lresult_t hit = send_message(item, evHitTest,
                                     MAKEDWORD((uint16_t)(x - r.x),
                                               (uint16_t)(y - r.y)),
                                     NULL);
        return hit ? (hit == 1 ? (lresult_t)(intptr_t)item : hit)
                   : (lresult_t)(intptr_t)item;
      }
      return false;
    }

    case evPaint: {
      uint32_t scroll_x = win->hscroll.pos;
      win->hscroll.pos = 0;
      lresult_t result = default_winproc(win, msg, wparam, lparam);
      win->hscroll.pos = scroll_x;
      return result;
    }

    case evHScroll:
      win->hscroll.pos = (uint32_t)wparam;
      cb_layout_columns(win, st);
      if (st && st->delegate.did_scroll)
        st->delegate.did_scroll(st->delegate.userdata, win);
      cb_notify(win, CBN_SCROLL, cb_selected_column(st));
      return true;

    case evDestroy:
      if (st) {
        free(st);
        win->userdata = NULL;
      }
      return false;

    default:
      return default_winproc(win, msg, wparam, lparam);
  }
}
