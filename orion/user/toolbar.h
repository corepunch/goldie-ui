#ifndef __UI_TOOLBAR_H__
#define __UI_TOOLBAR_H__

#include <stdbool.h>
#include <stdint.h>

#include "user.h"

toolbar_state_t *toolbar_ensure_state(window_t *win);
toolbar_state_t *toolbar_get_state(window_t *win);
int toolbar_effective_bsz(window_t const *win);
int toolbar_effective_item_height(window_t const *win);

void toolbar_draw_non_client(window_t *win);

bool toolbar_handle_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

bool toolbar_handle_notitle_nc_left_button_up(window_t *win, uint32_t wparam);

bool toolbar_dispatch_embedded_mouse(window_t *parent, uint32_t msg, int tb_x, int tb_y);

int toolbar_item_hit(const toolbar_state_t *tb, int tx, int ty);

#endif
