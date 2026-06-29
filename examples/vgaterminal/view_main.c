// Main window procedure for VGA Terminal emulator.

#include "vgat.h"
#include "pty.h"
#include "ansi_parser.h"
#include <stdlib.h>

#define TIMER_ID_DRAIN 1

static vgat_parser_t g_parser;

static void on_write_cell(void *screen, uint8_t ch, int fg, int bg) {
  vgat_screen_write_cell((vgat_screen *)screen, ch, fg, bg);
}
static void on_newline(void *screen) { vgat_screen_newline((vgat_screen *)screen); }
static void on_backspace(void *screen) { vgat_screen_backspace((vgat_screen *)screen); }
static void on_cr(void *screen) { vgat_screen_carriage_return((vgat_screen *)screen); }
static void on_cursor_left(void *screen) { vgat_screen_cursor_left((vgat_screen *)screen); }
static void on_cursor_right(void *screen) { vgat_screen_cursor_right((vgat_screen *)screen); }
static void on_cursor_up(void *screen) { vgat_screen_cursor_up((vgat_screen *)screen); }
static void on_cursor_down(void *screen) { vgat_screen_cursor_down((vgat_screen *)screen); }
static void on_set_fg(void *screen, int fg) { (void)screen; (void)fg; }
static void on_set_bg(void *screen, int bg) { (void)screen; (void)bg; }
static void on_reset(void *screen) { vgat_screen_reset((vgat_screen *)screen); }
static void on_save_cursor(void *screen) { vgat_screen_save_cursor((vgat_screen *)screen); }
static void on_restore_cursor(void *screen) { vgat_screen_restore_cursor((vgat_screen *)screen); }
static void on_erase_display(void *screen, int mode) { vgat_screen_erase_display((vgat_screen *)screen, mode); }
static void on_erase_line(void *screen, int mode) { vgat_screen_erase_line((vgat_screen *)screen, mode); }
static void on_cursor_pos(void *screen, int row, int col) { vgat_screen_cursor_position((vgat_screen *)screen, row, col); }

static void send_escape(int fd, char code) {
  char buf[3] = { 0x1B, '[', code };
  vgat_pty_write(fd, buf, 3);
}

lresult_t vgaterminal_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  vgat_state_t *st = (vgat_state_t *)win->userdata;

  switch (msg) {
    case evCreate: {
      st = (vgat_state_t *)calloc(1, sizeof(vgat_state_t));
      win->userdata = st;
      if (!st) return false;
      st->win = win;
      st->default_fg = kAnsi16[VGAT_FG_DEFAULT];
      st->default_bg = kAnsi16[VGAT_BG_DEFAULT];
      st->scroll_pos = 0;
      st->master_fd = -1;
      st->child_pid = -1;

      irect16_t cr = get_client_rect(win);
      int cols = cr.w / VGA_CHAR_W;
      int rows = cr.h / VGA_CHAR_H;
      if (cols <= 0) cols = VGAT_DEFAULT_COLS;
      if (rows <= 0) rows = VGAT_DEFAULT_ROWS;

      vgat_screen_init(&st->screen, rows, cols);

      char font_path[512];
      snprintf(font_path, sizeof(font_path), "%s/../share/orion/fonts/vga-rom-font-8x16.png",
               ui_get_exe_dir());
      vga_font_init(font_path);

      const char *shell = getenv("SHELL");
      st->master_fd = vgat_pty_open(shell, rows, cols, &st->child_pid);

      vgat_parser_init(&g_parser, &st->screen,
                       on_write_cell, on_newline, on_backspace, on_cr,
                       on_cursor_left, on_cursor_right, on_cursor_up, on_cursor_down,
                       on_set_fg, on_set_bg, on_reset,
                       on_save_cursor, on_restore_cursor,
                       on_erase_display, on_erase_line, on_cursor_pos);

      st->timer_id = axSetTimer(win, VGAT_TIMER_INTERVAL_MS, NULL, true);

      if (st->master_fd < 0) {
        const char *msg2 = "PTY not available on this platform";
        for (int i = 0; msg2[i]; i++)
          vgat_screen_write_cell(&st->screen, (uint8_t)msg2[i], VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
        vgat_screen_newline(&st->screen);
        invalidate_window(win);
      }
      return true;
    }

    case evDestroy: {
      if (st) {
        if (st->timer_id > 0)
          axCancelTimer(st->timer_id);
        if (st->master_fd >= 0) {
          vgat_pty_close(st->child_pid);
        }
        vga_font_shutdown();
        vgat_screen_shutdown(&st->screen);
        free(st);
        win->userdata = NULL;
      }
      return false;
    }

    case evTimer: {
      if (wparam == TIMER_ID_DRAIN && st && st->master_fd >= 0) {
        uint8_t buf[256];
        int n = vgat_pty_read(st->master_fd, buf, sizeof(buf));
        if (n > 0) {
          vgat_parser_feed(&g_parser, buf, n);
          invalidate_window(win);
        }
      }
      return true;
    }

    case evPaint: {
      if (!st || !st->screen.rows) return true;

      irect16_t cr = get_client_rect(win);
      int vis_cols = cr.w / VGA_CHAR_W;
      int vis_rows = cr.h / VGA_CHAR_H;
      if (vis_cols <= 0 || vis_rows <= 0) return true;

      vga_text_grid_t grid;
      if (!vga_text_ensure_grid(&grid, vis_cols, vis_rows)) return true;
      vga_text_clear_grid(&grid, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);

      int start_row = vis_rows - 1 - st->scroll_pos;
      if (start_row < 0) start_row = 0;

      for (int row = 0; row < vis_rows; row++) {
        int screen_row = row + start_row;
        if (screen_row >= st->screen.total_rows) continue;
        int phys = (st->screen.head + screen_row) % st->screen.total_rows;
        for (int col = 0; col < vis_cols; col++) {
          if (col >= st->screen.cols) break;
          vgat_cell *cell = &st->screen.rows[phys * st->screen.cols + col];
          vga_text_set_cell(&grid, col, row, cell->ch, cell->fg, cell->bg);
        }
      }

      if (R_UpdateTextureRG8(grid.cells_tex, 0, 0, grid.cells_w, grid.cells_h, grid.cells)) {
        R_VgaBuffer buf = {
          .vga_buffer = grid.cells_tex,
          .width = grid.cells_w,
          .height = grid.cells_h,
        };
        R_DrawVGABuffer(&buf, cr.x, cr.y,
                        grid.cells_w * VGA_CHAR_W,
                        grid.cells_h * VGA_CHAR_H,
                        vga_font_texture_id(), kAnsi16);
      }

      vga_text_free_grid(&grid);
      return true;
    }

    case evResize: {
      if (!st) return false;
      irect16_t cr = get_client_rect(win);
      int cols = cr.w / VGA_CHAR_W;
      int rows = cr.h / VGA_CHAR_H;
      if (cols <= 0 || rows <= 0) return false;

      vgat_screen_resize(&st->screen, cols);

      if (st->master_fd >= 0) {
        vgat_pty_resize(st->master_fd, rows, cols);
      }

      scroll_info_t si = {
        .fMask = SIF_RANGE | SIF_PAGE | SIF_POS,
        .nMin = 0,
        .nMax = VGAT_SCROLLBACK_LINES,
        .nPage = (uint32_t)rows,
        .nPos = st->scroll_pos,
      };
      set_scroll_info(win, SB_VERT, &si, true);
      invalidate_window(win);
      return false;
    }

    case evVScroll: {
      if (!st) return false;
      irect16_t cr = get_client_rect(win);
      int vis_rows = cr.h / VGA_CHAR_H;
      if (vis_rows <= 0) vis_rows = 24;
      int max_scroll = VGAT_SCROLLBACK_LINES - vis_rows;
      if (max_scroll < 0) max_scroll = 0;
      int new_pos = CLAMP((int)wparam, 0, max_scroll);
      if (new_pos != st->scroll_pos) {
        st->scroll_pos = new_pos;
        scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_pos };
        set_scroll_info(win, SB_VERT, &si, false);
        invalidate_window(win);
      }
      return true;
    }

    case evWheel: {
      if (!st) return false;
      int delta = (int16_t)HIWORD((uintptr_t)lparam);
      irect16_t cr = get_client_rect(win);
      int vis_rows = cr.h / VGA_CHAR_H;
      if (vis_rows <= 0) vis_rows = 24;
      int max_scroll = VGAT_SCROLLBACK_LINES - vis_rows;
      if (max_scroll < 0) max_scroll = 0;
      int lines = delta < 0 ? 3 : -3;
      int new_pos = CLAMP(st->scroll_pos + lines, 0, max_scroll);
      if (new_pos != st->scroll_pos) {
        st->scroll_pos = new_pos;
        scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_pos };
        set_scroll_info(win, SB_VERT, &si, false);
        invalidate_window(win);
      }
      return true;
    }

    case evKeyDown: {
      if (!st || st->master_fd < 0) return false;
      char buf[8];
      int len = 0;

      switch (wparam) {
        case AX_KEY_ENTER:    buf[len++] = '\r'; break;
        case AX_KEY_BACKSPACE: buf[len++] = 0x7F; break;
        case AX_KEY_TAB:     buf[len++] = '\t'; break;
        case AX_KEY_ESCAPE:  buf[len++] = 0x1B; break;
        case AX_KEY_UPARROW:    send_escape(st->master_fd, 'A'); return true;
        case AX_KEY_DOWNARROW:  send_escape(st->master_fd, 'B'); return true;
        case AX_KEY_RIGHTARROW: send_escape(st->master_fd, 'C'); return true;
        case AX_KEY_LEFTARROW: send_escape(st->master_fd, 'D'); return true;
        case AX_KEY_HOME:   buf[len++] = 0x1B; buf[len++] = '['; buf[len++] = 'H'; break;
        case AX_KEY_END:    buf[len++] = 0x1B; buf[len++] = '['; buf[len++] = 'F'; break;
        default: return false;
      }

      if (len > 0) {
        vgat_pty_write(st->master_fd, buf, len);
      }
      return true;
    }

    case evTextInput: {
      if (!st || st->master_fd < 0) return false;
      const char *text = (const char *)lparam;
      if (text && text[0]) {
        vgat_pty_write(st->master_fd, text, (int)strlen(text));
      }
      return true;
    }

    default:
      return default_winproc(win, msg, wparam, lparam);
  }
}
