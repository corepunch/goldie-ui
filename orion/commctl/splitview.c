// win_splitview — two-pane container with a resizable divider.
//
// Creates two child content windows separated by a draggable splitter bar.
// The split orientation (vertical or horizontal) is set at creation time via
// lparam: (void *)SPLIT_VERT or (void *)SPLIT_HORZ.
//
// SplitView manages the splitter drag loop internally — the parent does not
// need to handle spnDragStart.  The divider position is tracked as a ratio
// (0.0 .. 1.0) of the total content area.
//
// Usage:
//   window_t *sv = create_window("", WINDOW_NOTITLE | WINDOW_NOFILL,
//                                &rect, parent, win_splitview,
//                                hinstance, (void *)SPLIT_VERT);
//   // After creation, get the two panes:
//   window_t *left = splitview_get_left(sv);
//   window_t *right = splitview_get_right(sv);
//   // Or create with form children:
//   // The first two children of the splitview become left/right panes.

#include <stdlib.h>
#include <string.h>

#include <orion/user/user.h>
#include <orion/user/messages.h>
#include "commctl.h"

#define SPLITVIEW_MIN_PANE 32

typedef struct {
  int      orientation;   // SPLIT_VERT or SPLIT_HORZ
  double   split_ratio;   // 0.0 .. 1.0 — fraction of space given to left/top pane
  int      divider_w;     // splitter bar thickness in pixels (default 6)
  bool     dragging;      // true while the user is dragging the divider
  int      drag_start;    // mouse axis position when drag began
  double   drag_ratio;    // split_ratio when drag began
} splitview_state_t;

static bool splitview_is_splitter(window_t *win) {
  return win && win->proc == win_splitter;
}

static int splitview_orientation(void *lparam) {
  if (!lparam || (uintptr_t)lparam <= SPLIT_HORZ)
    return (int)(intptr_t)lparam;
  const form_ctrl_def_t *cd = (const form_ctrl_def_t *)lparam;
  return cd->lparam ? (int)(intptr_t)cd->lparam : SPLIT_VERT;
}

static void splitview_get_panes(window_t *win, window_t **left, window_t **right, window_t **splitter) {
  if (left) *left = NULL; if (right) *right = NULL; if (splitter) *splitter = NULL;
  int pane = 0;
  for (window_t *c = win ? win->children : NULL; c; c = c->next) {
    if (splitview_is_splitter(c)) { if (splitter) *splitter = c; continue; }
    if (pane == 0 && left) *left = c;
    if (pane == 1 && right) *right = c;
    pane++;
  }
}

static void splitview_arrange(splitview_state_t *st, window_t *win) {
  irect16_t cr = get_client_rect(win);
  int total, split_px;
  irect16_t r_left, r_split, r_right;

  if (st->orientation == SPLIT_VERT) {
    total = cr.w;
    int available = MAX(0, total - st->divider_w);
    split_px = (int)(available * st->split_ratio + 0.5);
    if (available >= SPLITVIEW_MIN_PANE * 2)
      split_px = MAX(SPLITVIEW_MIN_PANE, MIN(split_px, available - SPLITVIEW_MIN_PANE));
    else split_px = MAX(0, MIN(split_px, available));
    r_left  = (irect16_t){cr.x, cr.y, split_px, cr.h};
    r_split = (irect16_t){cr.x + split_px, cr.y, st->divider_w, cr.h};
    r_right = (irect16_t){cr.x + split_px + st->divider_w, cr.y,
                          total - split_px - st->divider_w, cr.h};
  } else {
    total = cr.h;
    int available = MAX(0, total - st->divider_w);
    split_px = (int)(available * st->split_ratio + 0.5);
    if (available >= SPLITVIEW_MIN_PANE * 2)
      split_px = MAX(SPLITVIEW_MIN_PANE, MIN(split_px, available - SPLITVIEW_MIN_PANE));
    else split_px = MAX(0, MIN(split_px, available));
    r_left  = (irect16_t){cr.x, cr.y, cr.w, split_px};
    r_split = (irect16_t){cr.x, cr.y + split_px, cr.w, st->divider_w};
    r_right = (irect16_t){cr.x, cr.y + split_px + st->divider_w,
                          cr.w, total - split_px - st->divider_w};
  }

  window_t *left = NULL, *right = NULL, *splitter = NULL;
  splitview_get_panes(win, &left, &right, &splitter);
  if (left)     left->frame = r_left;
  if (splitter) splitter->frame = r_split;
  if (right)    right->frame = r_right;

  window_t *children[] = { left, splitter, right };
  for (int i = 0; i < 3; i++) {
    if (children[i] && children[i]->proc) {
      layout_arrange_t la = {children[i]->frame};
      send_message(children[i], evArrange, 0, &la);
    }
  }
}

result_t win_splitview(window_t *win, uint32_t msg,
                       uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate: {
      splitview_state_t *st = allocate_window_data(win, sizeof(splitview_state_t));
      if (!st) return false;
      st->orientation = splitview_orientation(lparam);
      st->split_ratio = 0.5;
      st->divider_w = 6;
      st->dragging = false;

      // Create the splitter child.
      irect16_t r0 = {0, 0, 0, 0};
      create_window("", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_NOTABSTOP,
                    &r0, win, "Splitter",
                    win->hinstance, (void *)(intptr_t)st->orientation);
      return true;
    }

    case evArrange: {
      splitview_state_t *st = (splitview_state_t *)win->userdata;
      layout_arrange_t *a = (layout_arrange_t *)lparam;
      if (a) win->frame = a->rect;
      if (st) splitview_arrange(st, win);
      return true;
    }

    case evResize: {
      splitview_state_t *st = (splitview_state_t *)win->userdata;
      if (st) splitview_arrange(st, win);
      return true;
    }

    case evMeasure: {
      layout_measure_t *m = (layout_measure_t *)lparam;
      if (m) {
        m->desired_w = 200;
        m->desired_h = 200;
      }
      return true;
    }

    case evCommand: {
      splitview_state_t *st = (splitview_state_t *)win->userdata;
      if (!st) return false;
      uint16_t code = HIWORD(wparam);
      if (code == spnDragStart) {
        st->dragging = true;
        uint32_t packed = (uint32_t)(uintptr_t)lparam;
        st->drag_start = st->orientation == SPLIT_VERT
                           ? (int16_t)LOWORD(packed)
                           : (int16_t)HIWORD(packed);
        st->drag_ratio = st->split_ratio;
        set_capture(win);
        return true;
      }
      return false;
    }

    case evMouseMove: {
      splitview_state_t *st = (splitview_state_t *)win->userdata;
      if (!st || !st->dragging) return false;
      int pos = st->orientation == SPLIT_VERT
                  ? (int16_t)LOWORD(wparam)
                  : (int16_t)HIWORD(wparam);
      int delta = pos - st->drag_start;
      irect16_t cr = get_client_rect(win);
      int total = st->orientation == SPLIT_VERT ? cr.w : cr.h;
      if (total > 0)
        st->split_ratio = st->drag_ratio + (double)delta / total;
      if (st->split_ratio < 0.0) st->split_ratio = 0.0;
      if (st->split_ratio > 1.0) st->split_ratio = 1.0;
      splitview_arrange(st, win);
      invalidate_window(win);
      return true;
    }

    case evLeftButtonUp: {
      splitview_state_t *st = (splitview_state_t *)win->userdata;
      if (st && st->dragging) {
        st->dragging = false;
        set_capture(NULL);
        return true;
      }
      return false;
    }

    case evGetCursor:
      return curArrow;

    default:
      return false;
  }
}

window_t *splitview_get_left(window_t *win) {
  window_t *left = NULL;
  splitview_get_panes(win, &left, NULL, NULL);
  return left;
}

window_t *splitview_get_right(window_t *win) {
  window_t *right = NULL;
  splitview_get_panes(win, NULL, &right, NULL);
  return right;
}
