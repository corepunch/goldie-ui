// VGA text buffer rendering — cell-based operations for RG8 grids.
// Handles buffer allocation, cell writing, and ANSI-colored text rendering.

#ifndef __UI_VGA_TEXT_H__
#define __UI_VGA_TEXT_H__

#include <stdint.h>
#include <stdbool.h>

// RGBA cell grid state (4 bytes/cell: R+G=glyph, B=fg, A=bg)
typedef struct {
  uint8_t *cells;     // RGBA grid: R=glyph_lo, G=glyph_hi, B=fg, A=bg
  int      cells_w;
  int      cells_h;
  uint32_t cells_tex;
} vga_text_grid_t;

// Detect UTF-8 multibyte sequence length from first byte
// Returns: 1 if ASCII (0x00-0x7F), or the number of bytes in the sequence
int vga_text_utf8_length(unsigned char first_byte);

// Set a single cell in the RGBA grid
// glyph: 16-bit glyph index, fg_idx/bg_idx: color palette indices [0..255]
void vga_text_set_cell(vga_text_grid_t *grid,
                        int x, int y,
                        uint16_t glyph,
                        int fg_idx, int bg_idx);

// Clear entire grid with given foreground/background colors
void vga_text_clear_grid(vga_text_grid_t *grid,
                         int fg_idx, int bg_idx);

// Allocate or reuse a VGA cell grid of exact size
// Returns false if allocation fails
bool vga_text_ensure_grid(vga_text_grid_t *grid, int w, int h);

// Free all resources in a VGA text grid
void vga_text_free_grid(vga_text_grid_t *grid);

// Write a line of text with ANSI SGR codes to the grid
// Parses escape sequences, detects UTF-8 characters (replaced with placeholder),
// and writes each cell with its color.
// def_fg_col, def_bg_col: default colors (0xAARRGGBB) before any escape codes
void vga_text_write_ansi_line(const char *line,
                              vga_text_grid_t *grid,
                              int row,
                              int col_start,
                              int max_cols,
                              uint32_t def_fg_col,
                              uint32_t def_bg_col);

#endif // __UI_VGA_TEXT_H__
