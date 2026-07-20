#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commctl.h"
#include "../user/draw.h"
#include "../user/messages.h"
#include "../user/text.h"
#include "../user/theme.h"

#define ICON_BADGE_TEXT_MAX 15
#define ICON_ARTIFACT_LABEL_MAX 31
#define ICON_ARTIFACT_STRIP_W 16
#define ICON_ARTIFACT_SIZE 32
#define ICON_ARTIFACT_BADGE_SIZE 13
#define ICON_STATUS_SIZE 9
#define ICON_STATUS_GAP 4
#define ICON_DRAG_THRESHOLD 3

typedef struct {
  bool visible;
  char text[ICON_BADGE_TEXT_MAX + 1];
  uint32_t background, foreground;
  icon_badge_anchor_t anchor;
} icon_badge_state_t;

typedef struct {
  int id, count;
  icon_image_t image, count_badge;
  char label[ICON_ARTIFACT_LABEL_MAX + 1];
  void *item_data;
} icon_artifact_state_t;

typedef struct {
  icon_image_t image, status_image;
  void *item_data;
  window_t *notify_window, *artifact_ghost;
  bool draggable, drag_pending, dragging;
  int drag_x, drag_y;
  icon_badge_state_t badges[ICON_MAX_BADGES];
  int artifact_count, artifact_pending, artifact_dragging, artifact_grab_x, artifact_grab_y;
  icon_artifact_state_t artifacts[ICON_MAX_ARTIFACTS];
} icon_state_t;

static ipoint16_t desktop_icon_mouse_point(uint32_t packed) {
  return (ipoint16_t){(int16_t)LOWORD(packed), (int16_t)HIWORD(packed)};
}

static ipoint16_t desktop_icon_local_to_screen(window_t *win, ipoint16_t point) {
  if (!win) return (ipoint16_t){0, 0};
  return (ipoint16_t){
    (int16_t)(window_screen_x(win) + point.x - win->hscroll.pos),
    (int16_t)(window_screen_y(win) + point.y - win->vscroll.pos),
  };
}

static void desktop_icon_drag(window_t *win, icon_state_t *st, ipoint16_t point) {
  if (!st->dragging && abs(point.x - st->drag_x) < ICON_DRAG_THRESHOLD &&
                       abs(point.y - st->drag_y) < ICON_DRAG_THRESHOLD) return;
  st->dragging = true;
  int nx = win->frame.x + point.x - st->drag_x, ny = win->frame.y + point.y - st->drag_y;
  if (win->parent) {
    nx = MAX(0, MIN(nx, win->parent->frame.w - win->frame.w));
    ny = MAX(0, MIN(ny, win->parent->frame.h - win->frame.h));
  }
  move_window(win, nx, ny);
}

static result_t desktop_icon_notify_data(window_t *win, uint16_t code, void *data) {
  icon_state_t *st = (icon_state_t *)win->userdata2;
  window_t *target = st && st->notify_window && is_window(st->notify_window)
                     ? st->notify_window : (win->parent ? win->parent : get_root_window(win));
  return send_message(target, evCommand, MAKEDWORD(win->id, code), data);
}

static void desktop_icon_notify(window_t *win, uint16_t code) {
  desktop_icon_notify_data(win, code, win);
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
  int status_h = st->status_image.texture ? ICON_STATUS_SIZE : 0;
  int label_h = MAX(text_char_height(FONT_ICON), status_h);
  irect16_t area = rect_trim_bottom(rect_trim_right(get_client_rect(win),
                                  st->artifact_count ? ICON_ARTIFACT_STRIP_W : 0), label_h);
  int avail_w = MAX(1, area.w), avail_h = MAX(1, area.h);
  int w = avail_w, h = avail_h;
  if (st->image.width > 0 && st->image.height > 0) {
    float scale = MIN((float)avail_w / st->image.width, (float)avail_h / st->image.height);
    w = MAX(1, (int)(st->image.width * scale));
    h = MAX(1, (int)(st->image.height * scale));
  }
  return rect_center(area, w, h);
}

static irect16_t desktop_icon_artifact_rect(window_t *win, const icon_state_t *st, int index) {
  irect16_t image = desktop_icon_image_rect(win, st);
  int status_h = st->status_image.texture ? ICON_STATUS_SIZE : 0;
  int label_h = MAX(text_char_height(FONT_ICON), status_h);
  int area_h = MAX(ICON_ARTIFACT_SIZE, rect_trim_bottom(get_client_rect(win), label_h).h);
  int size = MIN(ICON_ARTIFACT_SIZE, MAX(16, area_h / MAX(1, st->artifact_count)));
  int total_h = st->artifact_count * size;
  int x = image.x + image.w - size / 2;
  int y = MAX(0, (area_h - total_h) / 2) + index * size;
  return R(x, y, size, size);
}

static int desktop_icon_artifact_at(window_t *win, const icon_state_t *st, ipoint16_t point) {
  for (int i = 0; i < st->artifact_count; i++) {
    irect16_t r = desktop_icon_artifact_rect(win, st, i);
    if (rect_contains_point(r, point)) return i;
  }
  return -1;
}

static window_t *desktop_icon_drop_target(window_t *win, ipoint16_t point) {
  if (!win->parent) return NULL;
  ipoint16_t parent_point = {(int16_t)(win->frame.x + point.x), (int16_t)(win->frame.y + point.y)};
  for (window_t *it = win->parent->children; it; it = it->next)
    if (it != win && it->proc == win_icon && rect_contains_point(it->frame, parent_point)) return it;
  return NULL;
}

static result_t desktop_icon_artifact_ghost_proc(window_t *win, uint32_t msg,
                                                 uint32_t wparam, void *lparam) {
  (void)wparam;
  icon_state_t *st = (icon_state_t *)win->userdata2;
  switch (msg) {
    case evCreate: win->userdata2 = lparam; win->flags |= WINDOW_NOTABSTOP; return true;
    case evPaint:
      if (st && st->artifact_pending >= 0 && st->artifact_pending < st->artifact_count) {
        icon_image_t image = st->artifacts[st->artifact_pending].image;
        draw_rect((int)image.texture, get_client_rect(win));
      }
      return true;
    default: return false;
  }
}

static void desktop_icon_artifact_hide_ghost(icon_state_t *st) {
  if (!st || !st->artifact_ghost) return;
  if (is_window(st->artifact_ghost)) destroy_window(st->artifact_ghost);
  st->artifact_ghost = NULL;
}

static void desktop_icon_artifact_update_ghost(window_t *win, icon_state_t *st, ipoint16_t point) {
  if (!st || st->artifact_pending < 0 || st->artifact_pending >= st->artifact_count) return;
  irect16_t slot = desktop_icon_artifact_rect(win, st, st->artifact_pending);
  ipoint16_t screen = desktop_icon_local_to_screen(win, point);
  if (!st->artifact_ghost) {
    st->artifact_ghost = create_window("",
      WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_ALWAYSONTOP | WINDOW_NOTRAYBUTTON |
      WINDOW_NOFILL | WINDOW_NOACTIVATE | WINDOW_TRANSPARENT,
      MAKERECT(screen.x - st->artifact_grab_x, screen.y - st->artifact_grab_y, slot.w, slot.h),
      NULL, desktop_icon_artifact_ghost_proc, win->hinstance, st);
    if (!st->artifact_ghost) return;
  }
  move_window(st->artifact_ghost, screen.x - st->artifact_grab_x, screen.y - st->artifact_grab_y);
  resize_window(st->artifact_ghost, slot.w, slot.h);
  show_window(st->artifact_ghost, true); invalidate_window(st->artifact_ghost);
}

static void desktop_icon_draw_badge(window_t *win, const icon_badge_state_t *badge,
                                    irect16_t image, int stack) {
  if (!badge->visible || !badge->text[0]) return;
  int h = text_char_height(FONT_ICON) + 4;
  int w = text_strwidth(FONT_ICON, badge->text) + 8;
  int x = badge->anchor == ICON_BADGE_TOP_CENTER ? image.x + (image.w - w) / 2
        : (badge->anchor == ICON_BADGE_TOP_LEFT || badge->anchor == ICON_BADGE_BOTTOM_LEFT)
          ? image.x - 2 : image.x + image.w - w + 2;
  int y = (badge->anchor == ICON_BADGE_TOP_LEFT || badge->anchor == ICON_BADGE_TOP_RIGHT ||
           badge->anchor == ICON_BADGE_TOP_CENTER)
          ? image.y + stack * (h + 2) - 2 : image.y + image.h - h - stack * (h + 2) + 2;
  x = MAX(0, MIN(x, win->frame.w - w));
  y = MAX(0, MIN(y, win->frame.h - h));
  irect16_t badge_rect = R(x, y, w, h);
  irect16_t text_rect = rect_trim_top(badge_rect, 2);
  fill_rect(get_sys_color(brTextNormal), rect_inset(badge_rect, -1));
  fill_rect(badge->background, badge_rect);
  draw_text_clipped(FONT_ICON, badge->text, &text_rect, badge->foreground, TEXT_ALIGN_CENTER);
}

static void desktop_icon_paint(window_t *win, const icon_state_t *st) {
  irect16_t local = get_client_rect(win);
  irect16_t image = desktop_icon_image_rect(win, st);
  int status_h = st->status_image.texture ? ICON_STATUS_SIZE : 0;
  int label_h = MAX(text_char_height(FONT_ICON), status_h) + 2;
  irect16_t content = rect_trim_right(local, st->artifact_count ? ICON_ARTIFACT_STRIP_W : 0);
  int content_w = MAX(1, content.w);
  irect16_t label = rect_split_bottom(content, label_h); label.w = content_w;
  uint32_t bg = get_sys_color(brWorkspaceBg);
  if (!(win->flags & WINDOW_TRANSPARENT)) fill_rect(bg, local);
  if (win->value) {
    uint32_t selection = get_sys_color(brActiveTitlebar);
    irect16_t border = rect_inset(local, 1);
    irect16_t sides = rect_trim_bottom(rect_trim_top(border, 2), 2);
    fill_rect(selection, rect_split_top(border, 2));    fill_rect(selection, rect_split_bottom(border, 2));
    fill_rect(selection, rect_split_left(sides, 2));    fill_rect(selection, rect_split_right(sides, 2));
  }
  if (st->image.texture) draw_rect((int)st->image.texture, image);
  uint32_t text_col = win->value ? get_sys_color(brFocusRing) : get_sys_color(brTextNormal);
  if (st->status_image.texture) {
    int text_w = text_strwidth(FONT_ICON, win->title);
    int status_w = ICON_STATUS_SIZE;
    int status_h = ICON_STATUS_SIZE;
    int group_w = status_w + ICON_STATUS_GAP + text_w;
    irect16_t group_area = label; group_area.w = MAX(group_area.w, group_w);
    irect16_t group = rect_center(group_area, group_w, label.h);
    irect16_t status = rect_center(rect_split_left(group, status_w), status_w, status_h);
    irect16_t status_label = rect_trim_left(group, status_w + ICON_STATUS_GAP);
    draw_rect((int)st->status_image.texture, status);
    draw_text_clipped(FONT_ICON, win->title, &status_label, text_col, TEXT_ALIGN_CENTER);
  } else draw_text_clipped(FONT_ICON, win->title, &label, text_col, TEXT_ALIGN_CENTER);
  int anchor_counts[5] = {0};
  for (int i = 0; i < ICON_MAX_BADGES; i++) {
    int anchor = st->badges[i].anchor;
    if (anchor < 0 || anchor > ICON_BADGE_TOP_CENTER) anchor = ICON_BADGE_TOP_RIGHT;
    desktop_icon_draw_badge(win, &st->badges[i], image, anchor_counts[anchor]++);
  }
  for (int i = 0; i < st->artifact_count; i++) {
    const icon_artifact_state_t *artifact = &st->artifacts[i];
    irect16_t r = desktop_icon_artifact_rect(win, st, i);
    if (i == st->artifact_pending && st->artifact_dragging) continue;
    draw_rect((int)artifact->image.texture, r);
    if (artifact->count > 1) {
      int bs = ICON_ARTIFACT_BADGE_SIZE;
      irect16_t badge = rect_offset(rect_split_bottom(rect_split_right(r, bs), bs), 2, 2);
      irect16_t count_rect = rect_offset(rect_trim_top(badge, 1), 1, 0);
      char count[2] = { artifact->count >= 10 ? '#' : (char)('0' + artifact->count), '\0' };
      draw_rect((int)artifact->count_badge.texture, badge);
      draw_text_clipped(FONT_ICON, count, &count_rect, 0xffffffff, TEXT_ALIGN_CENTER);
    }
  }
}

result_t win_icon(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  icon_state_t *st = (icon_state_t *)win->userdata2;
  switch (msg) {
    case evCreate: {
      st = calloc(1, sizeof(*st));
      if (!st) return false;
      st->artifact_pending = -1;
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
    case evDestroy: desktop_icon_artifact_hide_ghost(st); free(st); win->userdata2 = NULL; return true;
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
    case evLeftButtonDown: {
      ipoint16_t point = desktop_icon_mouse_point(wparam);
      window_set_state(win, WINDOW_STATE_PRESSED, true);
      desktop_icon_select(win, true, true);
      if (st) {
        st->artifact_pending = desktop_icon_artifact_at(win, st, point);
        st->artifact_dragging = false;
        if (st->artifact_pending >= 0) {
          irect16_t slot = desktop_icon_artifact_rect(win, st, st->artifact_pending);
          st->artifact_grab_x = point.x - slot.x; st->artifact_grab_y = point.y - slot.y;
          st->drag_x = point.x; st->drag_y = point.y; set_capture(win); invalidate_window(win); return true;
        }
      }
      if (st && st->draggable) {
        st->drag_pending = true; st->dragging = false;
        st->drag_x = point.x; st->drag_y = point.y;
        set_capture(win);
      }
      invalidate_window(win);
      return true;
    }
    case evMouseMove: {
      ipoint16_t point = desktop_icon_mouse_point(wparam);
      if (st && st->artifact_pending >= 0) {
        if (!st->artifact_dragging &&
            (abs(point.x - st->drag_x) >= ICON_DRAG_THRESHOLD || abs(point.y - st->drag_y) >= ICON_DRAG_THRESHOLD)) {
          st->artifact_dragging = true; invalidate_window(win);
        }
        if (st->artifact_dragging) desktop_icon_artifact_update_ghost(win, st, point);
        return true;
      }
      if (!st || !st->drag_pending) return false;
      desktop_icon_drag(win, st, point);
      return true;
    }
    case evLeftButtonUp: {
      if (st && st->artifact_pending >= 0) {
        int index = st->artifact_pending;
        ipoint16_t point = desktop_icon_mouse_point(wparam);
        window_t *target = st->artifact_dragging ? desktop_icon_drop_target(win, point) : NULL;
        bool accepted = false;
        desktop_icon_artifact_hide_ghost(st); set_capture(NULL);
        window_set_state(win, WINDOW_STATE_PRESSED, false);
        if (target) {
          icon_artifact_drop_t drop = { win, target, st->artifacts[index].id, st->artifacts[index].item_data };
          accepted = desktop_icon_notify_data(win, icnArtifactDrop, &drop) != 0;
        }
        // Accepted drops refresh the inventory synchronously. Rejected drops leave
        // it unchanged, so revealing the source slot makes the drag image snap back.
        st->artifact_pending = -1; st->artifact_dragging = false;
        if (!accepted) invalidate_window(win);
        return true;
      }
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
    case icSetStatusImage:
      if (!st) return false;
      st->status_image = lparam ? *(icon_image_t *)lparam : (icon_image_t){0};
      invalidate_window(win); return true;
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
    case icSetArtifacts: {
      if (!st || wparam > ICON_MAX_ARTIFACTS || (wparam && !lparam)) return false;
      desktop_icon_artifact_hide_ghost(st);
      memset(st->artifacts, 0, sizeof(st->artifacts)); st->artifact_count = (int)wparam;
      st->artifact_pending = -1; st->artifact_dragging = false;
      icon_artifact_t *src = (icon_artifact_t *)lparam;
      for (int i = 0; i < st->artifact_count; i++) {
        icon_artifact_state_t *dst = &st->artifacts[i];
        dst->id = src[i].id; dst->count = src[i].count; dst->image = src[i].image;
        dst->count_badge = src[i].count_badge; dst->item_data = src[i].item_data;
        if (src[i].label) { strncpy(dst->label, src[i].label, ICON_ARTIFACT_LABEL_MAX); dst->label[ICON_ARTIFACT_LABEL_MAX] = '\0'; }
      }
      invalidate_window(win); return true;
    }
    default: return false;
  }
}
