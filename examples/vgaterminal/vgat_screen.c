// Screen management functions for VGA terminal.
// These handle the scrollback ring buffer and cursor state.

#include "vgat.h"
#include <stdlib.h>
#include <string.h>

void vgat_screen_init(vgat_screen *s, int rows, int cols) {
  memset(s, 0, sizeof(*s));
  s->total_rows = VGAT_SCROLLBACK_LINES;
  s->cols = cols;
  s->rows = (vgat_cell *)calloc((size_t)s->total_rows * (size_t)cols, sizeof(vgat_cell));
  s->head = 0;
  s->cursor_col = 0;
  s->cursor_row = 0;
  s->scroll_pos = 0;
  s->cursor_visible = true;
}

void vgat_screen_resize(vgat_screen *s, int cols) {
  if (!s || !s->rows || cols <= 0) return;
  s->cols = cols;
  if (s->cursor_col >= cols) s->cursor_col = cols - 1;
}

void vgat_screen_shutdown(vgat_screen *s) {
  if (!s) return;
  free(s->rows);
  s->rows = NULL;
  s->total_rows = 0;
  s->cols = 0;
}

void vgat_screen_clear(vgat_screen *s) {
  if (!s || !s->rows) return;
  memset(s->rows, 0, (size_t)s->total_rows * s->cols * sizeof(vgat_cell));
  s->head = 0;
  s->cursor_col = 0;
  s->cursor_row = 0;
  s->scroll_pos = 0;
}

static int phys_row(vgat_screen *s, int logical_row) {
  if (logical_row < 0) logical_row = 0;
  if (logical_row >= s->total_rows) logical_row = s->total_rows - 1;
  return (s->head + logical_row) % s->total_rows;
}

void vgat_screen_scroll(vgat_screen *s) {
  if (!s || !s->rows) return;
  s->head = (s->head + 1) % s->total_rows;
  int row = phys_row(s, 0);
  memset(&s->rows[row * s->cols], 0, (size_t)s->cols * sizeof(vgat_cell));
}

void vgat_screen_write_cell(vgat_screen *s, uint8_t ch, int fg, int bg) {
  if (!s || !s->rows) return;
  int row = phys_row(s, s->cursor_row);
  int idx = row * s->cols + s->cursor_col;
  s->rows[idx].ch = ch;
  s->rows[idx].fg = (uint8_t)(fg & 0xF);
  s->rows[idx].bg = (uint8_t)(bg & 0xF);
  s->cursor_col++;
  if (s->cursor_col >= s->cols) {
    s->cursor_col = 0;
    s->cursor_row++;
    while (s->cursor_row >= s->total_rows) {
      vgat_screen_scroll(s);
      s->cursor_row--;
    }
  }
}

void vgat_screen_newline(vgat_screen *s) {
  if (!s) return;
  s->cursor_col = 0;
  s->cursor_row++;
  while (s->cursor_row >= s->total_rows) {
    vgat_screen_scroll(s);
    s->cursor_row--;
  }
}

void vgat_screen_backspace(vgat_screen *s) {
  if (!s) return;
  if (s->cursor_col > 0) {
    s->cursor_col--;
  } else if (s->cursor_row > 0) {
    s->cursor_row--;
    s->cursor_col = s->cols - 1;
  }
}

void vgat_screen_carriage_return(vgat_screen *s) {
  if (!s) return;
  s->cursor_col = 0;
}

void vgat_screen_cursor_left(vgat_screen *s) {
  if (!s) return;
  if (s->cursor_col > 0) s->cursor_col--;
}

void vgat_screen_cursor_right(vgat_screen *s) {
  if (!s) return;
  if (s->cursor_col < s->cols - 1) s->cursor_col++;
}

void vgat_screen_cursor_up(vgat_screen *s) {
  if (!s) return;
  if (s->cursor_row > 0) s->cursor_row--;
}

void vgat_screen_cursor_down(vgat_screen *s) {
  if (!s) return;
  if (s->cursor_row < s->total_rows - 1) s->cursor_row++;
}

void vgat_screen_set_fg(vgat_screen *s, int fg) {
  (void)s; (void)fg;
}

void vgat_screen_set_bg(vgat_screen *s, int bg) {
  (void)s; (void)bg;
}

void vgat_screen_reset(vgat_screen *s) {
  if (!s) return;
  vgat_screen_clear(s);
  s->cursor_visible = true;
}

void vgat_screen_save_cursor(vgat_screen *s) {
  if (!s) return;
  s->saved_cursor_col = s->cursor_col;
  s->saved_cursor_row = s->cursor_row;
  s->saved_cursor_visible = s->cursor_visible;
}

void vgat_screen_restore_cursor(vgat_screen *s) {
  if (!s) return;
  s->cursor_col = s->saved_cursor_col;
  s->cursor_row = s->saved_cursor_row;
  s->cursor_visible = s->saved_cursor_visible;
}

void vgat_screen_erase_display(vgat_screen *s, int mode) {
  if (!s || !s->rows) return;
  if (mode == 2) {
    memset(s->rows, 0, (size_t)s->total_rows * s->cols * sizeof(vgat_cell));
    s->head = 0;
    s->cursor_col = 0;
    s->cursor_row = 0;
  }
  (void)mode;
}

void vgat_screen_erase_line(vgat_screen *s, int mode) {
  if (!s || !s->rows) return;
  int row = phys_row(s, s->cursor_row);
  if (mode == 0 || mode == 2) {
    memset(&s->rows[row * s->cols + s->cursor_col], 0,
           (size_t)(s->cols - s->cursor_col) * sizeof(vgat_cell));
  }
  if (mode == 1 || mode == 2) {
    memset(&s->rows[row * s->cols], 0, (size_t)s->cursor_col * sizeof(vgat_cell));
  }
}

void vgat_screen_cursor_position(vgat_screen *s, int row, int col) {
  if (!s) return;
  if (row < 0) row = 0;
  if (row >= s->total_rows) row = s->total_rows - 1;
  if (col < 0) col = 0;
  if (col >= s->cols) col = s->cols - 1;
  s->cursor_row = row;
  s->cursor_col = col;
}

void vgat_screen_write_string(vgat_screen *s, const char *str, int fg, int bg) {
  if (!s || !str) return;
  for (const char *p = str; *p; p++) {
    if (*p == '\n') {
      vgat_screen_newline(s);
    } else {
      vgat_screen_write_cell(s, (uint8_t)*p, fg, bg);
    }
  }
}
