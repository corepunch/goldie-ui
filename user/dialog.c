// Window system stub - actual implementation moved to ui/ framework
// This file remains only to provide compatibility with existing code

#include <SDL2/SDL.h>
#include "gl_compat.h"

#include "../ui.h"

// SDL window (defined here for compatibility)
extern SDL_Window* window;

// All window management functionality is now in:
// - ui/user/window.c - Window lifecycle and management
// - ui/user/message.c - Message queue and dispatch
// - ui/user/draw_impl.c - Drawing primitives
// - ui/kernel/event.c - SDL event handling
//
// Controls are in:
// - ui/commctl/*.c - Button, checkbox, edit, label, list, combobox
// - mapview/editor/sprite.c - Sprite control (moved from ui/commctl)

// Dialog handling (still in mapview as it's app-specific)
static uint32_t _return_code;

bool running;

uint32_t show_dialog(char const *title,
                     const rect_t* frame,
                     window_t *parent,
                     winproc_t proc,
                     void *param)
{
  extern bool running;
  SDL_Event event;
  uint32_t flags = WINDOW_VSCROLL|WINDOW_DIALOG|WINDOW_NOTRAYBUTTON;
  window_t *dlg = create_window("Things", flags, frame, NULL, proc, param);
  enable_window(parent, false);
  show_window(dlg, true);
  while (running && is_window(dlg)) {
    while (get_message(&event)) {
      dispatch_message(&event);
    }
    repost_messages();
  }
  enable_window(parent, true);
  return _return_code;
}

// Dialog result handling
void end_dialog(window_t *win, uint32_t code) {
  _return_code = code;
  destroy_window(win);
}
