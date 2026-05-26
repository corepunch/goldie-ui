#include "../user/user.h"
#include "commctl.h"

// TextEdit is the single-line personality of MultiEdit. Keep this wrapper
// thin so selection/caret/editing behavior has one implementation to evolve.
lresult_t win_textedit(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  if (msg == evCreate)
    return win_multiedit(win, msg, 1, lparam);
  return win_multiedit(win, msg, wparam, lparam);
}
