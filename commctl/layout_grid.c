#include "../user/user.h"
#include "../user/messages.h"
#include "commctl.h"

extern result_t layout_container_proc(window_t *win, uint32_t msg,
                                      uint32_t wparam, void *lparam,
                                      const char *default_layout_kind,
                                      flags_t default_orientation,
                                      uint8_t default_spacing);
extern void window_layout_sync(window_t *win);

static result_t layout_init_default_grid_columns(window_t *win) {
  if (!win)
    return true;

  window_t *col = NULL;
  if (!win->children) {
    layout_view_config_t cfg = {
      .layout_kind = "stack",
      .orientation = WINDOW_STACK_VERTICAL,
      .spacing = 4,
      .padding = (irect16_t){0, 0, 0, 0},
      .margin = (irect16_t){0, 0, 0, 0},
    };

    for (int i = 0; i < 2; i++) {
      irect16_t frame = {0, 0, 0, 0};
      col = create_window("",
          WINDOW_NOTITLE | WINDOW_NOFILL,
          &frame,
          win, win_column, win->hinstance, &cfg);
      if (col)
        col->id = (uint32_t)(i + 1);
    }
  }
  send_message(win, evResize, 0, NULL);
  return true;
}

result_t win_gridview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate: {
      result_t r = layout_container_proc(win, msg, wparam, lparam,
                                         "grid",
                                         WINDOW_STACK_VERTICAL,
                                         0);
      if (r)
        send_message(win, evInitChildren, 0, NULL);
      return r;
    }
    case evInitChildren:
      return layout_init_default_grid_columns(win);
    default:
      return layout_container_proc(win, msg, wparam, lparam,
                                   "grid",
                                   WINDOW_STACK_VERTICAL,
                                   0);
  }
}
