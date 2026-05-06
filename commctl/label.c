#include <string.h>
#include <stdio.h>
#include <math.h>

#include "../user/user.h"
#include "../user/messages.h"
#include "../user/draw.h"
#include "../user/rect.h"
#include "../user/theme.h"

// Label control window procedure.
// lparam in evCreate is an optional RGBA color (void*)(uintptr_t)col.
// When lparam is NULL the default brTextNormal is used.
result_t win_label(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      // Single-line labels auto-expand to fit text; multiline labels wrap and
      // must not expand or the layout breaks.
      if (win->frame.h <= CONTROL_HEIGHT)
        win->frame.w = MAX(win->frame.w, text_strwidth(FONT_SMALL, win->title) + TEXT_SHADOW_OFFSET);
      win->flags |= WINDOW_NOTABSTOP;
      if (lparam) win->userdata = lparam;
      return true;
    case evMeasure: {
      layout_measure_t *m = (layout_measure_t *)lparam;
      if (!m) return true;
      if (win->frame.h <= CONTROL_HEIGHT) {
        m->desired_w = MAX(win->frame.w, text_strwidth(FONT_SMALL, win->title) + TEXT_SHADOW_OFFSET);
        m->desired_h = MAX(win->frame.h, CONTROL_HEIGHT);
      } else {
        int avail_w = m->avail_w > 0 ? m->avail_w : win->frame.w;
        if (avail_w < 1) avail_w = win->frame.w > 0 ? win->frame.w : 1;
        m->desired_w = MAX(win->frame.w, avail_w);
        m->desired_h = MAX(win->frame.h, calc_text_height(win->title, avail_w));
      }
      return true;
    }
    case evPaint: {
      // Convention: userdata == 0 → default (brTextNormal);
      // 0 < userdata < brCount → sys_color_idx_t index resolved at paint time;
      // userdata >= brCount → raw RGBA color (top byte is 0xff for any valid RGBA).
      uint32_t col;
      uintptr_t ud = (uintptr_t)win->userdata;
      if (ud == 0)
        col = get_sys_color(brTextNormal);
      else if (ud < (uintptr_t)brCount)
        col = get_sys_color((sys_color_idx_t)ud);
      else
        col = (uint32_t)ud;
      irect16_t text_pos = {0, 0, win->frame.w, win->frame.h};
      // Labels taller than a single control row use draw_text_wrapped so that
      // long text reflows naturally within the available width.
      if (win->frame.h > CONTROL_HEIGHT) {
        draw_text_wrapped(win->title, &text_pos, col);
      } else {
        irect16_t shadow_pos = rect_offset(text_pos, TEXT_SHADOW_OFFSET, TEXT_SHADOW_OFFSET);
        draw_text_clipped(FONT_SMALL, win->title, &shadow_pos, get_sys_color(brDarkEdge), 0);
        draw_text_clipped(FONT_SMALL, win->title, &text_pos, col, 0);
      }
      return true;
    }
  }
  return false;
}
