// ANSI escape sequence parser for VGA terminal emulator.
// Consumes raw bytes from PTY and updates the screen state.

#ifndef __VGAT_ANSI_PARSER_H__
#define __VGAT_ANSI_PARSER_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct vgat_parser_s {
  int state;
  int params[16];
  int nparams;
  int cur_fg;
  int cur_bg;
  void *screen;
  void (*write_cell)(void *s, uint8_t ch, int fg, int bg);
  void (*newline)(void *s);
  void (*backspace)(void *s);
  void (*cr)(void *s);
  void (*cursor_left)(void *s);
  void (*cursor_right)(void *s);
  void (*cursor_up)(void *s);
  void (*cursor_down)(void *s);
  void (*set_fg)(void *s, int fg);
  void (*set_bg)(void *s, int bg);
  void (*reset)(void *s);
  void (*save_cursor)(void *s);
  void (*restore_cursor)(void *s);
  void (*erase_display)(void *s, int mode);
  void (*erase_line)(void *s, int mode);
  void (*cursor_pos)(void *s, int row, int col);
} vgat_parser_t;

void vgat_parser_init(vgat_parser_t *p, void *screen,
                      void (*write_cell)(void*, uint8_t, int, int),
                      void (*newline)(void*),
                      void (*backspace)(void*),
                      void (*cr)(void*),
                      void (*cursor_left)(void*),
                      void (*cursor_right)(void*),
                      void (*cursor_up)(void*),
                      void (*cursor_down)(void*),
                      void (*set_fg)(void*, int),
                      void (*set_bg)(void*, int),
                      void (*reset)(void*),
                      void (*save_cursor)(void*),
                      void (*restore_cursor)(void*),
                      void (*erase_display)(void*, int),
                      void (*erase_line)(void*, int),
                      void (*cursor_pos)(void*, int, int));

void vgat_parser_feed(vgat_parser_t *p, const uint8_t *data, int len);

#endif // __VGAT_ANSI_PARSER_H__
