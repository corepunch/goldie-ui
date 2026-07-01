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
// stb_truetype and all 256 ASCII glyphs are rasterised into an RGBA texture
// arranged as a 16×16 grid.  Cell dimensions are derived from the font's own
// advance width and ascender/descender metrics so the texture adapts to any
// monospace TTF.

#include "vga_font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
static int      g_cell_w     = 0;   // glyph cell width  (pixels)
static int      g_cell_h     = 0;   // glyph cell height (pixels)
static int      g_sheet_w    = 0;   // full sheet width
static int      g_sheet_h    = 0;   // full sheet height

// The grid is always 16×16 (256 characters).
#define VGA_GRID_COLS   16
#define VGA_GRID_ROWS   16

static const uint16_t kCp437High[128] = {
  0x00C7,0x00FC,0x00E9,0x00E2,0x00E4,0x00E0,0x00E5,0x00E7,0x00EA,0x00EB,0x00E8,0x00EF,0x00EE,0x00EC,0x00C4,0x00C5,
  0x00C9,0x00E6,0x00C6,0x00F4,0x00F6,0x00F2,0x00FB,0x00F9,0x00FF,0x00D6,0x00DC,0x00A2,0x00A3,0x00A5,0x20A7,0x0192,
  0x00E1,0x00ED,0x00F3,0x00FA,0x00F1,0x00D1,0x00AA,0x00BA,0x00BF,0x2310,0x00AC,0x00BD,0x00BC,0x00A1,0x00AB,0x00BB,
  0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,0x2555,0x2563,0x2551,0x2557,0x255D,0x255C,0x255B,0x2510,
  0x2514,0x2534,0x252C,0x251C,0x2500,0x253C,0x255E,0x255F,0x255A,0x2554,0x2569,0x2566,0x2560,0x2550,0x256C,0x2567,
  0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256B,0x256A,0x2518,0x250C,0x2588,0x2584,0x258C,0x2590,0x2580,
  0x03B1,0x00DF,0x0393,0x03C0,0x03A3,0x03C3,0x00B5,0x03C4,0x03A6,0x0398,0x03A9,0x03B4,0x221E,0x03C6,0x03B5,0x2229,
  0x2261,0x00B1,0x2265,0x2264,0x2320,0x2321,0x00F7,0x2248,0x00B0,0x2219,0x00B7,0x221A,0x207F,0x00B2,0x25A0,0x00A0,
};

uint8_t vga_font_glyph_for_codepoint(uint32_t cp) {
  if (cp < 0x80) return (uint8_t)cp;
  for (int i = 0; i < 128; i++) if (kCp437High[i] == cp) return (uint8_t)(i + 0x80);
  if (cp == 0x2022 || cp == 0x25CF) return 0xF9;
  return 0xFE;
}

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
// TTF → character-sheet helpers
// ============================================================

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
// Character origin is at the top-left of the cell; the glyph is positioned at
// its natural xoff/yoff (not centred) so side bearings match the font design.
// Pixels outside the cell are clipped.
static void rasterise_glyph(stbtt_fontinfo *font, float scale,
                            int baseline,
                            int cell_w, int cell_h,
                            int ch, int col, int row,
                            uint8_t *sheet, int sheet_w, int sheet_h) {
  int cell_x  = col * cell_w;
  int cell_y  = row * cell_h;
  int cell_x1 = cell_x + cell_w;
  int cell_y1 = cell_y + cell_h;

  int bw, bh, xoff, yoff;
  unsigned char *bmp = stbtt_GetCodepointBitmap(font, 0, scale, ch,
                                                 &bw, &bh, &xoff, &yoff);
  if (!bmp) return;

  int draw_x = cell_x + xoff;
  int draw_y = cell_y + baseline + yoff;

  for (int y = 0; y < bh; y++) {
    int py = draw_y + y;
    if (py < cell_y || py >= cell_y1)
      continue;
    for (int x = 0; x < bw; x++) {
      int px = draw_x + x;
      if (px < cell_x || px >= cell_x1)
        continue;
      uint8_t a = bmp[y * bw + x];
      if (a > 0) {
        int idx = (py * sheet_w + px) * 4;
        sheet[idx + 0] = 0xFF;
        sheet[idx + 1] = 0xFF;
        sheet[idx + 2] = 0xFF;
        sheet[idx + 3] = a;
      }
    }
  }
  stbtt_FreeBitmap(bmp, NULL);
}

// ============================================================
// Public: init / shutdown
// ============================================================

bool vga_font_init(const char *ttf_path, float font_size) {
  if (g_vga_tex) return true;
  if (!ttf_path) return false;

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

  // Font metrics
  int ascent, descent, linegap;
  stbtt_GetFontVMetrics(&font, &ascent, &descent, &linegap);

  float scale = stbtt_ScaleForMappingEmToPixels(&font, font_size);

  int baseline = (int)(ascent * scale + 0.5f);

  // Compute cell height from font metrics (ascent - descent + linegap)
  int cell_h = (int)((ascent - descent + linegap) * scale + 0.5f);
  if (cell_h < 1) cell_h = 1;

  // Compute cell width from the maximum advance width across the glyph set
  // so every glyph has enough room at its natural side-bearing position.
  float max_aw = 0.0f;
  for (int ch = 32; ch < 256; ch++) {
    int aw, lsb;
    int cp = ch < 128 ? ch : kCp437High[ch - 128];
    stbtt_GetCodepointHMetrics(&font, cp, &aw, &lsb);
    if (aw > 0) {
      float w = (float)aw * scale;
      if (w > max_aw) max_aw = w;
    }
  }
  // Fallback if no advance found (unlikely for a proper TTF)
  if (max_aw < 1.0f) max_aw = (float)cell_h * 0.5f;

  int cell_w = (int)ceilf(max_aw);
  if (cell_w < 1) cell_w = 1;

  int sheet_w = VGA_GRID_COLS * cell_w;
  int sheet_h = VGA_GRID_ROWS * cell_h;

  // ---- generate character sheet ----------------------------------
  uint8_t *pixels = (uint8_t *)calloc((size_t)(sheet_w * sheet_h * 4), 1);
  if (!pixels) { free(ttf_data); return false; }

  for (int ch = 0; ch < 256; ch++) {
    int col = ch & 0xF;
    int row = ch >> 4;
    int cp = ch < 128 ? ch : kCp437High[ch - 128];
    rasterise_glyph(&font, scale, baseline, cell_w, cell_h,
                    cp, col, row, pixels, sheet_w, sheet_h);
  }

  g_vga_tex = R_CreateTextureRGBA(sheet_w, sheet_h, pixels,
                                   R_FILTER_NEAREST, R_WRAP_CLAMP);
  free(pixels);
  free(ttf_data);

  if (!g_vga_tex) {
    VGA_FONT_LOG("vga_font_init: R_CreateTextureRGBA failed");
    return false;
  }

  g_cell_w  = cell_w;
  g_cell_h  = cell_h;
  g_sheet_w = sheet_w;
  g_sheet_h = sheet_h;

  VGA_FONT_LOG("vga_font_init: loaded %s  (font_size=%.1f  cell=%dx%d  sheet=%dx%d tex=%u)",
         ttf_path, font_size, cell_w, cell_h, sheet_w, sheet_h, g_vga_tex);
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

int vga_char_width(void) {
  return g_cell_w;
}

int vga_char_height(void) {
  return g_cell_h;
}

VgaFontLayout vga_get_font_layout(void) {
  VgaFontLayout layout = {
    .texture = g_vga_tex,
    .cell_w  = g_cell_w,
    .cell_h  = g_cell_h,
    .sheet_w = g_sheet_w,
    .sheet_h = g_sheet_h,
  };
  return layout;
}

// ============================================================
// Internal: UV coordinates for a glyph
// ============================================================

static void glyph_uv(int ch, float *u0, float *v0, float *u1, float *v1) {
  int col = ch & 0xF;
  int row = ch >> 4;
  *u0 = (float)(col    ) / (float)VGA_GRID_COLS;
  *u1 = (float)(col + 1) / (float)VGA_GRID_COLS;
  *v0 = (float)(row    ) / (float)VGA_GRID_ROWS;
  *v1 = (float)(row + 1) / (float)VGA_GRID_ROWS;
}

// ============================================================
// Public: draw a single character cell
// ============================================================

static void vga_draw_char_fallback(int ch, int x, int y, uint32_t fg, uint32_t bg) {
  irect16_t cell = { x, y, g_cell_w, g_cell_h };

  fill_rect(bg, cell);

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

  if (!g_vga_tex || !g_cell_w || !g_cell_h) {
    int cx = x;
    int drawn = 0;
    for (const char *p = text; *p && drawn < max_chars; p++, drawn++) {
      vga_draw_char_fallback((unsigned char)*p, cx, y, fg, bg);
      cx += g_cell_w;
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
      cx += g_cell_w;
    }
    return cx - x;
  }

  uint8_t *cells = (uint8_t *)malloc((size_t)draw_chars * 2u);
  if (!cells) {
    int cx = x;
    for (int i = 0; i < draw_chars; i++) {
      vga_draw_char_fallback((unsigned char)text[i], cx, y, fg, bg);
      cx += g_cell_w;
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

  R_FontSheet fs = {
    .texture = g_vga_tex,
    .cell_w  = g_cell_w,
    .cell_h  = g_cell_h,
    .sheet_w = g_sheet_w,
    .sheet_h = g_sheet_h,
  };
  R_VgaBuffer buf = {
    .vga_buffer = g_cell_tex,
    .width = draw_chars,
    .height = 1,
  };
  if (!R_DrawVGABuffer(&buf, x, y,
                       draw_chars * g_cell_w, g_cell_h,
                       &fs, kEgaPalette)) {
    int cx = x;
    for (int i = 0; i < draw_chars; i++) {
      vga_draw_char_fallback((unsigned char)text[i], cx, y, fg, bg);
      cx += g_cell_w;
    }
    return cx - x;
  }

  int cx = x;
  cx += draw_chars * g_cell_w;
  return cx - x;
}

int vga_draw_text(const char *text, int x, int y, uint32_t fg, uint32_t bg) {
  if (!text) return 0;
  return vga_draw_textn(text, (int)strlen(text), x, y, fg, bg);
}

int vga_text_width(int char_count) {
  return char_count * g_cell_w;
}
