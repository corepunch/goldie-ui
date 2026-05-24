#include <string.h>

#include "user.h"
#include "messages.h"
#include "draw.h"

static window_t *g_drag_item_win = NULL;
static window_t *g_drag_item_target = NULL;
static ui_drag_item_payload_t g_drag_item_payload = {0};

#define DRAG_ITEM_PAD_X    4
#define DRAG_ITEM_PAD_Y    2
#define DRAG_ITEM_OFFSET_X 14
#define DRAG_ITEM_OFFSET_Y 14

static lresult_t ui_drag_item_win_proc(window_t *win, uint32_t msg,
                                       uint32_t wparam, void *lparam) {
  (void)wparam;
  (void)lparam;
  switch (msg) {
    case evPaint:
      fill_rect(0xFFE7E7E7, R(0, 0, win->frame.w, win->frame.h));
      fill_rect(0xFF202020, R(0, 0, win->frame.w, 1));
      fill_rect(0xFF202020, R(0, win->frame.h - 1, win->frame.w, 1));
      fill_rect(0xFF202020, R(0, 0, 1, win->frame.h));
      fill_rect(0xFF202020, R(win->frame.w - 1, 0, 1, win->frame.h));
      draw_text(FONT_SMALL, win->title, DRAG_ITEM_PAD_X, DRAG_ITEM_PAD_Y,
                0xFF000000);
      return true;
    default:
      return default_winproc(win, msg, wparam, lparam);
  }
}

static void ui_drag_item_measure_rect(const char *text, int sx, int sy,
                                      irect16_t *out) {
  int w = text_strwidth(FONT_SMALL, text) + DRAG_ITEM_PAD_X * 2;
  int h = text_char_height(FONT_SMALL) + DRAG_ITEM_PAD_Y * 2;
  int x = sx + DRAG_ITEM_OFFSET_X;
  int y = sy + DRAG_ITEM_OFFSET_Y;
  int sw = ui_get_system_metrics(kSystemMetricScreenWidth);
  int sh = ui_get_system_metrics(kSystemMetricScreenHeight);
  if (x + w > sw) x = sw - w;
  if (y + h > sh) y = sh - h;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  *out = (irect16_t){(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h};
}

static window_t *ui_drag_item_pick_target(int sx, int sy) {
  window_t *hit = find_window(sx, sy);
  window_t *target = hit ? get_root_window(hit) : NULL;
  if (!target)
    target = g_ui_runtime.focused ? get_root_window(g_ui_runtime.focused) : NULL;
  return target;
}

static void ui_drag_item_notify_target(window_t *target, uint32_t msg, int sx, int sy) {
  if (!target)
    return;
  ipoint16_t client = window_client_origin_xy(target);
  int local_x = sx - client.x + target->hscroll.pos;
  int local_y = sy - client.y + target->vscroll.pos;
  send_message(target, msg,
               MAKEDWORD((uint16_t)local_x, (uint16_t)local_y),
               &g_drag_item_payload);
}

void ui_drag_item_set(const char *text, const ui_drag_item_payload_t *payload) {
  if (!text || !text[0]) {
    ui_drag_item_clear();
    return;
  }

  if (payload)
    g_drag_item_payload = *payload;
  else
    g_drag_item_payload = (ui_drag_item_payload_t){0};
  g_drag_item_target = NULL;

  irect16_t r;
  ui_drag_item_measure_rect(text, g_ui_runtime.mouse_x, g_ui_runtime.mouse_y, &r);

  if (!g_drag_item_win) {
    hinstance_t owner_hinst = 0;
    if (g_ui_runtime.focused) {
      window_t *owner_root = get_root_window(g_ui_runtime.focused);
      if (owner_root)
        owner_hinst = owner_root->hinstance;
    }
    g_drag_item_win = create_window(
        "", WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_ALWAYSONTOP |
            WINDOW_NOTRAYBUTTON | WINDOW_NOACTIVATE,
        &r, NULL, ui_drag_item_win_proc, owner_hinst, NULL);
    if (!g_drag_item_win)
      return;
  } else {
    g_drag_item_win->frame.w = r.w;
    g_drag_item_win->frame.h = r.h;
    move_window(g_drag_item_win, r.x, r.y);
  }

  snprintf(g_drag_item_win->title, sizeof(g_drag_item_win->title), "%s", text);
  show_window(g_drag_item_win, true);
  invalidate_window(g_drag_item_win);
}

void ui_drag_item_move(int sx, int sy) {
  window_t *target;

  if (!g_drag_item_win)
    return;
  irect16_t r;
  ui_drag_item_measure_rect(g_drag_item_win->title, sx, sy, &r);
  if (g_drag_item_win->frame.w != r.w || g_drag_item_win->frame.h != r.h) {
    g_drag_item_win->frame.w = r.w;
    g_drag_item_win->frame.h = r.h;
  }
  move_window(g_drag_item_win, r.x, r.y);

  target = ui_drag_item_pick_target(sx, sy);
  if (target != g_drag_item_target) {
    ui_drag_item_notify_target(g_drag_item_target, evMouseDragLeave, sx, sy);
    g_drag_item_target = target;
    ui_drag_item_notify_target(g_drag_item_target, evMouseDragEnter, sx, sy);
  }
  ui_drag_item_notify_target(g_drag_item_target, evMouseDrag, sx, sy);
}

void ui_drag_item_clear(void) {
  if (!g_drag_item_win)
    return;
  ui_drag_item_notify_target(g_drag_item_target, evMouseDrop,
                             g_ui_runtime.mouse_x, g_ui_runtime.mouse_y);
  window_t *drag_win = g_drag_item_win;
  g_drag_item_win = NULL;
  g_drag_item_target = NULL;
  destroy_window(drag_win);
  g_drag_item_payload = (ui_drag_item_payload_t){0};
}
