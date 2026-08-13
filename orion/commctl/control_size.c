#include <stdio.h>

#include <orion/user/user.h>
#include <orion/user/messages.h>
#include "commctl.h"

int control_predefined_height(flags_t flags) {
  switch (flags & CONTROL_SIZE_MASK) {
    case CONTROL_SIZE_MINI:  return CONTROL_HEIGHT_MINI;
    case CONTROL_SIZE_SMALL: return CONTROL_HEIGHT_SMALL;
    case CONTROL_SIZE_LARGE: return CONTROL_HEIGHT_LARGE;
    default:                 return CONTROL_HEIGHT_REGULAR;
  }
}

void control_apply_predefined_height(window_t *win, const char *module) {
  if (!win) return;
  int requested = win->layout.layout_fixed_h;
  int applied = control_predefined_height(win->flags);
  if (requested > 0 && requested != applied) {
    fprintf(stderr, "[ctl] normalize module=%s win=%u requested_h=%d applied_h=%d size=%u\n",
            module ? module : "control", (unsigned)win->id, requested, applied,
            (unsigned)((win->flags & CONTROL_SIZE_MASK) >> 29));
    fflush(stderr);
  }
  win->layout.layout_fixed_h = 0;
  win->frame.h = applied;
}

bool control_arrange_predefined_height(window_t *win, const layout_arrange_t *a) {
  if (!win || !a) return false;
  int h = control_predefined_height(win->flags);
  if (a->rect.h > 0 && a->rect.h < h) h = a->rect.h;
  win->frame.x = a->rect.x;
  win->frame.w = MAX(1, a->rect.w);
  win->frame.h = MAX(1, h);
  win->frame.y = a->rect.y + MAX(0, (a->rect.h - win->frame.h) / 2);
  return true;
}
