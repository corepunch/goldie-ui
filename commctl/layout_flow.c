#include "../user/user.h"
#include "commctl.h"

extern result_t layout_container_proc(window_t *win, uint32_t msg,
                                      uint32_t wparam, void *lparam,
                                      const char *default_layout_kind,
                                      flags_t default_orientation,
                                      uint8_t default_spacing);

result_t win_flowview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  return layout_container_proc(win, msg, wparam, lparam,
                               "flow",
                               WINDOW_STACK_HORIZONTAL,
                               0);
}
