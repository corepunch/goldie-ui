#include <stdlib.h>
#include <string.h>

#include "commctl.h"
#include "../user/draw.h"
#include "../user/messages.h"
#include "../user/text.h"
#include "../user/theme.h"

#define ICON_PAD 4
#define ICON_LABEL_GAP 2
#define ICON_BADGE_TEXT_MAX 15
#define ICON_DRAG_THRESHOLD 3

typedef struct {
  bool visible;
  char text[ICON_BADGE_TEXT_MAX + 1];
  uint32_t background, foreground;
  icon_badge_anchor_t anchor;
} icon_badge_state_t;

typedef struct {
  icon_image_t image;
  void *item_data;
  window_t *notify_window;
  bool draggable, drag_pending, dragging;
  int drag_x, drag_y;
  icon_badge_state_t badges[ICON_MAX_BADGES];
} icon_state_t;

static int icon_mouse_coord(uint32_t packed, bool y) {
  return (int16_t)(y ? HIWORD(packed) : LOWORD(packed));
}

static void desktop_icon_drag(window_t *win, icon_state_t *st, uint32_t wparam) {
  int x = icon_mouse_coord(wparam, false), y = icon_mouse_coord(wparam, true);
  if (!st->dragging && abs(x - st->drag_x) < ICON_DRAG_THRESHOLD &&
                       abs(y - st->drag_y) < ICON_DRAG_THRESHOLD) return;
  st->dragging = true;
  int nx = win->frame.x + x - st->drag_x, ny = win->frame.y + y - st->drag_y;
  if (win->parent) {
    nx = MAX(0, MIN(nx, win->parent->frame.w - win->frame.w));
    ny = MAX(0, MIN(ny, win->parent->frame.h - win->frame.h));
  }
  move_window(win, nx, ny);
}

static void desktop_icon_notify(window_t *win, uint16_t code) {
  icon_state_t *st = (icon_state_t *)win->userdata2;
  window_t *target = st && st->notify_window && is_window(st->notify_window)
                     ? st->notify_window : (win->parent ? win->parent : get_root_window(win));
  send_message(target, evCommand, MAKEDWORD(win->id, code), win);
}

static void desktop_icon_select(window_t *win, bool selected, bool notify) {
  bool changed = (win->value != 0) != selected;
  if (selected && win->parent) {
    for (window_t *it = win->parent->children; it; it = it->next) {
      if (it != win && it->proc == win_icon && it->value) {
        it->value = false;
        invalidate_window(it);
      }
    }
  }
  win->value = selected;
  if (changed) {
    invalidate_window(win);
    if (notify) desktop_icon_notify(win, icnSelectionChange);
  }
}

static irect16_t desktop_icon_image_rect(window_t *win, const icon_state_t *st) {
  int label_h = text_char_height(FONT_ICON);
  int avail_w = MAX(1, win->frame.w - ICON_PAD * 2);
  int avail_h = MAX(1, win->frame.h - ICON_PAD * 2 - label_h - ICON_LABEL_GAP);
  int w = avail_w, h = avail_h;
  if (st->image.width > 0 && st->image.height > 0) {
    float scale = MIN((float)avail_w / st->image.width, (float)avail_h / st->image.height);
    w = MAX(1, (int)(st->image.width * scale));
    h = MAX(1, (int)(st->image.height * scale));
  }
  return R((win->frame.w - w) / 2, ICON_PAD + (avail_h - h) / 2, w, h);
}

static void desktop_icon_draw_badge(window_t *win, const icon_badge_state_t *badge,
                                    irect16_t image, int stack) {
  if (!badge->visible || !badge->text[0]) return;
  int h = text_char_height(FONT_ICON) + 4;
  int w = text_strwidth(FONT_ICON, badge->text) + 8;
  int x = (badge->anchor == ICON_BADGE_TOP_LEFT || badge->anchor == ICON_BADGE_BOTTOM_LEFT)
          ? image.x - 2 : image.x + image.w - w + 2;
  int y = (badge->anchor == ICON_BADGE_TOP_LEFT || badge->anchor == ICON_BADGE_TOP_RIGHT)
          ? image.y + stack * (h + 2) - 2 : image.y + image.h - h - stack * (h + 2) + 2;
  x = MAX(0, MIN(x, win->frame.w - w));
  y = MAX(0, MIN(y, win->frame.h - h));
  fill_rect(get_sys_color(brTextNormal), R(x - 1, y - 1, w + 2, h + 2));
  fill_rect(badge->background, R(x, y, w, h));
  draw_text_clipped(FONT_ICON, badge->text, &(irect16_t){x, y + 2, w, h - 2},
                    badge->foreground, TEXT_ALIGN_CENTER);
}

static void desktop_icon_paint(window_t *win, const icon_state_t *st) {
  irect16_t local = R(0, 0, win->frame.w, win->frame.h);
  irect16_t image = desktop_icon_image_rect(win, st);
  int label_h = text_char_height(FONT_ICON) + 2;
  irect16_t label = R(ICON_PAD, win->frame.h - ICON_PAD - label_h,
                      MAX(1, win->frame.w - ICON_PAD * 2), label_h);
  uint32_t bg = get_sys_color(brWorkspaceBg);
  if (!(win->flags & WINDOW_TRANSPARENT)) fill_rect(bg, local);
  if (win->value) {
    uint32_t selection = get_sys_color(brActiveTitlebar);
    fill_rect(selection, R(1, 1, win->frame.w - 2, 2));
    fill_rect(selection, R(1, win->frame.h - 3, win->frame.w - 2, 2));
    fill_rect(selection, R(1, 3, 2, win->frame.h - 6));
    fill_rect(selection, R(win->frame.w - 3, 3, 2, win->frame.h - 6));
  }
  if (st->image.texture) draw_rect((int)st->image.texture, image);
  uint32_t text_col = win->value ? get_sys_color(brFocusRing) : get_sys_color(brTextNormal);
  draw_text_clipped(FONT_ICON, win->title, &label, text_col, TEXT_ALIGN_CENTER);
  int anchor_counts[4] = {0};
  for (int i = 0; i < ICON_MAX_BADGES; i++) {
    int anchor = st->badges[i].anchor;
    if (anchor < 0 || anchor > ICON_BADGE_BOTTOM_RIGHT) anchor = ICON_BADGE_TOP_RIGHT;
    desktop_icon_draw_badge(win, &st->badges[i], image, anchor_counts[anchor]++);
  }
}

result_t win_icon(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  icon_state_t *st = (icon_state_t *)win->userdata2;
  switch (msg) {
    case evCreate: {
      st = calloc(1, sizeof(*st));
      if (!st) return false;
      win->userdata2 = st;
      win->flags |= WINDOW_NOTITLE | WINDOW_NORESIZE;
      if (lparam) {
        icon_params_t *params = (icon_params_t *)lparam;
        st->image = params->image;
        st->item_data = params->item_data;
        st->draggable = params->draggable;
        st->notify_window = params->notify_window;
      }
      return true;
    }
    case evDestroy: free(st); win->userdata2 = NULL; return true;
    case evPaint: if (st) desktop_icon_paint(win, st); return true;
    case evMeasure: {
      layout_measure_t *m = (layout_measure_t *)lparam;
      if (m) { m->desired_w = MAX(m->desired_w, 128); m->desired_h = MAX(m->desired_h, 128); }
      return true;
    }
    case evArrange: {
      layout_arrange_t *a = (layout_arrange_t *)lparam;
      if (a) win->frame = a->rect;
      return true;
    }
    case evLeftButtonDown:
      window_set_state(win, WINDOW_STATE_PRESSED, true);
      desktop_icon_select(win, true, true);
      if (st && st->draggable) {
        st->drag_pending = true; st->dragging = false;
        st->drag_x = icon_mouse_coord(wparam, false); st->drag_y = icon_mouse_coord(wparam, true);
        set_capture(win);
      }
      invalidate_window(win);
      return true;
    case evMouseMove:
      if (!st || !st->drag_pending) return false;
      desktop_icon_drag(win, st, wparam);
      return true;
    case evLeftButtonUp: {
      bool dragged = st && st->drag_pending && st->dragging;
      if (st && st->drag_pending) {
        st->drag_pending = false; st->dragging = false;
        set_capture(NULL);
      }
      window_set_state(win, WINDOW_STATE_PRESSED, false);
      invalidate_window(win);
      if (!dragged) desktop_icon_notify(win, icnClicked);
      return true;
    }
    case evLeftButtonDoubleClick:
      desktop_icon_select(win, true, true);
      desktop_icon_notify(win, icnOpen);
      return true;
    case evKeyDown:
      if (wparam == AX_KEY_SPACE) { desktop_icon_select(win, true, true); desktop_icon_notify(win, icnClicked); return true; }
      if (wparam == AX_KEY_ENTER) { desktop_icon_select(win, true, true); desktop_icon_notify(win, icnOpen); return true; }
      return false;
    case evGetTooltipText:
      if (lparam) { strncpy((char *)lparam, win->title, 255); ((char *)lparam)[255] = '\0'; return true; }
      return false;
    case icSetImage:
      if (!st || !lparam) return false;
      st->image = *(icon_image_t *)lparam; invalidate_window(win); return true;
    case icSetBadge: {
      if (!st || wparam >= ICON_MAX_BADGES) return false;
      icon_badge_state_t *dst = &st->badges[wparam];
      memset(dst, 0, sizeof(*dst));
      if (lparam) {
        icon_badge_t *src = (icon_badge_t *)lparam;
        dst->visible = true; dst->background = src->background;
        dst->foreground = src->foreground; dst->anchor = src->anchor;
        if (src->text) { strncpy(dst->text, src->text, ICON_BADGE_TEXT_MAX); dst->text[ICON_BADGE_TEXT_MAX] = '\0'; }
      }
      invalidate_window(win); return true;
    }
    case icClearBadges:
      if (!st) return false;
      memset(st->badges, 0, sizeof(st->badges)); invalidate_window(win); return true;
    case icSetSelected: desktop_icon_select(win, wparam != 0, false); return true;
    case icGetSelected: return win->value != 0;
    case icSetItemData: if (st) st->item_data = lparam; return st != NULL;
    case icGetItemData: return st ? (result_t)st->item_data : 0;
    default: return false;
  }
}
