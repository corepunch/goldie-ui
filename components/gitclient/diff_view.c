#include "diff_view.h"
#include "../../user/vga_font.h"
#include "../../user/ansi.h"

#define CLR_CTX_BG   0xFF1E1E1E

static int visible_lines(window_t *win) {
  int ch = vga_char_height();
  return MAX(1, win->frame.h / ch);
}

static int max_scroll_start(window_t *win, const gc_diff_state_t *st) {
  if (!win || !st) return 0;
  return MAX(0, st->line_count - visible_lines(win));
}

result_t gc_diff_proc(window_t *win, uint32_t msg,
                      uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate: {
      gc_diff_state_t *st = (gc_diff_state_t *)calloc(1, sizeof(gc_diff_state_t));
      win->userdata = st;
      ansi_init_palette256();
      return true;
    }

    case evArrange: {
      layout_arrange_t const *a = (layout_arrange_t const *)lparam;
      if (a) {
        irect16_t r = a->rect;
        if (r.w < 1) r.w = 1;
        if (r.h < 1) r.h = 1;
        win->frame = r;
      }
      invalidate_window(win);
      return true;
    }

    case evDestroy: {
      gc_diff_state_t *st = (gc_diff_state_t *)win->userdata;
      if (st) {
        vga_text_free_grid(&st->grid);
        free(st->lines);
        free(st);
        win->userdata = NULL;
      }
      return false;
    }

    case evResize: {
      gc_diff_state_t *st = (gc_diff_state_t *)win->userdata;
      if (!st) return false;
      st->scroll_y = CLAMP(st->scroll_y, 0, max_scroll_start(win, st));
      scroll_info_t si = {
        .fMask = SIF_PAGE | SIF_POS,
        .nPage = (uint32_t)visible_lines(win),
        .nPos = st->scroll_y,
      };
      set_scroll_info(win, SB_VERT, &si, true);
      invalidate_window(win);
      return false;
    }

    case evVScroll: {
      gc_diff_state_t *st = (gc_diff_state_t *)win->userdata;
      if (!st) return false;
      st->scroll_y = CLAMP((int)wparam, 0, max_scroll_start(win, st));
      scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_y };
      set_scroll_info(win, SB_VERT, &si, false);
      invalidate_window(win);
      return true;
    }

    case evWheel: {
      gc_diff_state_t *st = (gc_diff_state_t *)win->userdata;
      if (!st || !st->line_count) return false;
      int delta = (int16_t)HIWORD((uintptr_t)lparam);
      if (delta == 0) return false;
      int lines = delta < 0 ? 3 : -3;
      int max_start = max_scroll_start(win, st);
      int new_pos = CLAMP(st->scroll_y + lines, 0, max_start);
      if (new_pos != st->scroll_y) {
        st->scroll_y = new_pos;
        scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_y };
        set_scroll_info(win, SB_VERT, &si, false);
        invalidate_window(win);
      }
      return true;
    }

    case evPaint: {
      gc_diff_state_t *st = (gc_diff_state_t *)win->userdata;
      irect16_t cr = get_client_rect(win);

      fill_rect(CLR_CTX_BG, cr);

      if (!st || !st->lines || st->line_count <= 0)
        return true;

      int cw = vga_char_width();
      int ch = vga_char_height();
      int vis_cols = cr.w / cw;
      int vis_rows = cr.h / ch;
      if (vis_cols <= 0 || vis_rows <= 0)
        return true;

      if (!vga_text_ensure_grid(&st->grid, vis_cols, vis_rows))
        return true;

      vga_text_clear_grid(&st->grid, 7, 0);

      int first = st->scroll_y;
      int last = first + vis_rows;
      if (last > st->line_count)
        last = st->line_count;

      for (int row = 0; row < vis_rows && first + row < st->line_count; row++) {
        vga_text_write_ansi_line(st->lines[first + row], &st->grid, row,
                                 0, vis_cols, kAnsi16[7], kAnsi16[0]);
      }

      if (R_UpdateTextureRG8(st->grid.cells_tex, 0, 0,
                              st->grid.cells_w, st->grid.cells_h,
                              st->grid.cells)) {
        R_VgaBuffer buf = {
          .vga_buffer = st->grid.cells_tex,
          .width = st->grid.cells_w,
          .height = st->grid.cells_h,
        };
        VgaFontLayout fl = vga_get_font_layout();
        R_DrawVGABuffer(&buf, cr.x, cr.y,
                        st->grid.cells_w * cw,
                        st->grid.cells_h * ch,
                        (const R_FontSheet*)&fl, kAnsi256);
      }

      return true;
    }

    default:
      return false;
  }
}
