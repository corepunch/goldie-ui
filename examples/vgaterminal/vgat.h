// VGA Terminal — shared types, constants, and includes.
// A simple terminal emulator using the Orion framework's VGA font rendering.

#ifndef __VGAT_H__
#define __VGAT_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "../../ui.h"
#include "../../user/vga_font.h"
#include "../../user/vga_text.h"
#include "../../user/ansi.h"
#include "../../user/scrollbar.h"
#include "../../platform/platform.h"

#define VGAT_WINDOW_TITLE  "VGA Terminal"
#define VGAT_DEFAULT_COLS  80
#define VGAT_DEFAULT_ROWS  24
#define VGAT_SCROLLBACK_LINES 1000
#define VGAT_TIMER_INTERVAL_MS 16

#define VGAT_FG_DEFAULT 7   // ANSI white
#define VGAT_BG_DEFAULT 0   // ANSI black

typedef struct {
  uint8_t ch;
  uint8_t fg;
  uint8_t bg;
} vgat_cell;

typedef struct {
  vgat_cell *rows;
  int total_rows;
  int cols;
  int head;
  int cursor_col;
  int cursor_row;
  int scroll_pos;
  bool cursor_visible;
  int saved_cursor_col;
  int saved_cursor_row;
  int saved_cursor_visible;
} vgat_screen;

typedef struct {
  window_t *win;
  int master_fd;
  int child_pid;
  vgat_screen screen;
  int timer_id;
  uint32_t default_fg;
  uint32_t default_bg;
  int scroll_pos;
} vgat_state_t;

extern vgat_state_t *g_vgat;

lresult_t vgaterminal_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

void vgat_screen_init(vgat_screen *s, int rows, int cols);
void vgat_screen_resize(vgat_screen *s, int cols);
void vgat_screen_shutdown(vgat_screen *s);
void vgat_screen_clear(vgat_screen *s);
void vgat_screen_scroll(vgat_screen *s);
void vgat_screen_write_cell(vgat_screen *s, uint8_t ch, int fg, int bg);
void vgat_screen_newline(vgat_screen *s);
void vgat_screen_backspace(vgat_screen *s);
void vgat_screen_carriage_return(vgat_screen *s);
void vgat_screen_cursor_left(vgat_screen *s);
void vgat_screen_cursor_right(vgat_screen *s);
void vgat_screen_cursor_up(vgat_screen *s);
void vgat_screen_cursor_down(vgat_screen *s);
void vgat_screen_set_fg(vgat_screen *s, int fg);
void vgat_screen_set_bg(vgat_screen *s, int bg);
void vgat_screen_reset(vgat_screen *s);
void vgat_screen_save_cursor(vgat_screen *s);
void vgat_screen_restore_cursor(vgat_screen *s);
void vgat_screen_erase_display(vgat_screen *s, int mode);
void vgat_screen_erase_line(vgat_screen *s, int mode);
void vgat_screen_cursor_position(vgat_screen *s, int row, int col);

int  vgat_pty_open(const char *shell, int rows, int cols, int *pid_out);
void vgat_pty_close(int pid);
bool vgat_pty_resize(int master_fd, int rows, int cols);
int  vgat_pty_read(int master_fd, void *buf, int sz);
int  vgat_pty_write(int master_fd, const void *buf, int sz);

#endif // __VGAT_H__
