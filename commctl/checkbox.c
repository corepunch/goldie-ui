#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>

#include "../user/user.h"
#include "../user/messages.h"
#include "../user/draw.h"

// Helper function (will be moved to ui/user/window.c later)
extern window_t *get_root_window(window_t *window);
extern window_t *_focused;

// Checkbox control window procedure
result_t win_checkbox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      win->frame.w = MAX(win->frame.w, strwidth(win->title)+16);
      win->frame.h = MAX(win->frame.h, BUTTON_HEIGHT);
      return true;
    case WM_PAINT:
      fill_rect(_focused == win?COLOR_FOCUSED:COLOR_PANEL_BG, win->frame.x-2, win->frame.y-2, 14, 14);
      draw_button(win->frame.x, win->frame.y, 10, 10, win->pressed);
      draw_text_small(win->title, win->frame.x + 17, win->frame.y + 3, COLOR_DARK_EDGE);
      draw_text_small(win->title, win->frame.x + 16, win->frame.y + 2, COLOR_TEXT_NORMAL);
      if (win->value) {
        draw_icon8(icon8_checkbox, win->frame.x+1, win->frame.y+1, COLOR_TEXT_NORMAL);
      }
      return true;
    case WM_LBUTTONDOWN:
      win->pressed = true;
      invalidate_window(win);
      return true;
    case WM_LBUTTONUP:
      win->pressed = false;
      send_message(win, BM_SETCHECK, !send_message(win, BM_GETCHECK, 0, NULL), NULL);
      send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, BN_CLICKED), win);
      invalidate_window(win);
      return true;
    case BM_SETCHECK:
      win->value = (wparam != BST_UNCHECKED);
      return true;
    case BM_GETCHECK:
      return win->value ? BST_CHECKED : BST_UNCHECKED;
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
        send_message(win, BM_SETCHECK, !send_message(win, BM_GETCHECK, 0, NULL), NULL);
        send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, BN_CLICKED), win);
        invalidate_window(win);
        return true;
      } else {
        return false;
      }
  }
  return false;
}
