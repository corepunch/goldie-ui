#ifndef __UI_SCROLLBAR_H__
#define __UI_SCROLLBAR_H__

#include <stdbool.h>
#include <stdint.h>

#include "user.h"

bool scrollbar_handle_builtin_mouse(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
void scrollbar_handle_builtin_wheel(window_t *win, void *lparam);
void scrollbar_draw_statusbar_merged_hscroll(window_t *win, irect16_t row, int split_x);

#endif