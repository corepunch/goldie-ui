#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>

#include "../user/user.h"
#include "../user/messages.h"
#include "../user/draw.h"

// Helper function (will be moved to ui/user/window.c later)
extern window_t *get_root_window(window_t *window);

// Button control window procedure
result_t win_button(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      win->frame.w = MAX(win->frame.w, strwidth(win->title)+6);
      win->frame.h = MAX(win->frame.h, BUTTON_HEIGHT);
      return true;
    case WM_PAINT:
      fill_rect(_focused == win?COLOR_FOCUSED:COLOR_PANEL_BG, win->frame.x-2, win->frame.y-2, win->frame.w+4, win->frame.h+4);
      draw_button(win->frame.x, win->frame.y, win->frame.w, win->frame.h, win->pressed);
      if (!win->pressed) {
        draw_text_small(win->title, win->frame.x+4, win->frame.y+4, COLOR_DARK_EDGE);
      }
      draw_text_small(win->title, win->frame.x+((win->pressed)?4:3), win->frame.y+((win->pressed)?4:3), COLOR_TEXT_NORMAL);
      return true;
    case WM_LBUTTONDOWN:
      win->pressed = true;
      invalidate_window(win);
      return true;
    case WM_LBUTTONUP:
      win->pressed = false;
      send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, BN_CLICKED), win);
      invalidate_window(win);
      return true;
    case WM_KEYDOWN:
      if (wparam == SDL_SCANCODE_RETURN || wparam == SDL_SCANCODE_SPACE) {
        win->pressed = true;
        invalidate_window(win);
        return true;
      }
      return false;
    case WM_KEYUP:
      if (wparam == SDL_SCANCODE_RETURN || wparam == SDL_SCANCODE_SPACE) {
        win->pressed = false;
        send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, BN_CLICKED), win);
        invalidate_window(win);
        return true;
      } else {
        return false;
      }
  }
  return false;
}

result_t win_space(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  return false;
}
