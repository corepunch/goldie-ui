#include "ansi_parser.h"
#include "vgat.h"
#include "../../user/vga_font.h"
#include <stdlib.h>
#include <string.h>

enum {
  VGAT_STATE_NORMAL = 0,
  VGAT_STATE_ESC,
  VGAT_STATE_CSI,
};

#define VGAT_MAX(a, b) ((a) > (b) ? (a) : (b))

void vgat_parser_init(vgat_parser_t *p, const vgat_parser_callbacks_t *cbs) {
  memset(p, 0, sizeof(*p));
  p->state = VGAT_STATE_NORMAL;
  p->screen = cbs->screen;
  p->write_cell = cbs->write_cell;
  p->newline = cbs->newline;
  p->backspace = cbs->backspace;
  p->cr = cbs->cr;
  p->cursor_left = cbs->cursor_left;
  p->cursor_right = cbs->cursor_right;
  p->cursor_up = cbs->cursor_up;
  p->cursor_down = cbs->cursor_down;
  p->set_fg = cbs->set_fg;
  p->set_bg = cbs->set_bg;
  p->reset = cbs->reset;
  p->save_cursor = cbs->save_cursor;
  p->restore_cursor = cbs->restore_cursor;
  p->erase_display = cbs->erase_display;
  p->erase_line = cbs->erase_line;
  p->cursor_pos = cbs->cursor_pos;
  p->cur_fg = VGAT_FG_DEFAULT;
  p->cur_bg = VGAT_BG_DEFAULT;
  p->bold = false;
}

static void csi_final(vgat_parser_t *p, char final) {
  int *args = p->params;
  int n = p->nparams;

  switch (final) {
    case 'm':
      if (n == 0) { p->cur_fg = VGAT_FG_DEFAULT; p->cur_bg = VGAT_BG_DEFAULT; p->bold = false; break; }
      ansi_apply_sgr_codes(args, n, &p->cur_fg, &p->cur_bg, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT, &p->bold);
      break;
    case 'H': case 'f':  p->cursor_pos(p->screen, n >= 1 ? args[0] - 1 : 0, n >= 2 ? args[1] - 1 : 0); break;
    case 'J':            p->erase_display(p->screen, n >= 1 ? args[0] : 0);                             break;
    case 'K':            p->erase_line(p->screen, n >= 1 ? args[0] : 0);                               break;
    case 'A':            for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++) p->cursor_up(p->screen);    break;
    case 'B':            for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++) p->cursor_down(p->screen);  break;
    case 'C':            for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++) p->cursor_right(p->screen); break;
    case 'D':            for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++) p->cursor_left(p->screen);  break;
    case 's':            p->save_cursor(p->screen);    break;
    case 'u':            p->restore_cursor(p->screen); break;
    case 'c':            p->reset(p->screen);           break;
    case 'I':            for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++) p->cursor_right(p->screen); break;
    case 'Z':            for (int i = 0; i < VGAT_MAX(1, n >= 1 ? args[0] : 1); i++) p->cursor_left(p->screen);  break;
    case 'l': case 'h':  break;  // DECTCEM - ignored
    case 'd':            break;  // VPA - not implemented
    default:
      break;
  }
}

void vgat_parser_feed(vgat_parser_t *p, const uint8_t *data, int len) {
  for (int i = 0; i < len; i++) {
    uint8_t c = data[i];

    switch (p->state) {
      case VGAT_STATE_NORMAL:
        if (p->utf8_remaining) {
          if ((c & 0xC0) == 0x80) {
            p->utf8_codepoint = (p->utf8_codepoint << 6) | (c & 0x3F);
            if (--p->utf8_remaining == 0) {
              uint16_t glyph = vga_font_glyph_for_codepoint(p->utf8_codepoint);
              vga_font_ensure_glyph(glyph);
              p->write_cell(p->screen, glyph, p->cur_fg, p->cur_bg);
            }
            break;
          }
          vga_font_ensure_glyph(0xFE);
          p->write_cell(p->screen, 0xFE, p->cur_fg, p->cur_bg);
          p->utf8_remaining = 0;
        }
        if (c == 0x1B)   { p->state = VGAT_STATE_ESC; }
        else if (c == '\n') { p->newline(p->screen); }
        else if (c == '\r') { p->cr(p->screen); }
        else if (c == 0x08) { p->backspace(p->screen); }
        else if (c == '\t') { vga_font_ensure_glyph(c); p->write_cell(p->screen, c, p->cur_fg, p->cur_bg); }
        else if (c == 0x07) { /* BEL - ignore */ }
        else if (c >= 0xF0 && c <= 0xF4) { p->utf8_codepoint = c & 0x07; p->utf8_remaining = 3; }
        else if (c >= 0xE0 && c <= 0xEF) { p->utf8_codepoint = c & 0x0F; p->utf8_remaining = 2; }
        else if (c >= 0xC2 && c <= 0xDF) { p->utf8_codepoint = c & 0x1F; p->utf8_remaining = 1; }
        else if (c >= 0x20 && c < 0x80)  { vga_font_ensure_glyph(c); p->write_cell(p->screen, c, p->cur_fg, p->cur_bg); }
        else if (c >= 0x80)              { vga_font_ensure_glyph(0xFE); p->write_cell(p->screen, 0xFE, p->cur_fg, p->cur_bg); }
        break;

      case VGAT_STATE_ESC:
        if (c == '[')   { p->state = VGAT_STATE_CSI; p->nparams = 0; p->params[0] = 0; }
        else if (c == 'c')  { p->reset(p->screen); p->state = VGAT_STATE_NORMAL; }
        else if (c == '7')  { p->save_cursor(p->screen); p->state = VGAT_STATE_NORMAL; }
        else if (c == '8')  { p->restore_cursor(p->screen); p->state = VGAT_STATE_NORMAL; }
        else            { p->state = VGAT_STATE_NORMAL; }
        break;

      case VGAT_STATE_CSI:
        if (c >= '0' && c <= '9')           { p->params[p->nparams] = p->params[p->nparams] * 10 + (c - '0'); }
        else if (c == ';')                  { p->nparams++; if (p->nparams < 15) p->params[p->nparams] = 0; }
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) { p->nparams++; csi_final(p, (char)c); p->state = VGAT_STATE_NORMAL; }
        else                                { p->state = VGAT_STATE_NORMAL; }
        break;
    }
  }
}
