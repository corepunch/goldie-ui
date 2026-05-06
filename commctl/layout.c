#include <stdlib.h>
#include <string.h>

#include "../user/user.h"
#include "../user/messages.h"
#include "commctl.h"

extern int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

static int layout_apply_alignment(int avail, int desired, uint8_t align) {
  if (avail < 0) avail = 0;
  if (desired < 0) desired = 0;
  if (align == LAYOUT_ALIGN_STRETCH) return avail;
  if (desired > avail) desired = avail;
  return desired;
}

static bool layout_is_horizontal(const window_t *win) {
  return win && (win->layout_orientation & WINDOW_STACK_HORIZONTAL) != 0;
}

static bool is_stack_layout(const window_t *win) {
  return win && win->layout_kind && strcmp(win->layout_kind, "stack") == 0;
}

static bool is_grid_layout(const window_t *win) {
  return win && win->layout_kind && strcmp(win->layout_kind, "grid") == 0;
}

static layout_measure_t layout_measure_child(window_t *child, int avail_w, int avail_h) {
  irect16_t margin = child ? child->layout_margin : (irect16_t){0, 0, 0, 0};
  avail_w -= margin.x + margin.w;
  avail_h -= margin.y + margin.h;
  if (avail_w < 0) avail_w = 0;
  if (avail_h < 0) avail_h = 0;
  layout_measure_t m = {
    .avail_w = avail_w,
    .avail_h = avail_h,
    .desired_w = child && child->frame.w > 0 ? child->frame.w : 1,
    .desired_h = child && child->frame.h > 0 ? child->frame.h : 1,
  };
  if (child) {
    send_message(child, evMeasure, 0, &m);
    m.desired_w += margin.x + margin.w;
    m.desired_h += margin.y + margin.h;
    if (m.desired_w < 1) m.desired_w = 1;
    if (m.desired_h < 1) m.desired_h = 1;
  }
  return m;
}

static int layout_spacing_for(window_t *win) {
  return win ? (int)win->layout_spacing : 0;
}

static irect16_t layout_padding_for(window_t *win) {
  return win ? win->layout_padding : (irect16_t){0, 0, 0, 0};
}

static irect16_t layout_margin_for(window_t *win) {
  return win ? win->layout_margin : (irect16_t){0, 0, 0, 0};
}

static irect16_t layout_inset_rect(irect16_t r, irect16_t inset) {
  r.x += inset.x;
  r.y += inset.y;
  r.w -= inset.x + inset.w;
  r.h -= inset.y + inset.h;
  if (r.w < 0) r.w = 0;
  if (r.h < 0) r.h = 0;
  return r;
}

static irect16_t layout_content_rect(window_t *win, irect16_t r) {
  return layout_inset_rect(r, layout_padding_for(win));
}

static void layout_arrange_child(window_t *child, irect16_t rect) {
  rect = layout_inset_rect(rect, layout_margin_for(child));
  layout_arrange_t a = {
    .rect = rect,
    .h_align = child ? child->h_align : LAYOUT_ALIGN_STRETCH,
    .v_align = child ? child->v_align : LAYOUT_ALIGN_STRETCH,
  };
  send_message(child, evArrange, 0, &a);
}

void layout_measure_window(window_t *win, layout_measure_t *m) {
  irect16_t cr = get_client_rect(win);
  if (m) {
    if (m->avail_w > 0) cr.w = m->avail_w;
    if (m->avail_h > 0) cr.h = m->avail_h;
  }
  irect16_t content = layout_content_rect(win, cr);
  int content_w = content.w;
  int content_h = content.h;
  if (is_grid_layout(win)) {
    int cols = win->layout_columns > 0 ? win->layout_columns : 2;
    int count = 0;
    for (window_t *child = win->children; child; child = child->next) {
      count++;
    }
    int rows = (count + cols - 1) / cols;
    if (rows < 1) rows = 1;
    int gap = layout_spacing_for(win);
    int *col_w = cols > 0 ? calloc((size_t)cols, sizeof(int)) : NULL;
    int *row_h = rows > 0 ? calloc((size_t)rows, sizeof(int)) : NULL;
    if (!col_w || !row_h) {
      free(col_w);
      free(row_h);
      if (m) {
        m->desired_w = content.w + layout_padding_for(win).x + layout_padding_for(win).w;
        m->desired_h = content.h + layout_padding_for(win).y + layout_padding_for(win).h;
      }
      return;
    }
    int idx = 0;
    for (window_t *child = win->children; child; child = child->next) {
      int row = idx / cols;
      int col = idx % cols;
      layout_measure_t cm = layout_measure_child(child, content_w, content_h);
      if (cm.desired_w > col_w[col]) col_w[col] = cm.desired_w;
      if (cm.desired_h > row_h[row]) row_h[row] = cm.desired_h;
      idx++;
    }
    int total_w = 0;
    int total_h = 0;
    for (int col = 0; col < cols; col++) {
      if (col > 0) total_w += gap;
      total_w += col_w[col];
    }
    for (int row = 0; row < rows; row++) {
      if (row > 0) total_h += gap;
      total_h += row_h[row];
    }
    free(col_w);
    free(row_h);
    if (m) {
      m->desired_w = total_w + layout_padding_for(win).x + layout_padding_for(win).w;
      m->desired_h = total_h + layout_padding_for(win).y + layout_padding_for(win).h;
    }
    return;
  }

  int count = 0;
  int desired_w = 0;
  int desired_h = 0;
  int gap = layout_spacing_for(win);

  for (window_t *child = win->children; child; child = child->next) {
    layout_measure_t cm = layout_measure_child(child, content_w, content_h);
    if (layout_is_horizontal(win)) {
      if (count > 0) desired_w += gap;
      desired_w += cm.desired_w;
      if (cm.desired_h > desired_h) desired_h = cm.desired_h;
    } else {
      if (count > 0) desired_h += gap;
      desired_h += cm.desired_h;
      if (cm.desired_w > desired_w) desired_w = cm.desired_w;
    }
    count++;
  }

  if (count == 0) {
    desired_w = content.w;
    desired_h = content.h;
  }
  if (m) {
    m->desired_w = desired_w + layout_padding_for(win).x + layout_padding_for(win).w;
    m->desired_h = desired_h + layout_padding_for(win).y + layout_padding_for(win).h;
  }
}

void layout_arrange_window(window_t *win, const irect16_t *rect) {
  irect16_t cr = rect ? *rect : get_client_rect(win);
  irect16_t content = layout_content_rect(win, cr);
  int gap = layout_spacing_for(win);

  if (is_stack_layout(win)) {
    int count = 0;
    if (layout_is_horizontal(win)) {
      int total_fixed = 0;
      int stretch_count = 0;
      for (window_t *child = win->children; child; child = child->next) {
        layout_measure_t cm = layout_measure_child(child, content.w, content.h);
        if (child->h_align == LAYOUT_ALIGN_STRETCH) stretch_count++;
        else total_fixed += cm.desired_w;
        count++;
      }
      int total_gap = (count > 0) ? gap * (count - 1) : 0;
      int remaining = content.w - total_fixed - total_gap;
      if (remaining < 0) remaining = 0;
      int stretch_share = stretch_count > 0 ? remaining / stretch_count : 0;
      for (window_t *child = win->children; child; child = child->next) {
        layout_measure_t cm = layout_measure_child(child, content.w, content.h);
        int cw = (child->h_align == LAYOUT_ALIGN_STRETCH) ? stretch_share
                                                          : cm.desired_w;
        int ch = layout_apply_alignment(content.h, cm.desired_h, child->v_align);
        int cy = content.y;
        if (child->v_align == LAYOUT_ALIGN_CENTER)
          cy += (content.h - ch) / 2;
        else if (child->v_align == LAYOUT_ALIGN_END)
          cy += content.h - ch;
        layout_arrange_child(child, R(content.x, cy, cw, ch));
      }
      layout_flow_horizontal(win->children, content.x, gap);
    } else {
      int total_fixed = 0;
      int stretch_count = 0;
      for (window_t *child = win->children; child; child = child->next) {
        layout_measure_t cm = layout_measure_child(child, content.w, content.h);
        if (child->v_align == LAYOUT_ALIGN_STRETCH) stretch_count++;
        else total_fixed += cm.desired_h;
        count++;
      }
      int total_gap = (count > 0) ? gap * (count - 1) : 0;
      int remaining = content.h - total_fixed - total_gap;
      if (remaining < 0) remaining = 0;
      int stretch_share = stretch_count > 0 ? remaining / stretch_count : 0;
      int y = content.y;
      for (window_t *child = win->children; child; child = child->next) {
        if (y > content.y) y += gap;
        layout_measure_t cm = layout_measure_child(child, content.w, content.h);
        int cw = layout_apply_alignment(content.w, cm.desired_w, child->h_align);
        int ch = (child->v_align == LAYOUT_ALIGN_STRETCH) ? stretch_share
                                                          : cm.desired_h;
        int cx = content.x;
        if (child->h_align == LAYOUT_ALIGN_CENTER)
          cx += (content.w - cw) / 2;
        else if (child->h_align == LAYOUT_ALIGN_END)
          cx += content.w - cw;
        layout_arrange_child(child, R(cx, y, cw, ch));
        y += ch;
      }
    }
    return;
  }

  if (is_grid_layout(win)) {
    int cols = win->layout_columns > 0 ? win->layout_columns : 2;
    int count = 0;
    for (window_t *child = win->children; child; child = child->next) {
      count++;
    }
    int rows = (count + cols - 1) / cols;
    if (rows < 1) rows = 1;
    int *col_w = cols > 0 ? calloc((size_t)cols, sizeof(int)) : NULL;
    int *row_h = rows > 0 ? calloc((size_t)rows, sizeof(int)) : NULL;
    bool *col_stretch = cols > 0 ? calloc((size_t)cols, sizeof(bool)) : NULL;
    if (!col_w || !row_h || !col_stretch) {
      free(col_w);
      free(row_h);
      free(col_stretch);
      return;
    }
    int idx = 0;
    for (window_t *child = win->children; child; child = child->next) {
      int row = idx / cols;
      int col = idx % cols;
      layout_measure_t cm = layout_measure_child(child, content.w, content.h);
      if (cm.desired_w > col_w[col]) col_w[col] = cm.desired_w;
      if (cm.desired_h > row_h[row]) row_h[row] = cm.desired_h;
      if (child->h_align == LAYOUT_ALIGN_STRETCH)
        col_stretch[col] = true;
      idx++;
    }
    int total_w = 0;
    for (int col = 0; col < cols; col++) {
      total_w += col_w[col];
    }
    int total_gap = (cols > 0) ? gap * (cols - 1) : 0;
    int remaining = content.w - total_w - total_gap;
    if (remaining > 0) {
      int stretch_cols = 0;
      for (int col = 0; col < cols; col++) {
        if (col_stretch[col])
          stretch_cols++;
      }
      if (stretch_cols > 0) {
        int base = remaining / stretch_cols;
        int extra = remaining % stretch_cols;
        for (int col = 0; col < cols; col++) {
          if (!col_stretch[col])
            continue;
          col_w[col] += base;
          if (extra > 0) {
            col_w[col]++;
            extra--;
          }
        }
      }
    }
    int row_y = content.y;
    for (int row = 0; row < rows; row++) {
      int col_x = content.x;
      if (row > 0) row_y += row_h[row - 1] + gap;
      for (int col = 0; col < cols; col++) {
        int child_index = row * cols + col;
        if (child_index >= count) break;
        window_t *child = win->children;
        for (int k = 0; k < child_index; k++) child = child->next;
        int cell_w = col_w[col];
        int cell_h = row_h[row];
        layout_measure_t cm = layout_measure_child(child, cell_w, cell_h);
        int cw = (child->h_align == LAYOUT_ALIGN_STRETCH) ? cell_w
               : (cm.desired_w < cell_w ? cm.desired_w : cell_w);
        int ch = (child->v_align == LAYOUT_ALIGN_STRETCH) ? cell_h
               : (cm.desired_h < cell_h ? cm.desired_h : cell_h);
        int x = col_x;
        int y = row_y;
        if (child->h_align == LAYOUT_ALIGN_CENTER)
          x += (cell_w - cw) / 2;
        else if (child->h_align == LAYOUT_ALIGN_END)
          x += cell_w - cw;
        if (child->v_align == LAYOUT_ALIGN_CENTER)
          y += (cell_h - ch) / 2;
        else if (child->v_align == LAYOUT_ALIGN_END)
          y += cell_h - ch;
        layout_arrange_child(child, R(x, y, cw, ch));
        col_x += col_w[col] + gap;
      }
    }
    free(col_w);
    free(row_h);
    free(col_stretch);
  }
}

void window_layout_sync(window_t *win) {
  if (!win || !win->auto_layout || !win->layout_kind || !*win->layout_kind)
    return;
  irect16_t cr = get_client_rect(win);
  layout_arrange_window(win, &cr);
}

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

void layout_flow_horizontal(window_t *first, int start_x, int gap) {
  int cur_x = start_x;
  bool placed_visual = false;
  bool prev_was_space = false;
  for (window_t *child = first; child; child = child->next) {
    bool is_space = child->proc == win_space;
    if (placed_visual && !prev_was_space && !is_space)
      cur_x += gap;
    child->frame.x = cur_x;
    cur_x += child->frame.w;
    placed_visual = true;
    prev_was_space = is_space;
  }
}

static result_t layout_container_proc(window_t *win, uint32_t msg,
                                      uint32_t wparam, void *lparam,
                                      const char *default_layout_kind,
                                      flags_t default_orientation,
                                      uint8_t default_columns,
                                      uint8_t default_spacing) {
  (void)wparam;
  switch (msg) {
    case evCreate: {
      const layout_view_config_t *cfg = (const layout_view_config_t *)lparam;
      win->auto_layout = true;
      win->layout_kind = default_layout_kind;
      win->layout_orientation = default_orientation;
      win->layout_columns = default_columns;
      win->layout_spacing = default_spacing;
      win->layout_padding = (irect16_t){0, 0, 0, 0};
      win->layout_margin = (irect16_t){0, 0, 0, 0};
      win->h_align = LAYOUT_ALIGN_STRETCH;
      win->v_align = LAYOUT_ALIGN_STRETCH;
      if (cfg) {
        if (cfg->layout_kind && *cfg->layout_kind)
          win->layout_kind = cfg->layout_kind;
        win->layout_orientation = cfg->orientation & WINDOW_STACK_HORIZONTAL;
        if (cfg->columns > 0)
          win->layout_columns = cfg->columns;
        if (cfg->spacing > 0)
          win->layout_spacing = cfg->spacing;
        win->layout_padding = cfg->padding;
        win->layout_margin = cfg->margin;
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
                               "stack",
                               WINDOW_STACK_VERTICAL,
                               0,
                               4);
}

result_t win_gridview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  return layout_container_proc(win, msg, wparam, lparam,
                               "grid",
                               WINDOW_STACK_VERTICAL,
                               2,
                               0);
}
