// Win32-style tab control. Direct children are pages; each child's title is
// used as its tab label and only the selected page is visible.

#include <orion/user/user.h>
#include <orion/user/messages.h>
#include <orion/user/draw.h>
#include "commctl.h"

extern void draw_bevel(irect16_t r);

typedef struct { int selected; } tabview_state_t;

static int tab_count(window_t *win) {
  int n = 0; for (window_t *c = win ? win->children : NULL; c; c = c->next) n++;
  return n;
}

static window_t *tab_page(window_t *win, int index) {
  window_t *c = win ? win->children : NULL;
  for (int i = 0; c && i < index; i++) c = c->next;
  return c;
}

static int tab_width(window_t *page) { return MAX(48, strwidth(page ? page->title : "") + 18); }

static void draw_tab(irect16_t r) {
  fill_rect(get_sys_color(brButtonBg), r);
  fill_rect(get_sys_color(brLightEdge), R(r.x, r.y, r.w - 1, 1));
  fill_rect(get_sys_color(brLightEdge), R(r.x, r.y, 1, r.h));
  fill_rect(get_sys_color(brDarkEdge), R(r.x + r.w - 1, r.y + 1, 1, r.h - 1));
  fill_rect(get_sys_color(brFlare), R(r.x, r.y, 1, 1));
}

static void tab_arrange(window_t *win) {
  tabview_state_t *st = (tabview_state_t *)win->userdata;
  if (!st) return;
  int count = tab_count(win);
  if (st->selected >= count) st->selected = MAX(0, count - 1);
  irect16_t cr = get_client_rect(win);
  irect16_t page_rect = {cr.x + 2, cr.y + TAB_CONTROL_HEIGHT,
                         MAX(0, cr.w - 4), MAX(0, cr.h - TAB_CONTROL_HEIGHT - 2)};
  int i = 0;
  for (window_t *c = win->children; c; c = c->next, i++) {
    bool visible = i == st->selected;
    window_set_state(c, WINDOW_STATE_VISIBLE, visible);
    c->frame = page_rect;
    if (visible) { layout_arrange_t a = {page_rect}; send_message(c, evArrange, 0, &a); }
  }
}

static bool tab_select(window_t *win, int index, bool notify) {
  tabview_state_t *st = (tabview_state_t *)win->userdata;
  int count = tab_count(win);
  if (!st || index < 0 || index >= count || index == st->selected) return false;
  st->selected = index;
  tab_arrange(win);
  post_message(win, evRefreshStencil, 0, NULL);
  invalidate_window(win);
  if (notify && win->parent)
    send_message(win->parent, evCommand, MAKEDWORD((uint16_t)win->id, tcnSelChange), win);
  return true;
}

static void tab_paint(window_t *win) {
  tabview_state_t *st = (tabview_state_t *)win->userdata;
  irect16_t cr = get_client_rect(win);
  fill_rect(get_sys_color(brWindowBg), cr);
  if (!st) return;

  irect16_t page = {0, TAB_CONTROL_HEIGHT - 1, cr.w, MAX(1, cr.h - TAB_CONTROL_HEIGHT + 1)};
  draw_bevel(page);
  int x = 2, i = 0;
  for (window_t *c = win->children; c; c = c->next, i++) {
    bool selected = i == st->selected;
    int w = tab_width(c), y = selected ? 0 : 2;
    int h = TAB_CONTROL_HEIGHT - y;
    draw_tab(R(x, y, w, h));
    if (selected) fill_rect(get_sys_color(brButtonBg), R(x + 2, TAB_CONTROL_HEIGHT - 2, w - 4, 2));
    int tx = x + (w - strwidth(c->title)) / 2;
    int ty = y + (h - CHAR_HEIGHT) / 2;
    draw_text_small(c->title, tx, ty, get_sys_color(brTextNormal));
    x += w + 1;
  }
  window_t *selected = tab_page(win, st->selected);
  if (selected) send_message(selected, evPaint, 0, NULL);
}

result_t win_tabview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  tabview_state_t *st = (tabview_state_t *)win->userdata;
  switch (msg) {
    case evCreate:
      st = allocate_window_data(win, sizeof(*st));
      if (!st) return false;
      st->selected = 0;
      return true;
    case evMeasure: {
      layout_measure_t *m = (layout_measure_t *)lparam;
      if (m) { m->desired_w = 200; m->desired_h = 200; }
      return true;
    }
    case evArrange: {
      layout_arrange_t *a = (layout_arrange_t *)lparam;
      if (a) win->frame = a->rect;
      tab_arrange(win);
      return true;
    }
    case evResize: tab_arrange(win); return true;
    case evPaint: tab_paint(win); return true;
    case evLeftButtonDown: {
      int x = (int16_t)LOWORD(wparam), y = (int16_t)HIWORD(wparam);
      if (y < 0 || y >= TAB_CONTROL_HEIGHT) return false;
      int left = 2, i = 0;
      for (window_t *c = win->children; c; c = c->next, i++) {
        int w = tab_width(c);
        if (x >= left && x < left + w) { set_focus(win); tab_select(win, i, true); return true; }
        left += w + 1;
      }
      return true;
    }
    case evKeyDown:
      if (!st) return false;
      if (wparam == AX_KEY_LEFTARROW)  return tab_select(win, st->selected - 1, true);
      if (wparam == AX_KEY_RIGHTARROW) return tab_select(win, st->selected + 1, true);
      return false;
    case tcGetSelection: return st ? st->selected : -1;
    case tcSetSelection: return tab_select(win, (int)wparam, false) || (st && st->selected == (int)wparam);
    case evParentNotify: return win->parent ? send_message(win->parent, msg, wparam, lparam) : false;
    default: return false;
  }
}
