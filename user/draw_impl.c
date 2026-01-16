// Drawing primitives implementation
// Extracted from mapview/window.c

#include <SDL2/SDL.h>
#include "gl_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "user.h"
#include "messages.h"
#include "draw.h"

// External references
extern window_t *windows;
extern window_t *_focused;
extern SDL_Window *window;

// Forward declarations
extern void draw_text_small(const char* text, int x, int y, uint32_t col);
extern void draw_icon8(int icon, int x, int y, uint32_t col);
extern void draw_icon16(int icon, int x, int y, uint32_t col);
extern int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern void set_projection(int x, int y, int w, int h);

rect_t get_opengl_rect(rect_t const *r) {
  int w, h;
  SDL_GL_GetDrawableSize(window, &w, &h);

  float scale_x = (float)w / MAX(1,ui_get_system_metrics(SM_CXSCREEN));
  float scale_y = (float)h / MAX(1,ui_get_system_metrics(SM_CYSCREEN));
  
  return (rect_t){
    (int)(r->x * scale_x),
    (int)((ui_get_system_metrics(SM_CYSCREEN) - r->y - r->h) * scale_y), // flip Y
    (int)(r->w * scale_x),
    (int)(r->h * scale_y)
  };
}

// Get titlebar height
int titlebar_height(window_t const *win) {
  int t = 0;
  if (!(win->flags&WINDOW_NOTITLE)) {
    t += TITLEBAR_HEIGHT;
  }
  if (win->flags&WINDOW_TOOLBAR) {
    t += TOOLBAR_HEIGHT;
  }
  return t;
}

// Draw focused border
void draw_focused(rect_t const *r) {
  fill_rect(COLOR_FOCUSED, r->x-1, r->y-1, r->w+2, 1);
  fill_rect(COLOR_FOCUSED, r->x-1, r->y-1, 1, r->h+2);
  fill_rect(COLOR_FOCUSED, r->x+r->w, r->y, 1, r->h+1);
  fill_rect(COLOR_FOCUSED, r->x, r->y+r->h, r->w+1, 1);
}

// Draw bevel border
void draw_bevel(rect_t const *r) {
  fill_rect(COLOR_LIGHT_EDGE, r->x-1, r->y-1, r->w+2, 1);
  fill_rect(COLOR_LIGHT_EDGE, r->x-1, r->y-1, 1, r->h+2);
  fill_rect(COLOR_DARK_EDGE, r->x+r->w, r->y, 1, r->h+1);
  fill_rect(COLOR_DARK_EDGE, r->x, r->y+r->h, r->w+1, 1);
  fill_rect(COLOR_FLARE, r->x-1, r->y-1, 1, 1);
}

// Draw button
void draw_button(rect_t const *r, int dx, int dy, bool pressed) {
  fill_rect(pressed?COLOR_DARK_EDGE:COLOR_LIGHT_EDGE, r->x-dx, r->y-dy, r->w+dx+dy, r->h+dx+dy);
  fill_rect(pressed?COLOR_LIGHT_EDGE:COLOR_DARK_EDGE, r->x, r->y, r->w+dx, r->h+dy);
  fill_rect(pressed?COLOR_PANEL_DARK_BG:COLOR_PANEL_BG, r->x, r->y, r->w, r->h);
  if (pressed) {
    fill_rect(COLOR_FLARE, r->x+r->w, r->y+r->h, dx, dy);
  } else {
    fill_rect(COLOR_FLARE, r->x-dx, r->y-dy, dx, dy);
  }
}

// Draw window panel
void draw_panel(window_t const *win) {
  int t = titlebar_height(win);
  int x = win->frame.x, y = win->frame.y-t;
  int w = win->frame.w, h = win->frame.h+t;
  bool active = _focused == win;
  if (active) {
    draw_focused(MAKERECT(x, y, w, h));
  } else {
    draw_bevel(MAKERECT(x, y, w, h));
  }
  if (!(win->flags & WINDOW_NORESIZE)) {
    int r = RESIZE_HANDLE;
    fill_rect(COLOR_LIGHT_EDGE, x+w, y+h-r+1, 1, r);
    fill_rect(COLOR_LIGHT_EDGE, x+w-r+1, y+h, r, 1);
  }
  if (!(win->flags&WINDOW_NOFILL)) {
    fill_rect(COLOR_PANEL_BG, x, y, w, h);
  }
}

// Draw window controls (close, minimize, etc.)
void draw_window_controls(window_t *win) {
  rect_t r = win->frame;
  int t = titlebar_height(win);
  fill_rect(COLOR_PANEL_DARK_BG, r.x, r.y-t, r.w, t);
  set_viewport(&(rect_t){0, 0, ui_get_system_metrics(SM_CXSCREEN), ui_get_system_metrics(SM_CYSCREEN)});
  set_projection(0, 0, ui_get_system_metrics(SM_CXSCREEN), ui_get_system_metrics(SM_CYSCREEN));
  
  for (int i = 0; i < 1; i++) {
    int x = win->frame.x + win->frame.w - (i+1)*CONTROL_BUTTON_WIDTH - CONTROL_BUTTON_PADDING;
    int y = window_title_bar_y(win);
    draw_icon8(icon8_minus + i, x, y, COLOR_TEXT_NORMAL);
  }
}

// Set OpenGL viewport for window
void set_viewport(rect_t const *frame) {
  int w, h;
  SDL_GL_GetDrawableSize(window, &w, &h);
  
  rect_t ogl_rect = get_opengl_rect(frame);
  
  glEnable(GL_SCISSOR_TEST);
  glViewport(ogl_rect.x, ogl_rect.y, ogl_rect.w, ogl_rect.h);
  glScissor(ogl_rect.x, ogl_rect.y, ogl_rect.w, ogl_rect.h);
}

// Paint window to stencil buffer
void paint_window_stencil(window_t const *w) {
  int p = 1;
  int t = titlebar_height(w);
  glStencilFunc(GL_ALWAYS, w->id, 0xFF);            // Always pass
  glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE); // Replace stencil with window ID
  draw_rect(1, w->frame.x-p, w->frame.y-t-p, w->frame.w+p*2, w->frame.h+t+p*2);
}

// Repaint window stencil buffer
void repaint_stencil(void) {
  set_viewport(&(rect_t){0, 0, ui_get_system_metrics(SM_CXSCREEN), ui_get_system_metrics(SM_CYSCREEN)});
  set_projection(0, 0, ui_get_system_metrics(SM_CXSCREEN), ui_get_system_metrics(SM_CYSCREEN));
  
  glEnable(GL_STENCIL_TEST);
  glClearStencil(0);
  glClear(GL_STENCIL_BUFFER_BIT);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  for (window_t *w = windows; w; w = w->next) {
    if (!w->visible)
      continue;
    send_message(w, WM_PAINTSTENCIL, 0, NULL);
  }
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}

// Set stencil test to render for specific window
void ui_set_stencil_for_window(uint32_t window_id) {
  glStencilFunc(GL_EQUAL, window_id, 0xFF);
}

// Set stencil test to render for root window
void ui_set_stencil_for_root_window(uint32_t window_id) {
  glStencilFunc(GL_EQUAL, window_id, 0xFF);
}

// Fill a rectangle with a solid color
void fill_rect(int color, int x, int y, int w, int h) {
  extern bool running;
  extern GLuint ui_white_texture;
  
  // Skip drawing if graphics aren't initialized (e.g., in tests)
  if (!running) return;
  
  // Update the white texture with the desired color
  glBindTexture(GL_TEXTURE_2D, ui_white_texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &color);
  
  // Draw a rectangle using the texture
  draw_rect_ex(ui_white_texture, x, y, w, h, false, 1);
}

void draw_icon8(int icon, int x, int y, uint32_t col) {
  char str[2] = { icon+128+6*16, 0 };
  draw_text_small(str, x, y, col);
}

void draw_icon16(int icon, int x, int y, uint32_t col) {
  icon*=2;
  char str[6] = { icon+128, icon+129, '\n', icon+144, icon+145, 0 };
  draw_text_small(str, x, y, col);
}
