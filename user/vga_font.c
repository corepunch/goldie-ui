// VGA-style monospace font renderer — implementation.
//
// See vga_font.h for the public API.
//
// Rendering approach:
//   - Fallback path: fill_rect + draw_sprite_region per character.
//   - Fast path: build an RG8 cell buffer (R=char, G=bg<<4|fg) and call
//     R_DrawVGABuffer() so composition happens fully inside the renderer.
//
// The character sheet is generated at init time: a TTF font is loaded via
// stb_truetype and all 256 ASCII glyphs are rasterised into a 128x256 RGBA
// texture (16 x 16 grid of 8x16 cells) with anti-aliased alpha coverage.

#include "vga_font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "draw.h"
#include "../kernel/renderer.h"

// Single compilation unit for stb_truetype — must be the only place in
// libuser where STB_TRUETYPE_IMPLEMENTATION is defined.
#define STB_TRUETYPE_IMPLEMENTATION
#include "../tools/stb_truetype.h"

#ifndef VGA_FONT_LOG
#define VGA_FONT_LOG(...) do { axLog("[vga_font] " __VA_ARGS__); } while (0)
#endif

// ============================================================
// Module state
// ============================================================

static uint32_t g_vga_tex    = 0;   // GL texture ID; 0 = not loaded
static int      g_sheet_w    = 128;
static int      g_sheet_h    = 256;

// Glyph-grid constants (must match the shader in share/shaders/vga.frag.glsl).
#define VGA_GLYPH_W     8
#define VGA_GLYPH_H     16
#define VGA_GRID_COLS   16
#define VGA_GRID_ROWS   16

// Per-draw character buffer texture (R=char, G=bg<<4|fg).
static uint32_t g_cell_tex   = 0;
static int      g_cell_cap_w = 0;

static const uint32_t kEgaPalette[16] = {
  0xFF000000u, 0xFF0000AAu, 0xFF00AA00u, 0xFF00AAAAu,
  0xFFAA0000u, 0xFFAA00AAu, 0xFFAA5500u, 0xFFAAAAAAu,
  0xFF555555u, 0xFF5555FFu, 0xFF55FF55u, 0xFF55FFFFu,
  0xFFFF5555u, 0xFFFF55FFu, 0xFFFFFF55u, 0xFFFFFFFFu,
};

static bool vga_ensure_cell_texture(int width_chars) {
  if (width_chars <= 0)
    return false;

  // Keep physical texture width equal to logical text width so shader
  // sampling by character index remains exact.
  if (g_cell_tex && width_chars == g_cell_cap_w)
    return true;

  if (g_cell_tex)
    R_DeleteTexture(g_cell_tex);

  g_cell_tex = R_CreateTextureRG8(width_chars, 1, NULL,
                                  R_FILTER_NEAREST, R_WRAP_CLAMP);
  if (!g_cell_tex)
    return false;
  g_cell_cap_w = width_chars;
  return true;
}

static int nearest_ega_index(uint32_t rgba) {
  int r = (int)((rgba >> 16) & 0xFF);
  int g = (int)((rgba >> 8) & 0xFF);
  int b = (int)(rgba & 0xFF);
  int best = 0;
  uint32_t best_d = 0xFFFFFFFFu;

  for (int i = 0; i < 16; i++) {
    int pr = (int)((kEgaPalette[i] >> 16) & 0xFF);
    int pg = (int)((kEgaPalette[i] >> 8) & 0xFF);
    int pb = (int)(kEgaPalette[i] & 0xFF);
    int dr = r - pr;
    int dg = g - pg;
    int db = b - pb;
    uint32_t d = (uint32_t)(dr * dr + dg * dg + db * db);
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  return best;
}

// ============================================================
// Public: init / shutdown
// ============================================================

// ----------------------------------------------------------
// TTF → character-sheet helpers
// ----------------------------------------------------------

static uint8_t *load_ttf(const char *path, int *out_size) {
  FILE *fp = fopen(path, "rb");
  if (!fp) return NULL;
  fseek(fp, 0, SEEK_END);
  long sz = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (sz <= 0 || sz > 16 * 1024 * 1024) { fclose(fp); return NULL; }
  uint8_t *buf = (uint8_t *)malloc((size_t)sz);
  if (!buf) { fclose(fp); return NULL; }
  size_t n = fread(buf, 1, (size_t)sz, fp);
  fclose(fp);
  if ((long)n != sz) { free(buf); return NULL; }
  *out_size = (int)sz;
  return buf;
}

// Rasterise codepoint ch into cell (col, row) inside a pre-zeroed RGBA sheet.
static void rasterise_glyph(stbtt_fontinfo *font, float scale,
                            int baseline, int ch,
                            int col, int row,
                            uint8_t *sheet, int sheet_w, int sheet_h) {
  int cell_x = col * VGA_GLYPH_W;
  int cell_y = row * VGA_GLYPH_H;

  int bw, bh, xoff, yoff;
  unsigned char *bmp = stbtt_GetCodepointBitmap(font, 0, scale, ch,
                                                 &bw, &bh, &xoff, &yoff);
  if (!bmp) return;

  // Centre horizontally in the cell, baseline-align vertically.
  int draw_x = cell_x + (VGA_GLYPH_W - bw) / 2;
  int draw_y = cell_y + baseline + yoff;

  for (int y = 0; y < bh; y++) {
    for (int x = 0; x < bw; x++) {
      int px = draw_x + x;
      int py = draw_y + y;
      if (px < 0 || px >= sheet_w || py < 0 || py >= sheet_h)
        continue;
      uint8_t a = bmp[y * bw + x];
      if (a > 0) {
        int idx = (py * sheet_w + px) * 4;
        sheet[idx + 0] = 0xFF;  // R
        sheet[idx + 1] = 0xFF;  // G
        sheet[idx + 2] = 0xFF;  // B
        sheet[idx + 3] = a;     // A  (coverage from stb_truetype)
      }
    }
  }
  stbtt_FreeBitmap(bmp, NULL);
}

// ============================================================
// Public: init / shutdown
// ============================================================

bool vga_font_init(const char *ttf_path, int pixel_height) {
  if (g_vga_tex) return true;
  if (!ttf_path) return false;
  if (pixel_height <= 0) pixel_height = VGA_GLYPH_H;

  // ---- load TTF --------------------------------------------------
  int ttf_size = 0;
  uint8_t *ttf_data = load_ttf(ttf_path, &ttf_size);
  if (!ttf_data) {
    VGA_FONT_LOG("vga_font_init: could not load %s", ttf_path);
    return false;
  }

  stbtt_fontinfo font;
  int offset = stbtt_GetFontOffsetForIndex(ttf_data, 0);
  if (!stbtt_InitFont(&font, ttf_data, offset)) {
    VGA_FONT_LOG("vga_font_init: stbtt_InitFont failed for %s", ttf_path);
    free(ttf_data);
    return false;
  }

  float scale = stbtt_ScaleForPixelHeight(&font, (float)pixel_height);

  int ascent, descent, linegap;
  stbtt_GetFontVMetrics(&font, &ascent, &descent, &linegap);
  int baseline = (int)(ascent * scale + 0.5f);

  // ---- generate character sheet ----------------------------------
  int sheet_w = VGA_GRID_COLS * VGA_GLYPH_W;  // 128
  int sheet_h = VGA_GRID_ROWS * VGA_GLYPH_H;  // 256

  uint8_t *pixels = (uint8_t *)calloc((size_t)(sheet_w * sheet_h * 4), 1);
  if (!pixels) { free(ttf_data); return false; }

  for (int ch = 0; ch < 256; ch++) {
    int col = ch & 0xF;
    int row = ch >> 4;
    rasterise_glyph(&font, scale, baseline, ch, col, row,
                    pixels, sheet_w, sheet_h);
  }

  g_vga_tex = R_CreateTextureRGBA(sheet_w, sheet_h, pixels,
                                   R_FILTER_NEAREST, R_WRAP_CLAMP);
  free(pixels);
  free(ttf_data);

  if (!g_vga_tex) {
    VGA_FONT_LOG("vga_font_init: R_CreateTextureRGBA failed");
    return false;
  }

  g_sheet_w = sheet_w;
  g_sheet_h = sheet_h;

  VGA_FONT_LOG("vga_font_init: loaded %s  (pixel_height=%d  %dx%d tex=%u)",
         ttf_path, pixel_height, sheet_w, sheet_h, g_vga_tex);
  return true;
}

void vga_font_shutdown(void) {
  if (g_cell_tex)
    R_DeleteTexture(g_cell_tex);
  g_cell_tex = 0;
  g_cell_cap_w = 0;
  if (g_vga_tex) {
    R_DeleteTexture(g_vga_tex);
    g_vga_tex = 0;
  }
}

bool vga_font_loaded(void) {
  return g_vga_tex != 0;
}

uint32_t vga_font_texture_id(void) {
  return g_vga_tex;
}

// ============================================================
// Internal: UV coordinates for a glyph
// ============================================================

static void glyph_uv(int ch, float *u0, float *v0, float *u1, float *v1) {
  // Sheet: 16 cols x 16 rows of GLYPH_WxGLYPH_H cells.
  // Character N: col = N & 0xF, row = N >> 4.
  int col = ch & 0xF;
  int row = ch >> 4;
  *u0 = (float)(col    ) / 16.0f;
  *u1 = (float)(col + 1) / 16.0f;
  *v0 = (float)(row    ) / 16.0f;
  *v1 = (float)(row + 1) / 16.0f;
}

// ============================================================
// Public: draw a single character cell
// ============================================================

static void vga_draw_char_fallback(int ch, int x, int y, uint32_t fg, uint32_t bg) {
  irect16_t cell = { x, y, VGA_CHAR_W, VGA_CHAR_H };

  // 1. Background (always, even when font not loaded).
  fill_rect(bg, cell);

  // 2. Glyph.
  if (!g_vga_tex) return;

  if (ch < 0 || ch > 255) ch = 0x7F;

  float u0, v0, u1, v1;
  glyph_uv(ch, &u0, &v0, &u1, &v1);
  draw_sprite_region((int)g_vga_tex, cell, UV_RECT(u0, v0, u1, v1), fg, 0);
}

void vga_draw_char(int ch, int x, int y, uint32_t fg, uint32_t bg) {
  vga_draw_char_fallback(ch, x, y, fg, bg);
}

// ============================================================
// Public: draw a string
// ============================================================

int vga_draw_textn(const char *text, int max_chars,
                   int x, int y, uint32_t fg, uint32_t bg) {
  if (!text || max_chars <= 0) return 0;

  if (!g_vga_tex) {
    int cx = x;
    int drawn = 0;
    for (const char *p = text; *p && drawn < max_chars; p++, drawn++) {
      vga_draw_char_fallback((unsigned char)*p, cx, y, fg, bg);
      cx += VGA_CHAR_W;
    }
    return cx - x;
  }

  int draw_chars = 0;
  for (const char *p = text; *p && draw_chars < max_chars; p++)
    draw_chars++;
  if (draw_chars <= 0)
    return 0;

  if (!vga_ensure_cell_texture(draw_chars)) {
    int cx = x;
    for (int i = 0; i < draw_chars; i++) {
      vga_draw_char_fallback((unsigned char)text[i], cx, y, fg, bg);
      cx += VGA_CHAR_W;
    }
    return cx - x;
  }

  uint8_t *cells = (uint8_t *)malloc((size_t)draw_chars * 2u);
  if (!cells) {
    int cx = x;
    for (int i = 0; i < draw_chars; i++) {
      vga_draw_char_fallback((unsigned char)text[i], cx, y, fg, bg);
      cx += VGA_CHAR_W;
    }
    return cx - x;
  }

  int fg_idx = nearest_ega_index(fg);
  int bg_idx = nearest_ega_index(bg);
  uint8_t packed_color = (uint8_t)((bg_idx << 4) | fg_idx);
  for (int i = 0; i < draw_chars; i++) {
    cells[i * 2 + 0] = (uint8_t)text[i];
    cells[i * 2 + 1] = packed_color;
  }

  bool ok = R_UpdateTextureRG8(g_cell_tex, 0, 0, draw_chars, 1, cells);
  free(cells);

  if (!ok)
    return 0;

  R_VgaBuffer buf = {
    .vga_buffer = g_cell_tex,
    .width = draw_chars,
    .height = 1,
  };
  if (!R_DrawVGABuffer(&buf, x, y,
                       draw_chars * VGA_CHAR_W, VGA_CHAR_H,
                       g_vga_tex, kEgaPalette)) {
    int cx = x;
    for (int i = 0; i < draw_chars; i++) {
      vga_draw_char_fallback((unsigned char)text[i], cx, y, fg, bg);
      cx += VGA_CHAR_W;
    }
    return cx - x;
  }

  int cx = x;
  cx += draw_chars * VGA_CHAR_W;
  return cx - x;
}

int vga_draw_text(const char *text, int x, int y, uint32_t fg, uint32_t bg) {
  if (!text) return 0;
  return vga_draw_textn(text, (int)strlen(text), x, y, fg, bg);
}

int vga_text_width(int char_count) {
  return char_count * VGA_CHAR_W;
}
