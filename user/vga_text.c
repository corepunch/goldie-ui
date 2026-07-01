#include "vga_text.h"
#include "vga_font.h"
#include "ansi.h"
#include "../kernel/renderer.h"
#include <stdlib.h>

// Placeholder character for UTF-8/UTF-16 multibyte sequences
#define UTF8_REPLACEMENT_CHAR 0xB7

int vga_text_utf8_length(unsigned char first_byte) {
  if (first_byte < 0x80) return 1;        // ASCII
  if ((first_byte & 0xE0) == 0xC0) return 2;  // 2-byte sequence (110xxxxx)
  if ((first_byte & 0xF0) == 0xE0) return 3;  // 3-byte sequence (1110xxxx)
  if ((first_byte & 0xF8) == 0xF0) return 4;  // 4-byte sequence (11110xxx)
  return 1;  // Invalid sequence, treat as single byte
}

void vga_text_set_cell(vga_text_grid_t *grid,
                        int x, int y,
                        uint16_t glyph,
                        int fg_idx, int bg_idx) {
  if (!grid || !grid->cells || x < 0 || y < 0 || x >= grid->cells_w || y >= grid->cells_h)
    return;

  if (fg_idx < 0) fg_idx = 0;
  if (fg_idx > 255) fg_idx = 255;
  if (bg_idx < 0) bg_idx = 0;
  if (bg_idx > 255) bg_idx = 255;

  int i = (y * grid->cells_w + x) * 4;
  grid->cells[i + 0] = (uint8_t)(glyph & 0xFF);        // R: glyph_lo
  grid->cells[i + 1] = (uint8_t)((glyph >> 8) & 0xFF);  // G: glyph_hi
  grid->cells[i + 2] = (uint8_t)fg_idx;                  // B: fg
  grid->cells[i + 3] = (uint8_t)bg_idx;                  // A: bg
}

void vga_text_clear_grid(vga_text_grid_t *grid,
                         int fg_idx, int bg_idx) {
  if (!grid || !grid->cells || grid->cells_w <= 0 || grid->cells_h <= 0)
    return;
  uint8_t fg = (uint8_t)(fg_idx & 0xFF);
  uint8_t bg = (uint8_t)(bg_idx & 0xFF);
  int n = grid->cells_w * grid->cells_h;
  for (int i = 0; i < n; i++) {
    int j = i * 4;
    grid->cells[j + 0] = 0x20;    // R: glyph_lo (space)
    grid->cells[j + 1] = 0;       // G: glyph_hi
    grid->cells[j + 2] = fg;      // B: fg
    grid->cells[j + 3] = bg;      // A: bg
  }
}

bool vga_text_ensure_grid(vga_text_grid_t *grid, int w, int h) {
  if (!grid || w <= 0 || h <= 0)
    return false;

  if (grid->cells && grid->cells_tex && grid->cells_w == w && grid->cells_h == h)
    return true;

  free(grid->cells);
  grid->cells = NULL;
  if (grid->cells_tex)
    R_DeleteTexture(grid->cells_tex);
  grid->cells_tex = 0;

  grid->cells = (uint8_t *)malloc((size_t)w * (size_t)h * 4u);
  if (!grid->cells) {
    grid->cells_w = grid->cells_h = 0;
    return false;
  }

  grid->cells_tex = R_CreateTextureRGBA(w, h, NULL, R_FILTER_NEAREST, R_WRAP_CLAMP);
  if (!grid->cells_tex) {
    free(grid->cells);
    grid->cells = NULL;
    grid->cells_w = grid->cells_h = 0;
    return false;
  }

  grid->cells_w = w;
  grid->cells_h = h;
  return true;
}

void vga_text_free_grid(vga_text_grid_t *grid) {
  if (!grid) return;
  free(grid->cells);
  grid->cells = NULL;
  if (grid->cells_tex)
    R_DeleteTexture(grid->cells_tex);
  grid->cells_tex = 0;
  grid->cells_w = grid->cells_h = 0;
}

void vga_text_write_ansi_line(const char *line,
                              vga_text_grid_t *grid,
                              int row,
                              int col_start,
                              int max_cols,
                              uint32_t def_fg_col,
                              uint32_t def_bg_col) {
  if (!line || !grid || !grid->cells || row < 0 || row >= grid->cells_h || max_cols <= 0)
    return;

  int def_fg = nearest_ansi_index(def_fg_col);
  int def_bg = nearest_ansi_index(def_bg_col);
  int fg = def_fg;
  int bg = def_bg;
  bool bold = false;
  int out_col = 0;

  for (const char *p = line; *p && out_col < max_cols; ) {
    // Check for ANSI escape sequence
    if ((unsigned char)p[0] == 0x1B && p[1] == '[') {
      const char *q = p + 2;
      int val = 0;
      bool have_val = false;
      int codes[16];
      int n = 0;

      while (*q && *q != 'm') {
        if (*q >= '0' && *q <= '9') {
          have_val = true;
          val = val * 10 + (*q - '0');
        } else if (*q == ';') {
          if (n < 16)
            codes[n++] = have_val ? val : 0;
          val = 0;
          have_val = false;
        } else {
          break;
        }
        q++;
      }

      if (*q == 'm') {
        if (n < 16)
          codes[n++] = have_val ? val : 0;
        if (n == 0)
          ansi_apply_sgr(0, &fg, &bg, def_fg, def_bg, &bold);
        ansi_apply_sgr_codes(codes, n, &fg, &bg, def_fg, def_bg, &bold);
        fg = clamp_ansi_index(fg);
        bg = clamp_ansi_index(bg);
        p = q + 1;
        continue;
      }
    }

    uint32_t cp;
    int seq_len = vga_text_utf8_length((unsigned char)*p);
    if (seq_len == 1) cp = (unsigned char)*p;
    else {
      cp = (unsigned char)*p & (0x7F >> seq_len);
      int i;
      for (i = 1; i < seq_len && (p[i] & 0xC0) == 0x80; i++) cp = (cp << 6) | (p[i] & 0x3F);
      if (i != seq_len) { cp = 0xFFFD; seq_len = 1; }
    }
    uint16_t glyph = vga_font_glyph_for_codepoint(cp);
    vga_font_ensure_glyph(glyph);
    vga_text_set_cell(grid, col_start + out_col, row, glyph, fg, bg);
    p += seq_len;
    out_col++;
  }
}
