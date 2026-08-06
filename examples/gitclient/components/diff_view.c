#include "diff_view.h"
#include <orion/user/vga_font.h>
#include <orion/user/ansi.h>
#include <orion/user/rect.h>

#define CLR_CTX_BG   0xFF1E1E1E
#define CLR_HUNK_SEL 0xFF333355

static int visible_lines(window_t *win) {
  int ch = vga_char_height();
  return MAX(1, win->frame.h / ch);
}

static int max_scroll_start(window_t *win, const gc_diff_state_t *st) {
  if (!win || !st) return 0;
  return MAX(0, st->line_count - visible_lines(win));
}

static void notify_stage_hunk(window_t *win, gc_diff_state_t *st) {
  if (!win || !st || st->current_hunk < 0 || st->current_hunk >= st->hunk_count)
    return;
  window_t *root = get_root_window(win);
  if (root)
    send_message(root, evCommand,
                 MAKEDWORD((uint16_t)st->current_hunk, (uint16_t)GC_DIFF_STAGE_HUNK),
                 win);
}

static void notify_toggle_unified(window_t *win) {
  window_t *root = get_root_window(win);
  if (root)
    send_message(root, evCommand,
                 MAKEDWORD(0, (uint16_t)GC_DIFF_TOGGLE_UNIFIED),
                 win);
}

result_t gc_diff_proc(window_t *win, uint32_t msg,
                      uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate: {
      gc_diff_state_t *st = (gc_diff_state_t *)calloc(1, sizeof(gc_diff_state_t));
      win->userdata = st;
      st->unified_mode = true;
      st->current_hunk = -1;
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

    case evLeftButtonDown:
      set_focus(win);
      return false;

    case evKeyDown: {
      gc_diff_state_t *st = (gc_diff_state_t *)win->userdata;
      if (!st) return false;

      if (wparam == AX_KEY_TAB) {
        st->unified_mode = !st->unified_mode;
        notify_toggle_unified(win);
        return true;
      }

      // j/k scroll (vim-style)
      if (wparam == AX_KEY_J) {
        st->scroll_y = CLAMP(st->scroll_y + 1, 0, max_scroll_start(win, st));
        scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_y };
        set_scroll_info(win, SB_VERT, &si, false);
        invalidate_window(win);
        return true;
      }
      if (wparam == AX_KEY_K) {
        st->scroll_y = CLAMP(st->scroll_y - 1, 0, max_scroll_start(win, st));
        scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_y };
        set_scroll_info(win, SB_VERT, &si, false);
        invalidate_window(win);
        return true;
      }

      if (st->hunk_count > 0) {
        if (wparam == AX_KEY_UPARROW) {
          st->current_hunk = MAX(0, st->current_hunk - 1);
          if (st->current_hunk >= 0 && st->current_hunk < st->hunk_count) {
            int target = st->hunk_offsets[st->current_hunk];
            if (target < st->scroll_y)
              st->scroll_y = target;
          }
          scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_y };
          set_scroll_info(win, SB_VERT, &si, false);
          invalidate_window(win);
          return true;
        }
        if (wparam == AX_KEY_DOWNARROW) {
          st->current_hunk = MIN(st->hunk_count - 1, st->current_hunk + 1);
          int vis = visible_lines(win);
          if (st->current_hunk >= 0 && st->current_hunk < st->hunk_count) {
            int target = st->hunk_offsets[st->current_hunk];
            if (target >= st->scroll_y + vis)
              st->scroll_y = target - vis + 1;
          }
          scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_y };
          set_scroll_info(win, SB_VERT, &si, false);
          invalidate_window(win);
          return true;
        }
        if (wparam == AX_KEY_ENTER) {
          notify_stage_hunk(win, st);
          return true;
        }
      }

      if (wparam == AX_KEY_UPARROW || wparam == AX_KEY_DOWNARROW) {
        int dir = (wparam == AX_KEY_UPARROW) ? -1 : 1;
        st->scroll_y = CLAMP(st->scroll_y + dir * 3, 0, max_scroll_start(win, st));
        scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_y };
        set_scroll_info(win, SB_VERT, &si, false);
        invalidate_window(win);
        return true;
      }

      // Page Up / Down
      if (wparam == AX_KEY_PGUP) {
        int vis = visible_lines(win);
        st->scroll_y = CLAMP(st->scroll_y - vis, 0, max_scroll_start(win, st));
        scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_y };
        set_scroll_info(win, SB_VERT, &si, false);
        invalidate_window(win);
        return true;
      }
      if (wparam == AX_KEY_PGDN) {
        int vis = visible_lines(win);
        st->scroll_y = CLAMP(st->scroll_y + vis, 0, max_scroll_start(win, st));
        scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_y };
        set_scroll_info(win, SB_VERT, &si, false);
        invalidate_window(win);
        return true;
      }

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
        int line_idx = first + row;

        // Highlight the selected hunk range
        if (st->current_hunk >= 0 && st->current_hunk + 1 < st->hunk_count) {
          int hunk_start = st->hunk_offsets[st->current_hunk];
          int hunk_end = st->hunk_offsets[st->current_hunk + 1];
          if (line_idx >= hunk_start && line_idx < hunk_end)
            fill_rect(CLR_HUNK_SEL, R(cr.x, cr.y + row * ch, cr.w, ch));
        }

        // Highlight hunk header lines
        for (int h = 0; h < st->hunk_count; h++) {
          if (line_idx == st->hunk_offsets[h])
            fill_rect(CLR_HUNK_SEL, R(cr.x, cr.y + row * ch, cr.w, ch));
        }

        vga_text_write_ansi_line(st->lines[first + row], &st->grid, row,
                                 0, vis_cols, kAnsi16[7], kAnsi16[0]);
      }

      if (R_UpdateTextureRGBA(st->grid.cells_tex, 0, 0,
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
