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
extern int screen_width, screen_height;
extern SDL_Window *window;

// Forward declarations
extern void draw_text_small(const char* text, int x, int y, uint32_t col);
extern void draw_icon8(int icon, int x, int y, uint32_t col);
extern void draw_icon16(int icon, int x, int y, uint32_t col);
extern int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern void set_projection(int x, int y, int w, int h);

// Internal white texture for drawing solid colors
static GLuint ui_white_texture = 0;

// Initialize the internal white texture
static void init_ui_white_texture(void) {
  if (ui_white_texture == 0) {
    glGenTextures(1, &ui_white_texture);
    glBindTexture(GL_TEXTURE_2D, ui_white_texture);
    uint32_t white_pixel = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
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
void draw_button(int x, int y, int w, int h, bool pressed) {
  fill_rect(pressed?COLOR_DARK_EDGE:COLOR_LIGHT_EDGE, x-1, y-1, w+2, h+2);
  fill_rect(pressed?COLOR_LIGHT_EDGE:COLOR_DARK_EDGE, x, y, w+1, h+1);
  fill_rect(pressed?COLOR_PANEL_DARK_BG:COLOR_PANEL_BG, x, y, w, h);
  if (pressed) {
    fill_rect(COLOR_FLARE, x+w, y+h, 1, 1);
  } else {
    fill_rect(COLOR_FLARE, x-1, y-1, 1, 1);
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
  set_viewport(&(window_t){0, 0, screen_width, screen_height});
  set_projection(0, 0, screen_width, screen_height);
  
  for (int i = 0; i < 1; i++) {
    int x = win->frame.x + win->frame.w - (i+1)*CONTROL_BUTTON_WIDTH - CONTROL_BUTTON_PADDING;
    int y = window_title_bar_y(win);
    draw_icon8(icon8_minus + i, x, y, COLOR_TEXT_NORMAL);
  }
}

// Set OpenGL viewport for window
void set_viewport(window_t const *win) {
  int w, h;
  SDL_GL_GetDrawableSize(window, &w, &h);
  
  float scale_x = (float)w / screen_width;
  float scale_y = (float)h / screen_height;
  
  int vp_x = (int)(win->frame.x * scale_x);
  int vp_y = (int)((screen_height - win->frame.y - win->frame.h) * scale_y); // flip Y
  int vp_w = (int)(win->frame.w * scale_x);
  int vp_h = (int)(win->frame.h * scale_y);
  
  glEnable(GL_SCISSOR_TEST);
  glViewport(vp_x, vp_y, vp_w, vp_h);
  glScissor(vp_x, vp_y, vp_w, vp_h);
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
  set_viewport(&(window_t){0, 0, screen_width, screen_height});
  set_projection(0, 0, screen_width, screen_height);
  
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

// Draw all windows
void draw_windows(bool rich) {
  repaint_stencil();
  for (window_t *win = windows; win; win = win->next) {
    if (!win->visible)
      continue;
    send_message(win, WM_NCPAINT, 0, NULL);
    send_message(win, WM_PAINT, 0, NULL);
  }
}

// Set stencil test to render for specific window
void ui_set_stencil_for_window(uint32_t window_id) {
  glStencilFunc(GL_EQUAL, window_id, 0xFF);
}

// Set stencil test to render for root window
void ui_set_stencil_for_root_window(uint32_t window_id) {
  glStencilFunc(GL_EQUAL, window_id, 0xFF);
}

// Begin frame rendering
void ui_begin_frame(void) {
  // Nothing to do here for now - left for future expansion
}

// End frame rendering  
void ui_end_frame(void) {
  // Nothing to do here for now - left for future expansion
}

// Clear screen with color
void ui_clear_screen(float r, float g, float b) {
  glClearColor(r, g, b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

// Swap display buffers
void ui_swap_buffers(void) {
  extern SDL_Window *window;
  SDL_GL_SwapWindow(window);
}

// Fill a rectangle with a solid color
void fill_rect(int color, int x, int y, int w, int h) {
  // Ensure the white texture is initialized
  if (ui_white_texture == 0) {
    init_ui_white_texture();
  }
  
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
