#ifndef __UI_DRAW_H__
#define __UI_DRAW_H__

#include <stdint.h>
#include "../user/user.h"
#include "text.h"

// Rectangle drawing functions
void fill_rect(int color, int x, int y, int w, int h);
void draw_rect(int tex, int x, int y, int w, int h);
void draw_rect_ex(int tex, int x, int y, int w, int h, int type, float alpha);

// Icon drawing functions
void draw_icon8(int icon, int x, int y, uint32_t col);
void draw_icon16(int icon, int x, int y, uint32_t col);

// Viewport and projection
void set_viewport(window_t const *win);
void set_projection(int x, int y, int w, int h);

// Rendering context functions (abstraction over OpenGL)
void ui_begin_frame(void);
void ui_end_frame(void);
void ui_clear_screen(float r, float g, float b);
void ui_swap_buffers(void);

// Stencil management (internal use)
void ui_set_stencil_for_window(uint32_t window_id);
void ui_set_stencil_for_root_window(uint32_t window_id);

#endif
