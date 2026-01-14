#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "../user/user.h"
#include "../user/messages.h"
#include "../user/draw.h"

#define PADDING 3

// Helper function (will be moved to ui/user/window.c later)
extern window_t *_focused;

// Label control window procedure
result_t win_label(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      win->frame.w = MAX(win->frame.w, strwidth(win->title));
      win->notabstop = true;
      return true;
    case WM_PAINT:
      draw_text_small(win->title, win->frame.x+1, win->frame.y+1+PADDING, COLOR_DARK_EDGE);
      draw_text_small(win->title, win->frame.x, win->frame.y+PADDING, COLOR_TEXT_NORMAL);
      return true;
  }
  return false;
}
