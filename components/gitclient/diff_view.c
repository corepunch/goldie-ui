#include "diff_view.h"
#include "../../user/vga_font.h"
#include "../../user/ansi.h"
#include "../../kernel/renderer.h"

#define CLR_ADD_BG   0xFF1A3A1A
#define CLR_ADD_FG   0xFF66FF66
#define CLR_DEL_BG   0xFF3A1A1A
#define CLR_DEL_FG   0xFFFF6666
#define CLR_HUNK_BG  0xFF1A1A3A
#define CLR_HUNK_FG   0xFF66CCFF
#define CLR_CTX_BG   0xFF1E1E1E
#define CLR_CTX_FG   0xFFCCCCCC
#define CLR_LNUM_BG  0xFF2A2A2A
#define CLR_LNUM_FG  0xFF888888
#define CLR_HEADER_BG 0xFF2E2E2E
#define CLR_HEADER_FG 0xFFAAAAAA

#define LINE_NUM_W    (5 * VGA_CHAR_W)

#define GC_DIFF_USE_PREFIX_COLORS 0

static int visible_lines(window_t *win) {
  return MAX(1, win->frame.h / VGA_CHAR_H);
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
      return true;
    }

    case evDestroy: {
      gc_diff_state_t *st = (gc_diff_state_t *)win->userdata;
      if (st) {
        free(st->lines);
        vga_text_free_grid(&st->grid);
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

      if (!st || !st->lines || !st->line_count) {
        draw_text_small("No diff content.", cr.x + 4, cr.y + 4,
                        get_sys_color(brTextDisabled));
        return true;
      }

      int vis   = visible_lines(win);
      int start = CLAMP(st->scroll_y, 0, max_scroll_start(win, st));
      int end   = MIN(start + vis, st->line_count);

      int text_w = cr.w - LINE_NUM_W;
      if (text_w < 8) text_w = 8;

      int max_cols = text_w / VGA_CHAR_W;
      int gutter_cols = LINE_NUM_W / VGA_CHAR_W;
      int total_cols = gutter_cols + max_cols;

      if (total_cols <= 0 || vis <= 0) return true;

      if (!vga_text_ensure_grid(&st->grid, total_cols, vis)) return true;

      int def_fg_idx = nearest_ansi_index(CLR_CTX_FG);
      int def_bg_idx = nearest_ansi_index(CLR_CTX_BG);
      vga_text_clear_grid(&st->grid, def_fg_idx, def_bg_idx);

      int lnum_fg_idx = nearest_ansi_index(CLR_LNUM_FG);
      int lnum_bg_idx = nearest_ansi_index(CLR_LNUM_BG);

      for (int li = start; li < end; li++) {
        const char *line = st->lines[li];
        int row = li - start;

        uint32_t fg = CLR_CTX_FG;
        uint32_t bg = CLR_CTX_BG;

        char lnum[16];
        snprintf(lnum, sizeof(lnum), "%4d ", li + 1);
        for (int c = 0; c < gutter_cols; c++) {
          unsigned char ch = (unsigned char)(lnum[c] ? lnum[c] : ' ');
          vga_text_set_cell(&st->grid, c, row, ch, lnum_fg_idx, lnum_bg_idx);
        }

        vga_text_write_ansi_line(line, &st->grid, row, gutter_cols, max_cols, fg, bg);
      }

      if (R_UpdateTextureRG8(st->grid.cells_tex, 0, 0, st->grid.cells_w, st->grid.cells_h, st->grid.cells)) {
        R_VgaBuffer buf = {
          .vga_buffer = st->grid.cells_tex,
          .width = st->grid.cells_w,
          .height = st->grid.cells_h,
        };
        R_DrawVGABuffer(&buf,
                        cr.x, cr.y,
                        st->grid.cells_w * VGA_CHAR_W,
                        st->grid.cells_h * VGA_CHAR_H,
                        vga_font_texture_id(),
                        kAnsi16);
      }

      return true;
    }

    default:
      return false;
  }
}
