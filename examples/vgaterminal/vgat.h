// VGA Console — Quake-style command console with VGA font rendering.
//
// Built-in command dispatch replaces the PTY-driven terminal emulator.
// Commands are registered in a dispatch table: {name, help, func}.

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
#include "../../user/scrollbar.h"
#include "../../platform/platform.h"
#include "../../user/ansi.h"

/* Lua headers - same probe logic as commctl/terminal.c */
#if defined(HAVE_LUA)
#  if defined(_WIN32) || defined(_WIN64)
#    include <lua.h>
#    include <lauxlib.h>
#    include <lualib.h>
#  elif __has_include(<lua5.4/lua.h>)
#    include <lua5.4/lua.h>
#    include <lua5.4/lauxlib.h>
#    include <lua5.4/lualib.h>
#  elif __has_include(<lua.h>)
#    include <lua.h>
#    include <lauxlib.h>
#    include <lualib.h>
#  else
#    undef HAVE_LUA
#  endif
#endif

#define VGAT_WINDOW_TITLE  "VGA Console"
#define VGAT_DEFAULT_COLS  80
#define VGAT_DEFAULT_ROWS  24
#define VGAT_SCROLLBACK_LINES 1000
#define VGAT_TIMER_INTERVAL_MS 16
#define VGAT_CURSOR_BLINK_MS 500

#define VGAT_FG_DEFAULT 7   // ANSI white
#define VGAT_BG_DEFAULT 0   // ANSI black

#define VGAT_INPUT_MAX 256

// Forward declaration
typedef struct vgat_state_s vgat_state_t;

// Command function: called with parsed argc/argv.
typedef void (*vgat_cmd_func_t)(vgat_state_t *st, int argc, char **argv);

// Command table entry.
typedef struct {
  const char *name;
  const char *help;
  vgat_cmd_func_t func;
} vgat_cmd_t;

// VGA cell — stored per-character in the scrollback ring buffer.
typedef struct {
  uint8_t ch;
  uint8_t fg;
  uint8_t bg;
} vgat_cell;

// Ring-buffer scrollback screen.
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

// Per-window state.
struct vgat_state_s {
  window_t *win;
  vgat_screen screen;
  int timer_id;
  uint32_t default_fg;
  uint32_t default_bg;
  int scroll_pos;

  // Input line
  char input_buf[VGAT_INPUT_MAX];
  int input_len;

  // Cursor blink
  bool cursor_visible;
  int cursor_blink_ctr;

  // Lua scripting (NULL when not in use or not compiled in)
#if defined(HAVE_LUA)
  lua_State *L;          // Main Lua state (NULL in command-only mode)
  lua_State *co;         // Coroutine for current script
#else
  void *L;
  void *co;
#endif
  bool lua_running;       // True while a Lua script is executing
  bool waiting_for_input; // True while waiting for io.read() input
};

extern const vgat_cmd_t g_cmds[];

result_t vgaterminal_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

void vgat_screen_init(vgat_screen *s, int rows, int cols);
void vgat_screen_resize(vgat_screen *s, int cols);
void vgat_screen_shutdown(vgat_screen *s);
void vgat_screen_clear(vgat_screen *s);
void vgat_screen_scroll(vgat_screen *s);
void vgat_screen_write_cell(vgat_screen *s, uint8_t ch, int fg, int bg);
void vgat_screen_write_string(vgat_screen *s, const char *str, int fg, int bg);
void vgat_screen_newline(vgat_screen *s);
void vgat_screen_backspace(vgat_screen *s);
void vgat_screen_carriage_return(vgat_screen *s);
void vgat_screen_cursor_left(vgat_screen *s);
void vgat_screen_cursor_right(vgat_screen *s);
void vgat_screen_cursor_up(vgat_screen *s);
void vgat_screen_cursor_down(vgat_screen *s);
void vgat_screen_erase_display(vgat_screen *s, int mode);
void vgat_screen_erase_line(vgat_screen *s, int mode);
void vgat_screen_cursor_position(vgat_screen *s, int row, int col);

#endif // __VGAT_H__
