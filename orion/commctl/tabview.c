// Win32-style tab control. Direct children are pages; each child's title is
// used as its tab label and only the selected page is visible.
//
// Icons are set per-tab via tcSetImageStrip (bitmap_strip_t*) + tcSetTabIcon
// (tab_index, icon_index).  When icons are present the tab header height
// grows to MAX(TAB_CONTROL_HEIGHT, icon_h + 4).

#include <orion/user/user.h>
#include <orion/user/messages.h>
#include <orion/user/draw.h>
#include "commctl.h"

extern void draw_bevel(irect16_t r);

typedef struct {
  int selected;
  bitmap_strip_t strip;
  int *tab_icons;
  int tab_icon_count;
} tabview_state_t;

static int tab_count(window_t *win) {
  int n = 0; for (window_t *c = win ? win->children : NULL; c; c = c->next) n++;
  return n;
}

static int tab_header_height(tabview_state_t *st) {
  return (st && st->strip.cols > 0) ? MAX(TAB_CONTROL_HEIGHT, st->strip.icon_h + 4) : TAB_CONTROL_HEIGHT;
}

static bool tab_has_icon(tabview_state_t *st, int idx) {
  return st && st->tab_icons && idx >= 0 && idx < st->tab_icon_count
         && st->tab_icons[idx] >= 0 && st->strip.cols > 0;
}

static int tab_icon_gap(tabview_state_t *st) {
  (void)st; return 3;
}

static int tab_width(window_t *page, tabview_state_t *st, int idx) {
  int w = MAX(48, strwidth(page ? page->title : "") + 18);
  if (tab_has_icon(st, idx)) w += st->strip.icon_w + tab_icon_gap(st);
  return w;
}

static void draw_tab_icon(tabview_state_t *st, int idx, int x, int y, int h) {
  if (!tab_has_icon(st, idx)) return;
  int ic = st->tab_icons[idx];
  int col = ic % st->strip.cols;
  int row = ic / st->strip.cols;
  float u0 = (float)(col * st->strip.icon_w) / (float)st->strip.sheet_w;
  float v0 = (float)(row * st->strip.icon_h) / (float)st->strip.sheet_h;
  float u1 = u0 + (float)st->strip.icon_w / (float)st->strip.sheet_w;
  float v1 = v0 + (float)st->strip.icon_h / (float)st->strip.sheet_h;
  int iy = y + (h - st->strip.icon_h) / 2;
  draw_sprite_region((int)st->strip.tex, R(x, iy, st->strip.icon_w, st->strip.icon_h),
                     UV_RECT(u0, v0, u1, v1), 0xFFFFFFFF, 0);
}

static void draw_tab(irect16_t r) {
  fill_rect(get_sys_color(brControlBg), r);
  fill_rect(get_sys_color(brLightEdge), R(r.x, r.y, r.w - 1, 1));
  fill_rect(get_sys_color(brLightEdge), R(r.x, r.y, 1, r.h));
  fill_rect(get_sys_color(brDarkEdge), R(r.x + r.w - 1, r.y + 1, 1, r.h - 1));
  fill_rect(get_sys_color(brFlare), R(r.x, r.y, 1, 1));
}

static void draw_tab_item(window_t *page, int x, bool selected, tabview_state_t *st, int idx) {
  int w = tab_width(page, st, idx), y = selected ? 0 : 2;
  int th = tab_header_height(st);
  int h = th - y - 2;
  draw_tab(R(x, y, w, h));
  if (selected) fill_rect(get_sys_color(brControlBg), R(x + 2, th - 2, w - 4, 2));
  bool has_icon = tab_has_icon(st, idx);
  int sw = strwidth(page->title), iw = has_icon ? st->strip.icon_w + tab_icon_gap(st) : 0;
  int content_w = iw + sw;
  int cx = x + (w - content_w) / 2;
  if (has_icon) { draw_tab_icon(st, idx, cx, y, h); cx += iw; }
  draw_text_small(page->title, cx, y + (h - CHAR_HEIGHT) / 2, get_sys_color(brTextNormal));
}

static void tab_arrange(window_t *win) {
  tabview_state_t *st = (tabview_state_t *)win->userdata;
  if (!st) return;
  int count = tab_count(win);
  if (st->selected >= count) st->selected = MAX(0, count - 1);
  irect16_t cr = get_client_rect(win);
  int th = tab_header_height(st);
  irect16_t page_rect = rect_trim_top(cr, th);
  page_rect.x += 2; page_rect.w = MAX(0, page_rect.w - 4);
  page_rect.h = MAX(0, page_rect.h - 2);
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
  fill_rect(get_sys_color(brControlBg), cr);
  if (!st) return;

  int selected_x = 2;
  window_t *selected = NULL;
  int x = 2, i = 0;
  for (window_t *c = win->children; c; c = c->next, i++) {
    if (i == st->selected) { selected = c; selected_x = x; }
    else draw_tab_item(c, x, false, st, i);
    x += tab_width(c, st, i) + 1;
  }

  int th = tab_header_height(st);
  irect16_t page = rect_trim_top(cr, th - 1);
  draw_bevel(page);
  if (selected) draw_tab_item(selected, selected_x, true, st, st->selected);
  if (selected) send_message(selected, evPaint, 0, NULL);
}

static void tab_cleanup_icons(tabview_state_t *st) {
  if (st->tab_icons) { free(st->tab_icons); st->tab_icons = NULL; }
  st->tab_icon_count = 0;
  memset(&st->strip, 0, sizeof(st->strip));
}

static bool tab_set_tab_icon(tabview_state_t *st, int count, int idx, int ico) {
  if (idx < 0 || idx >= count) return false;
  if (!st->tab_icons) {
    st->tab_icon_count = count;
    st->tab_icons = calloc((size_t)count, sizeof(int));
    if (!st->tab_icons) return false;
    for (int j = 0; j < count; j++) st->tab_icons[j] = -1;
  } else if (idx >= st->tab_icon_count) {
    int old = st->tab_icon_count;
    st->tab_icon_count = idx + 1;
    st->tab_icons = realloc(st->tab_icons, (size_t)st->tab_icon_count * sizeof(int));
    if (!st->tab_icons) return false;
    for (int j = old; j < st->tab_icon_count; j++) st->tab_icons[j] = -1;
  }
  st->tab_icons[idx] = ico;
  return true;
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
      if (!st) return false;
      int mx = (int16_t)LOWORD(wparam), my = (int16_t)HIWORD(wparam);
      int th = tab_header_height(st);
      if (my < 0 || my >= th) return false;
      int left = 2, i = 0;
      for (window_t *c = win->children; c; c = c->next, i++) {
        int w = tab_width(c, st, i);
        if (mx >= left && mx < left + w) { set_focus(win); tab_select(win, i, true); return true; }
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
    case tcSetImageStrip: {
      if (!st || !lparam) return false;
      memcpy(&st->strip, lparam, sizeof(bitmap_strip_t));
      tab_arrange(win); invalidate_window(win);
      return true;
    }
    case tcSetTabIcon: {
      if (!st) return false;
      int idx = (int)wparam, ico = (int)(intptr_t)lparam;
      if (!tab_set_tab_icon(st, tab_count(win), idx, ico)) return false;
      tab_arrange(win); invalidate_window(win);
      return true;
    }
    case evParentNotify: return win->parent ? send_message(win->parent, msg, wparam, lparam) : false;
    case evDestroy:
      if (st) tab_cleanup_icons(st);
      return true;
    default: return false;
  }
}
