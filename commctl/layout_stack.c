#include <string.h>

#include "../user/user.h"
#include "../user/messages.h"
#include "commctl.h"
#include "layout_shared.h"

extern void layout_grid_measure_window(window_t *win, layout_measure_t *m);
extern void layout_grid_arrange_window(window_t *win, const irect16_t *rect);
extern void layout_flow_measure_window(window_t *win, layout_measure_t *m);
extern void layout_flow_arrange_window(window_t *win, const irect16_t *rect);

static bool layout_kind_is(const window_t *win, const char *kind) {
  return win && win->layout_kind && kind && strcmp(win->layout_kind, kind) == 0;
}

void layout_stack_measure_window(window_t *win, layout_measure_t *m) {
  irect16_t cr = get_client_rect(win);
  if (m) {
    if (m->avail_w > 0) cr.w = m->avail_w;
    if (m->avail_h > 0) cr.h = m->avail_h;
  }
  irect16_t content = layout_content_rect(win, cr);
  int content_w = content.w;
  int content_h = content.h;
  int count = 0;
  int desired_w = 0;
  int desired_h = 0;
  int gap = layout_spacing_for(win);

  for (window_t *child = win ? win->children : NULL; child; child = child->next) {
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
    irect16_t pad = layout_padding_for(win);
    m->desired_w = desired_w + pad.x + pad.w;
    m->desired_h = desired_h + pad.y + pad.h;
  }
}

void layout_stack_arrange_window(window_t *win, const irect16_t *rect) {
  irect16_t cr = rect ? *rect : get_client_rect(win);
  irect16_t content = layout_content_rect(win, cr);
  int gap = layout_spacing_for(win);

  if (layout_is_horizontal(win)) {
    int total_fixed = 0;
    int stretch_count = 0;
    for (window_t *child = win ? win->children : NULL; child; child = child->next) {
      layout_measure_t cm = layout_measure_child(child, content.w, content.h);
      bool stretchable = (child->flags & (WINDOW_FLEXSPACE | WINDOW_VSCROLL)) != 0;
      if (stretchable) stretch_count++;
      else total_fixed += cm.desired_w;
    }
    int total_gap = layout_horizontal_gap_total(win ? win->children : NULL, gap);
    int remaining = content.w - total_fixed - total_gap;
    if (remaining < 0) remaining = 0;
    int stretch_share = stretch_count > 0 ? remaining / stretch_count : 0;
    int x = content.x;
    bool placed_visual = false;
    bool prev_was_space = false;
    for (window_t *child = win ? win->children : NULL; child; child = child->next) {
      bool is_space = (child->flags & WINDOW_FLEXSPACE) != 0;
      if (placed_visual && !prev_was_space && !is_space)
        x += gap;
      layout_measure_t cm = layout_measure_child(child, content.w, content.h);
      bool stretchable = (child->flags & (WINDOW_FLEXSPACE | WINDOW_VSCROLL)) != 0;
      int cw = stretchable ? stretch_share : cm.desired_w;
      int ch = layout_apply_alignment(content.h, cm.desired_h, child->v_align);
      int cy = content.y;
      if (child->v_align == LAYOUT_ALIGN_CENTER)
        cy += (content.h - ch) / 2;
      else if (child->v_align == LAYOUT_ALIGN_END)
        cy += content.h - ch;
      layout_arrange_child(child, R(x, cy, cw, ch));
      x += cw;
      placed_visual = true;
      prev_was_space = is_space;
    }
  } else {
    int total_fixed = 0;
    int stretch_count = 0;
    int count = 0;
    for (window_t *child = win ? win->children : NULL; child; child = child->next) {
      layout_measure_t cm = layout_measure_child(child, content.w, content.h);
      bool stretchable = (child->flags & (WINDOW_FLEXSPACE | WINDOW_VSCROLL)) != 0;
      if (stretchable) stretch_count++;
      else total_fixed += cm.desired_h;
      count++;
    }
    int total_gap = (count > 0) ? gap * (count - 1) : 0;
    int remaining = content.h - total_fixed - total_gap;
    if (remaining < 0) remaining = 0;
    int stretch_share = stretch_count > 0 ? remaining / stretch_count : 0;
    int y = content.y;
    for (window_t *child = win ? win->children : NULL; child; child = child->next) {
      if (y > content.y) y += gap;
      layout_measure_t cm = layout_measure_child(child, content.w, content.h);
      int cw = layout_apply_alignment(content.w, cm.desired_w, child->h_align);
      bool stretchable = (child->flags & (WINDOW_FLEXSPACE | WINDOW_VSCROLL)) != 0;
      int ch = stretchable ? stretch_share : cm.desired_h;
      int cx = content.x;
      if (child->h_align == LAYOUT_ALIGN_CENTER)
        cx += (content.w - cw) / 2;
      else if (child->h_align == LAYOUT_ALIGN_END)
        cx += content.w - cw;
      layout_arrange_child(child, R(cx, y, cw, ch));
      y += ch;
    }
  }
}

void layout_flow_horizontal(window_t *first, int start_x, int gap) {
  int cur_x = start_x;
  bool placed_visual = false;
  bool prev_was_space = false;
  for (window_t *child = first; child; child = child->next) {
    bool is_space = (child->flags & WINDOW_FLEXSPACE) != 0;
    if (placed_visual && !prev_was_space && !is_space)
      cur_x += gap;
    child->frame.x = cur_x;
    cur_x += child->frame.w;
    placed_visual = true;
    prev_was_space = is_space;
  }
}

void layout_measure_window(window_t *win, layout_measure_t *m) {
  if (!win || !m) return;
  if (win->layout_measure_fn) {
    win->layout_measure_fn(win, m);
    return;
  }
  if (layout_kind_is(win, "stack") || layout_kind_is(win, "stackview")) {
    layout_stack_measure_window(win, m);
    return;
  }
  if (layout_kind_is(win, "grid")) {
    layout_grid_measure_window(win, m);
    return;
  }
  if (layout_kind_is(win, "flow") || layout_kind_is(win, "flowview")) {
    layout_flow_measure_window(win, m);
    return;
  }
  irect16_t cr = get_client_rect(win);
  if (m->desired_w <= 0) m->desired_w = cr.w > 0 ? cr.w : 1;
  if (m->desired_h <= 0) m->desired_h = cr.h > 0 ? cr.h : 1;
}

void layout_arrange_window(window_t *win, const irect16_t *rect) {
  if (!win) return;
  if (win->layout_arrange_fn) {
    win->layout_arrange_fn(win, rect);
    return;
  }
  if (layout_kind_is(win, "stack") || layout_kind_is(win, "stackview")) {
    layout_stack_arrange_window(win, rect);
    return;
  }
  if (layout_kind_is(win, "grid")) {
    layout_grid_arrange_window(win, rect);
    return;
  }
  if (layout_kind_is(win, "flow") || layout_kind_is(win, "flowview")) {
    layout_flow_arrange_window(win, rect);
  }
}

void window_layout_sync(window_t *win) {
  if (!win || !win->auto_layout || !win->layout_kind || !*win->layout_kind)
    return;
  irect16_t cr = get_client_rect(win);
  layout_arrange_window(win, &cr);
}

result_t win_stackview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate: {
      const layout_view_config_t *cfg = (const layout_view_config_t *)lparam;
      win->auto_layout = true;
      win->layout_kind = "stack";
      win->layout_orientation = WINDOW_STACK_VERTICAL;
      win->layout_spacing = 4;
      win->layout_padding = (irect16_t){0, 0, 0, 0};
      win->layout_margin = (irect16_t){0, 0, 0, 0};
      win->layout_measure_fn = layout_stack_measure_window;
      win->layout_arrange_fn = layout_stack_arrange_window;
      win->h_align = LAYOUT_ALIGN_STRETCH;
      win->v_align = LAYOUT_ALIGN_STRETCH;
      if (cfg) {
        if (cfg->layout_kind && *cfg->layout_kind)
          win->layout_kind = cfg->layout_kind;
        win->layout_orientation = cfg->orientation & WINDOW_STACK_HORIZONTAL;
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
    case evParentNotify:
      return win->parent ? send_message(win->parent, msg, wparam, lparam) : false;
    default:
      return false;
  }
}
