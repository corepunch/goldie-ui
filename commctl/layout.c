#include <string.h>

#include "../user/user.h"
#include "../user/messages.h"
#include "commctl.h"

extern int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

static void layout_paint_children(window_t *win) {
  if (!win) return;
  int origin_x = win->frame.x;
  int origin_y = win->frame.y + titlebar_height(win);
  for (window_t *child = win->children; child; child = child->next) {
    irect16_t saved = child->frame;
    child->frame.x = origin_x + saved.x;
    child->frame.y = origin_y + saved.y;
    send_message(child, evPaint, 0, NULL);
    child->frame = saved;
  }
}

static result_t layout_container_proc(window_t *win, uint32_t msg,
                                      uint32_t wparam, void *lparam,
                                      window_layout_kind_t kind,
                                      window_stack_orientation_t default_orientation,
                                      uint8_t default_columns,
                                      uint8_t default_spacing) {
  (void)wparam;
  switch (msg) {
    case evCreate: {
      const layout_view_config_t *cfg = (const layout_view_config_t *)lparam;
      win->auto_layout = true;
      win->layout_kind = kind;
      win->layout_orientation = default_orientation;
      win->layout_columns = default_columns;
      win->layout_spacing = default_spacing;
      win->h_align = LAYOUT_ALIGN_STRETCH;
      win->v_align = LAYOUT_ALIGN_STRETCH;
      if (cfg) {
        if (cfg->layout_kind == WINDOW_LAYOUT_STACK || cfg->layout_kind == WINDOW_LAYOUT_GRID)
          win->layout_kind = (window_layout_kind_t)cfg->layout_kind;
        if (cfg->orientation <= WINDOW_STACK_HORIZONTAL)
          win->layout_orientation = (window_stack_orientation_t)cfg->orientation;
        if (cfg->columns > 0)
          win->layout_columns = cfg->columns;
        if (cfg->spacing > 0)
          win->layout_spacing = cfg->spacing;
      }
      return true;
    }
    case evMeasure: {
      layout_measure_t *m = (layout_measure_t *)lparam;
      if (m) layout_measure_window(win, m);
      return true;
    }
    case evArrange: {
      layout_arrange_t *a = (layout_arrange_t *)lparam;
      if (a) {
        win->frame = a->rect;
        window_layout_sync(win);
      }
      return true;
    }
    case evResize:
      window_layout_sync(win);
      return true;
    case evPaint:
      layout_paint_children(win);
      return true;
    default:
      return false;
  }
}

result_t win_stackview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  return layout_container_proc(win, msg, wparam, lparam,
                               WINDOW_LAYOUT_STACK,
                               WINDOW_STACK_VERTICAL,
                               0,
                               4);
}

result_t win_gridview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  return layout_container_proc(win, msg, wparam, lparam,
                               WINDOW_LAYOUT_GRID,
                               WINDOW_STACK_VERTICAL,
                               2,
                               0);
}
