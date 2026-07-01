#include "ansi_parser.h"
#include "vgat.h"
#include <stdlib.h>
#include <string.h>

enum {
  VGAT_STATE_NORMAL = 0,
  VGAT_STATE_ESC,
  VGAT_STATE_CSI,
};

#define VGAT_MAX(a, b) ((a) > (b) ? (a) : (b))

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
                      void (*cursor_pos)(void*, int, int)) {
  memset(p, 0, sizeof(*p));
  p->state = VGAT_STATE_NORMAL;
  p->screen = screen;
  p->write_cell = write_cell;
  p->newline = newline;
  p->backspace = backspace;
  p->cr = cr;
  p->cursor_left = cursor_left;
  p->cursor_right = cursor_right;
  p->cursor_up = cursor_up;
  p->cursor_down = cursor_down;
  p->set_fg = set_fg;
  p->set_bg = set_bg;
  p->reset = reset;
  p->save_cursor = save_cursor;
  p->restore_cursor = restore_cursor;
  p->erase_display = erase_display;
  p->erase_line = erase_line;
  p->cursor_pos = cursor_pos;
  p->cur_fg = VGAT_FG_DEFAULT;
  p->cur_bg = VGAT_BG_DEFAULT;
}

static void csi_final(vgat_parser_t *p, char final) {
  int *args = p->params;
  int n = p->nparams;

  switch (final) {
    case 'm': {  // SGR - set graphics rendition
      if (n == 0) {
        p->cur_fg = VGAT_FG_DEFAULT;
        p->cur_bg = VGAT_BG_DEFAULT;
        break;
      }
      for (int i = 0; i < n; i++) {
        int code = args[i];
        if (code == 0) {
          p->cur_fg = VGAT_FG_DEFAULT;
          p->cur_bg = VGAT_BG_DEFAULT;
        } else if (code == 1) {
          if (p->cur_fg < 8) p->cur_fg += 8;
        } else if (code == 22) {
          if (p->cur_fg >= 8) p->cur_fg -= 8;
        } else if (code >= 30 && code <= 37) {
          p->cur_fg = code - 30;
        } else if (code >= 40 && code <= 47) {
          p->cur_bg = code - 40;
        } else if (code >= 90 && code <= 97) {
          p->cur_fg = code - 90 + 8;
        } else if (code >= 100 && code <= 107) {
          p->cur_bg = code - 100 + 8;
        } else if (code == 39) {
          p->cur_fg = VGAT_FG_DEFAULT;
        } else if (code == 49) {
          p->cur_bg = VGAT_BG_DEFAULT;
        }
      }
      break;
    }
    case 'H':  // CUP - cursor position (also HOM)
    case 'f':  // HVP - horizontal vertical position
      p->cursor_pos(p->screen,
                    n >= 1 ? args[0] - 1 : 0,
                    n >= 2 ? args[1] - 1 : 0);
      break;
    case 'J':  // ED - erase display
      p->erase_display(p->screen, n >= 1 ? args[0] : 0);
      break;
    case 'K':  // EL - erase line
      p->erase_line(p->screen, n >= 1 ? args[0] : 0);
      break;
    case 'A':  // CUU - cursor up
      for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++)
        p->cursor_up(p->screen);
      break;
    case 'B':  // CUD - cursor down
      for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++)
        p->cursor_down(p->screen);
      break;
    case 'C':  // CUF - cursor forward
      for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++)
        p->cursor_right(p->screen);
      break;
    case 'D':  // CUB - cursor back
      for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++)
        p->cursor_left(p->screen);
      break;
    case 's':  // SCUSR - save cursor
      p->save_cursor(p->screen);
      break;
    case 'u':  // RCUSR - restore cursor
      p->restore_cursor(p->screen);
      break;
    case 'l':  // DECTCEM hide - ?25l (private mode 25)
    case 'h':  // DECTCEM show - ?25h (private mode 25)
      // These have a '?' prefix — we just ignore them for now
      break;
    case 'c':  // RIS - reset to initial state
      p->reset(p->screen);
      break;
    case 'I':  // CHT - cursor horizontal tab forward
      for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++)
        p->cursor_right(p->screen);
      break;
    case 'Z':  // CBT - cursor backward tab
      for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++)
        p->cursor_left(p->screen);
      break;
    case 'd':  // VPA - vertical position absolute
      // Move to absolute row, keep column
      // We don't implement this in the simple screen model
      break;
    default:
      break;
  }
}

void vgat_parser_feed(vgat_parser_t *p, const uint8_t *data, int len) {
  for (int i = 0; i < len; i++) {
    uint8_t c = data[i];

    switch (p->state) {
      case VGAT_STATE_NORMAL:
        if (c == 0x1B) {
          p->state = VGAT_STATE_ESC;
        } else if (c == '\n') {
          p->newline(p->screen);
        } else if (c == '\r') {
          p->cr(p->screen);
        } else if (c == 0x08) {
          p->backspace(p->screen);
        } else if (c == '\t') {
          p->write_cell(p->screen, c, p->cur_fg, p->cur_bg);
        } else if (c == 0x07) {
          // BEL - ignore
        } else if (c >= 0x20) {
          p->write_cell(p->screen, c, p->cur_fg, p->cur_bg);
        }
        break;

      case VGAT_STATE_ESC:
        if (c == '[') {
          p->state = VGAT_STATE_CSI;
          p->nparams = 0;
          p->params[0] = 0;
        } else if (c == 'c') {
          p->reset(p->screen);
          p->state = VGAT_STATE_NORMAL;
        } else if (c == '7') {
          p->save_cursor(p->screen);
          p->state = VGAT_STATE_NORMAL;
        } else if (c == '8') {
          p->restore_cursor(p->screen);
          p->state = VGAT_STATE_NORMAL;
        } else {
          p->state = VGAT_STATE_NORMAL;
        }
        break;

      case VGAT_STATE_CSI:
        if (c >= '0' && c <= '9') {
          p->params[p->nparams] = p->params[p->nparams] * 10 + (c - '0');
        } else if (c == ';') {
          p->nparams++;
          if (p->nparams < 15) p->params[p->nparams] = 0;
        } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
          p->nparams++;
          csi_final(p, (char)c);
          p->state = VGAT_STATE_NORMAL;
        } else {
          p->state = VGAT_STATE_NORMAL;
        }
        break;
    }
  }
}
