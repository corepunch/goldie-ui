// tools/gen_vga_font.c
//
// Headless tool that generates share/fonts/vga-rom-font-8x16.png — a 128x256 RGBA
// monospace character sheet with 256 glyphs arranged in a 16-column x 16-row
// grid (each cell is 8x16 pixels, white glyphs on transparent background).
//
// The source bitmap is the raw Amiga Topaz a500 font from rewtnull/amigafonts:
// 256 glyphs * 16 rows, one byte per 8-pixel row, MSB on the left.
//
// Usage:
//   gen_vga_font [output_path [raw_font_path]]
//   (default output path: share/fonts/vga-rom-font-8x16.png)
//   (default raw font:   fonts/Topaz_a500_v1.0.raw)
//
// Build: this tool is compiled by the standard Makefile tools rule.  It is
// It has no GL context requirement; the only liborion dependency is
// save_image_png() (user/image.h, pure stb_image_write wrapper).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <orion/user/image.h>

#define GLYPH_W     8
#define GLYPH_H     16
#define GRID_COLS   16
#define GRID_ROWS   16
#define SHEET_W     (GRID_COLS * GLYPH_W)   /* 128 */
#define SHEET_H     (GRID_ROWS * GLYPH_H)   /* 256 */
#define RAW_FONT_SIZE (GRID_COLS * GRID_ROWS * GLYPH_H)

static uint8_t *read_raw_font(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "gen_vga_font: failed to open raw font %s\n", path);
    return NULL;
  }

  uint8_t *data = (uint8_t *)malloc(RAW_FONT_SIZE);
  if (!data) {
    fclose(f);
    fprintf(stderr, "gen_vga_font: out of memory\n");
    return NULL;
  }

  size_t n = fread(data, 1, RAW_FONT_SIZE, f);
  int extra = fgetc(f);
  fclose(f);

  if (n != RAW_FONT_SIZE || extra != EOF) {
    fprintf(stderr,
            "gen_vga_font: expected %d-byte raw font, got %zu%s from %s\n",
            RAW_FONT_SIZE, n, extra != EOF ? "+" : "", path);
    free(data);
    return NULL;
  }

  return data;
}

int main(int argc, char *argv[]) {
  const char *out_path = (argc > 1) ? argv[1] : "share/fonts/vga-rom-font-8x16.png";
  const char *raw_path = (argc > 2) ? argv[2] : "share/fonts/Topaz_a500_v1.0.raw";

  uint8_t *raw = read_raw_font(raw_path);
  if (!raw)
    return 1;

  // Allocate RGBA pixel buffer, initialised to transparent black.
  uint8_t *pixels = (uint8_t *)calloc((size_t)(SHEET_W * SHEET_H * 4), 1);
  if (!pixels) {
    fprintf(stderr, "gen_vga_font: out of memory\n");
    free(raw);
    return 1;
  }

  for (int ch = 0; ch < 256; ch++) {
    // Position of this glyph's top-left corner in the sheet.
    int base_x = (ch & 0xF) * GLYPH_W;
    int base_y = (ch >> 4)  * GLYPH_H;

    for (int row = 0; row < GLYPH_H; row++) {
      uint8_t row_bits = raw[ch * GLYPH_H + row];

      for (int bit = 0; bit < GLYPH_W; bit++) {
        // MSB of row_bits = leftmost pixel (column 0).
        int pixel_on = (row_bits >> (GLYPH_W - 1 - bit)) & 1;
        if (!pixel_on) continue;

        int idx = ((base_y + row) * SHEET_W + base_x + bit) * 4;
        pixels[idx + 0] = 0xFF;  /* R — white */
        pixels[idx + 1] = 0xFF;  /* G */
        pixels[idx + 2] = 0xFF;  /* B */
        pixels[idx + 3] = 0xFF;  /* A — fully opaque */
      }
    }
  }

  if (!save_image_png(out_path, pixels, SHEET_W, SHEET_H)) {
    fprintf(stderr, "gen_vga_font: failed to write %s\n", out_path);
    free(raw);
    free(pixels);
    return 1;
  }

  printf("gen_vga_font: wrote %s (%dx%d) from %s\n",
         out_path, SHEET_W, SHEET_H, raw_path);
  free(raw);
  free(pixels);
  return 0;
}
