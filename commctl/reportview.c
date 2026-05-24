#include <stdlib.h>
#include <string.h>

#include "columnview_internal.h"
#include "../user/messages.h"
#include "../user/draw.h"
#include "../user/theme.h"

static int report_content_height(reportview_data_t *data) {
  return rv_report_header_height(data) + (int)data->count * ENTRY_HEIGHT;
}

static void report_sync_scroll(window_t *win, reportview_data_t *data) {
  if (!win || !data)
    return;

  irect16_t cr = get_client_rect(win);
  if (cr.h <= 0)
    return;

  int total_h = report_content_height(data);
  int max_scroll_px = total_h - cr.h;
  if (max_scroll_px < 0)
    max_scroll_px = 0;
  if ((int)win->vscroll.pos > max_scroll_px)
    win->vscroll.pos = (uint32_t)max_scroll_px;

  scroll_info_t si;
  si.fMask = SIF_ALL;
  si.nMin = 0;
  si.nMax = total_h;
  si.nPage = (uint32_t)cr.h;
  si.nPos = (int)win->vscroll.pos;
  set_scroll_info(win, SB_VERT, &si, false);
}

static int report_hit_index(window_t *win, reportview_data_t *data, uint32_t wparam) {
  int my = (int)(int16_t)HIWORD(wparam);
  int header_h = rv_report_header_height(data);
  int scroll_y = (int)win->vscroll.pos;
  if (my < header_h)
    return -1;
  int row = (my + scroll_y - header_h) / ENTRY_HEIGHT;
  return rv_valid_index(data, row) ? row : RV_INVALID_SELECTION;
}

static void report_scroll_to_item(window_t *win, reportview_data_t *data, int index) {
  int scroll_y = (int)win->vscroll.pos;
  int visible_h = get_client_rect(win).h;
  int header_h = rv_report_header_height(data);
  int item_y_top = header_h + index * ENTRY_HEIGHT;
  int item_y_bottom = item_y_top + ENTRY_HEIGHT;

  if (item_y_top - scroll_y < header_h)
    win->vscroll.pos = (uint32_t)(item_y_top - header_h);
  else if (item_y_bottom - scroll_y > visible_h)
    win->vscroll.pos = (uint32_t)(item_y_bottom - visible_h);
}

static void report_paint(window_t *win, reportview_data_t *data) {
  irect16_t cr = get_client_rect(win);
  int eff_w = cr.w;
  int header_h = rv_report_header_height(data);
  int body_h = cr.h - header_h;
  int scroll_y = (int)win->vscroll.pos;
  bool focused = (g_ui_runtime.focused == win);
  uint32_t bg_col = get_sys_color(brColumnViewBg);
  uint32_t sel_bg_col = get_sys_color(focused ? brTextNormal : brSelectionInactive);
  uint32_t sel_fg_col = get_sys_color(brWindowBg);

  int first_row = (body_h > 0) ? (scroll_y / ENTRY_HEIGHT) : 0;
  int last_row = (body_h > 0) ? ((scroll_y + body_h + ENTRY_HEIGHT - 1) / ENTRY_HEIGHT) : 0;
  if (first_row < 0) first_row = 0;
  if (last_row > (int)data->count) last_row = (int)data->count;

  uint32_t hdr_fg = get_sys_color(brTextNormal);
  uint32_t sep_col = get_sys_color(brDarkEdge);

  // Always paint the full client width so report/table views stretch visually
  // even when column specs do not consume all horizontal space.
  fill_rect(bg_col, R(0, header_h, eff_w, body_h));

  if (data->selected >= first_row && data->selected < last_row) {
    int y = header_h + data->selected * ENTRY_HEIGHT - scroll_y;
    if (y < header_h) y = header_h;
    fill_rect(sel_bg_col, R(0, y, eff_w, ENTRY_HEIGHT - 1));
  }

  int scr_x = window_screen_x(win);
  int scr_y = window_screen_y(win);

  int col_x = 0;
  for (uint32_t col = 0; col < data->column_count; col++) {
    int col_w = rv_get_report_column_width(data, (int)col, eff_w);
    if (header_h > 0) {
      set_clip_rect(NULL, (irect16_t){scr_x + col_x, scr_y, col_w, header_h});
      draw_button((irect16_t){col_x, 0, col_w, header_h}, 1, 1, false);
      draw_text_small_clipped(data->columns[col].title,
                              &(irect16_t){col_x, 0, col_w, header_h},
                              hdr_fg, TEXT_PADDING_LEFT);
    }

    int body_h_local = cr.h - header_h;
    set_clip_rect(NULL, (irect16_t){scr_x + col_x, scr_y + header_h, col_w, body_h_local});
    for (int row = first_row; row < last_row; row++) {
      reportview_item_t *it = &data->items[row];
      uint32_t fg = (row == data->selected) ? sel_fg_col
                  : it->color ? it->color : get_sys_color(brTextNormal);
      int y = header_h + row * ENTRY_HEIGHT - scroll_y;
      const char *src = "";
      if (col == 0) {
        src = it->text ? it->text : "";
      } else {
        uint32_t idx = col - 1;
        src = (idx < it->subitem_count && it->subitems[idx]) ? it->subitems[idx] : "";
      }
      draw_text_clipped(FONT_SMALL, src, &(irect16_t){col_x, y, col_w, ENTRY_HEIGHT},
                        fg, TEXT_PADDING_LEFT);
    }

    col_x += col_w;
  }

  set_clip_rect(NULL, (irect16_t){scr_x, scr_y, eff_w, cr.h});
  col_x = 0;
  for (uint32_t col = 0; col < data->column_count; col++) {
    int col_w = rv_get_report_column_width(data, (int)col, eff_w);
    col_x += col_w;
    fill_rect(sep_col, R(col_x, header_h, 1, cr.h - header_h));
  }
}

lresult_t win_reportview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  reportview_data_t *data = (reportview_data_t *)win->userdata2;

  switch (msg) {
    case evCreate: {
      data = calloc(1, sizeof(reportview_data_t));
      if (!data) return false;
      win->userdata2 = data;
      win->flags |= WINDOW_VSCROLL;
      win->flags |= WINDOW_FLEXSPACE;
      win->vscroll.visible_mode = SB_VIS_AUTO;
      data->selected = -1;
      data->last_click_index = RV_INVALID_SELECTION;
      data->column_width = DEFAULT_COLUMN_WIDTH;
      data->icon_size = DEFAULT_ICON_SIZE;
      data->icon_text_gap = DEFAULT_ICON_TEXT_GAP;
      data->redraw_enabled = true;
      data->redraw_dirty = false;
      data->column_titles_visible = true;
      data->preserve_icon_colors = false;
      report_sync_scroll(win, data);
      return true;
    }
    case evMeasure: {
      layout_measure_t *m = (layout_measure_t *)lparam;
      if (!m) return true;
      int min_h = rv_report_header_height(data) + ENTRY_HEIGHT;
      int min_w = 1;
      if (data && data->column_count > 0) {
        int cols_w = 0;
        for (uint32_t i = 0; i < data->column_count; i++) {
          if (i > 0) cols_w += 1;
          cols_w += data->columns[i].width > 0 ? (int)data->columns[i].width : (int)data->column_width;
        }
        if (cols_w > 0) min_w = MAX(min_w, cols_w);
      }
      m->desired_w = MAX(m->desired_w, min_w);
      m->desired_h = MAX(m->desired_h, min_h);
      return true;
    }
    case evPaint:
      report_paint(win, data);
      return false;
    case evLeftButtonDown: {
      int index = report_hit_index(win, data, wparam);
      if (rv_valid_index(data, index)) {
        uint32_t now = axGetMilliseconds();
        if (data->last_click_index == index && (now - data->last_click_time) < RV_DOUBLE_CLICK_MS) {
          rv_notify(win, data, index, RVN_DBLCLK);
          rv_reset_click_state(data);
        } else {
          int old_selection = data->selected;
          data->selected = index;
          data->last_click_time = now;
          data->last_click_index = index;
          if (old_selection != data->selected)
            rv_notify(win, data, index, RVN_SELCHANGE);
          rv_invalidate(win, data);
        }
      }
      return true;
    }
    case evLeftButtonDoubleClick: {
      int index = report_hit_index(win, data, wparam);
      if (rv_valid_index(data, index)) {
        rv_reset_click_state(data);
        rv_notify(win, data, index, RVN_DBLCLK);
      }
      return true;
    }
    case RVM_ADDITEM: {
      reportview_item_t *item = (reportview_item_t *)lparam;
      if (!item || data->count >= MAX_COLUMNVIEW_ITEMS)
        return -1;
      uint32_t i = data->count;
      if (!rv_store_item(data, i, item))
        return -1;
      data->count++;
      report_sync_scroll(win, data);
      rv_invalidate(win, data);
      return (lresult_t)i;
    }
    case RVM_DELETEITEM: {
      if (wparam >= data->count)
        return false;
      memmove(data->items + wparam, data->items + wparam + 1, (data->count - wparam - 1) * sizeof(data->items[0]));
      memmove(data->names + wparam, data->names + wparam + 1, (data->count - wparam - 1) * sizeof(data->names[0]));
      memmove(data->subnames + wparam, data->subnames + wparam + 1, (data->count - wparam - 1) * sizeof(data->subnames[0]));
      data->count--;
      rv_rebind_item_refs(data, (uint32_t)wparam);
      if (data->selected == (int)wparam) data->selected = RV_INVALID_SELECTION;
      else if (data->selected > (int)wparam) data->selected--;
      report_sync_scroll(win, data);
      rv_invalidate(win, data);
      return true;
    }
    case RVM_GETITEMCOUNT:
      return (lresult_t)data->count;
    case RVM_GETSELECTION:
      return (lresult_t)data->selected;
    case RVM_SETSELECTION:
      if ((int)wparam >= 0 && wparam < data->count) {
        data->selected = (int)wparam;
        report_scroll_to_item(win, data, data->selected);
        report_sync_scroll(win, data);
        rv_invalidate(win, data);
        return true;
      }
      return false;
    case RVM_CLEAR:
      data->count = 0;
      rv_reset_view_state(win, data);
      report_sync_scroll(win, data);
      rv_invalidate(win, data);
      return true;
    case RVM_SETCOLUMNWIDTH:
      if (wparam > 0) {
        data->column_width = (int)wparam;
        report_sync_scroll(win, data);
        rv_invalidate(win, data);
        return true;
      }
      return false;
    case RVM_GETCOLUMNWIDTH:
      return (lresult_t)data->column_width;
    case RVM_GETITEMDATA:
      if (wparam < data->count && lparam) {
        *(reportview_item_t *)lparam = data->items[wparam];
        return true;
      }
      return false;
    case RVM_HITTEST:
      return (lresult_t)report_hit_index(win, data, wparam);
    case RVM_SETITEMDATA: {
      reportview_item_t *item = (reportview_item_t *)lparam;
      if (!item || wparam >= data->count)
        return false;
      if (!rv_store_item(data, (uint32_t)wparam, item))
        return false;
      rv_invalidate(win, data);
      return true;
    }
    case RVM_SETVIEWMODE:
      return wparam == RVM_VIEW_REPORT;
    case RVM_ADDCOLUMN: {
      reportview_column_t *col = (reportview_column_t *)lparam;
      if (!col || data->column_count >= MAX_REPORTVIEW_COLUMNS)
        return -1;
      uint32_t i = data->column_count;
      strncpy(data->columns[i].title, col->title ? col->title : "", MAX_REPORTVIEW_TITLE - 1);
      data->columns[i].title[MAX_REPORTVIEW_TITLE - 1] = '\0';
      data->columns[i].width_spec = col->width;  // Store original spec
      data->columns[i].width = col->width;        // Initial effective width
      data->column_count++;
      rv_invalidate(win, data);
      return (lresult_t)i;
    }
    case RVM_CLEARCOLUMNS:
      data->column_count = 0;
      rv_invalidate(win, data);
      return true;
    case RVM_GETCOLUMNCOUNT:
      return (lresult_t)data->column_count;
    case RVM_SETREPORTCOLUMNWIDTH: {
      uint32_t ci = (uint32_t)wparam;
      if (ci >= data->column_count)
        return false;
      data->columns[ci].width = (uint32_t)(uintptr_t)lparam;
      rv_invalidate(win, data);
      return true;
    }
    case RVM_GETREPORTCOLUMNWIDTH: {
      uint32_t ci = (uint32_t)wparam;
      if (ci >= data->column_count)
        return 0;
      return (lresult_t)data->columns[ci].width;
    }
    case RVM_SETREDRAW:
      if (wparam) {
        data->redraw_enabled = true;
        if (data->redraw_dirty) {
          data->redraw_dirty = false;
          invalidate_window(win);
        }
      } else {
        data->redraw_enabled = false;
      }
      return true;
    case RVM_SETICONSTRIP:
      data->icon_strip = (bitmap_strip_t *)lparam;
      rv_invalidate(win, data);
      return true;
    case RVM_SETLARGEICONCOLS:
      data->fixed_large_icon_cols = (int)wparam;
      rv_invalidate(win, data);
      return true;
    case RVM_SETICONSIZE:
      data->icon_size = (int)wparam;
      rv_invalidate(win, data);
      return true;
    case RVM_SETCOLUMNTITLESVISIBLE:
      data->column_titles_visible = (wparam != 0);
      report_sync_scroll(win, data);
      rv_invalidate(win, data);
      return true;
    case RVM_GETCOLUMNTITLESVISIBLE:
      return data->column_titles_visible ? true : false;
    case RVM_SETPRESERVEICONCOLORS:
      data->preserve_icon_colors = (wparam != 0);
      rv_invalidate(win, data);
      return true;
    case RVM_SETICONTEXTGAP:
      data->icon_text_gap = (int)wparam;
      if (data->icon_text_gap < 0) data->icon_text_gap = 0;
      rv_invalidate(win, data);
      return true;
    case evVScroll:
      win->vscroll.pos = (uint32_t)wparam;
      report_sync_scroll(win, data);
      rv_invalidate(win, data);
      return true;
    case evArrange: {
      layout_arrange_t const *a = (layout_arrange_t const *)lparam;
      if (a) {
        irect16_t r = a->rect;
        if (r.w < 1) r.w = 1;
        if (r.h < 1) r.h = 1;
        win->frame = r;
      }
      // Recalculate flex column widths after arranging
      irect16_t cr = get_client_rect(win);
      if (cr.w > 0 && data->column_count > 0) {
        for (uint32_t i = 0; i < data->column_count; i++) {
          int w = rv_get_report_column_width(data, (int)i, cr.w);
          data->columns[i].width = (uint32_t)w;
        }
      }
      report_sync_scroll(win, data);
      return true;
    }
    case evResize: {
      // Recalculate flex column widths when window resizes
      irect16_t cr = get_client_rect(win);
      if (cr.w <= 0) {
        // Window not sized yet - skip column width calculation
        report_sync_scroll(win, data);
        return false;
      }
      int avail_w = cr.w;
      for (uint32_t i = 0; i < data->column_count; i++) {
        int w = rv_get_report_column_width(data, (int)i, avail_w);
        data->columns[i].width = (uint32_t)w;
      }

      report_sync_scroll(win, data);
      rv_invalidate(win, data);
      return false;
    }
    case evKeyDown: {
      if (!data || data->count == 0)
        return false;
      int count = (int)data->count;
      int cur = data->selected;
      int next = cur;
      switch (wparam) {
        case AX_KEY_UPARROW:
        case AX_KEY_LEFTARROW:
          next = (cur <= 0) ? 0 : cur - 1;
          break;
        case AX_KEY_DOWNARROW:
        case AX_KEY_RIGHTARROW:
          next = (cur < 0) ? 0 : (cur + 1 < count ? cur + 1 : cur);
          break;
        case AX_KEY_ENTER:
          if (cur < 0) return false;
          rv_notify(win, data, cur, RVN_DBLCLK);
          return true;
        case AX_KEY_DEL:
          if (cur < 0) return false;
          rv_notify(win, data, cur, RVN_DELETE);
          return true;
        default:
          return default_winproc(win, msg, wparam, lparam);
      }
      if (next != cur && next >= 0) {
        data->selected = next;
        report_scroll_to_item(win, data, next);
        report_sync_scroll(win, data);
        rv_notify(win, data, next, RVN_SELCHANGE);
        rv_invalidate(win, data);
      }
      return true;
    }
    case evDestroy:
      free(data);
      win->userdata2 = NULL;
      return true;
    default:
      return default_winproc(win, msg, wparam, lparam);
  }
}
