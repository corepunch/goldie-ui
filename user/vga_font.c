// VGA-style monospace font renderer — implementation.
//
// See vga_font.h for the public API.
//
// Rendering approach:
//   - Fallback path: fill_rect + draw_sprite_region per character.
//   - Fast path: build an RGBA cell buffer (R+G=glyph, B=fg, A=bg) and call
//     R_DrawVGABuffer() so composition happens fully inside the renderer.
//
// The atlas is a 256x256 RGBA texture (65536 glyph slots).  Glyphs are
// rasterised lazily on first use via stb_truetype.  A per-glyph flag array
// tracks which slots have been filled.

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
// Atlas constants
// ============================================================

#define VGA_ATLAS_COLS  256
#define VGA_ATLAS_ROWS  256
#define VGA_ATLAS_SIZE  (VGA_ATLAS_COLS * VGA_ATLAS_ROWS)  // 65536 slots

// ============================================================
// Module state
// ============================================================

static uint32_t g_vga_tex    = 0;   // GL texture ID; 0 = not loaded
static int      g_cell_w     = 0;   // glyph cell width  (pixels)
static int      g_cell_h     = 0;   // glyph cell height (pixels)
static int      g_sheet_w    = 0;   // full sheet width  (256 * cell_w)
static int      g_sheet_h    = 0;   // full sheet height (256 * cell_h)

// Persistent TTF state for lazy rasterisation.
static uint8_t       *g_ttf_data = NULL;
static stbtt_fontinfo g_font;
static float          g_scale    = 0;
static int            g_baseline = 0;
// Fallback font chain — tried in order when primary font misses a glyph.
#define VGA_FONT_MAX_FALLBACKS 8
typedef struct {
  stbtt_fontinfo info;
  uint8_t       *data;
  float          scale;
} fallback_font_t;
static fallback_font_t g_fallbacks[VGA_FONT_MAX_FALLBACKS];
static int             g_fallback_count = 0;

// Per-glyph flag array: 1 = rasterised, 0 = empty.
static uint8_t g_glyph_flags[VGA_ATLAS_SIZE];

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

uint16_t vga_font_glyph_for_codepoint(uint32_t cp) {
  if (cp <= 0xFFFF) return (uint16_t)cp;
  return 0xFE;
}

// Per-draw character buffer texture (R+G=glyph, B=fg, A=bg).
static uint32_t g_cell_tex   = 0;
static int      g_cell_cap_w = 0;

static uint32_t kEgaPalette[256] = {
  // 0..15: classic EGA/VGA
  0xFF000000u, 0xFF0000AAu, 0xFF00AA00u, 0xFF00AAAAu,
  0xFFAA0000u, 0xFFAA00AAu, 0xFFAA5500u, 0xFFAAAAAAu,
  0xFF555555u, 0xFF5555FFu, 0xFF55FF55u, 0xFF55FFFFu,
  0xFFFF5555u, 0xFFFF55FFu, 0xFFFFFF55u, 0xFFFFFFFFu,
};

// Initialize the 256-color portion (16..255) of kEgaPalette at first use.
static bool kEgaPalette256_init;
static void ensure_ega_palette256(void) {
  if (kEgaPalette256_init) return;
  kEgaPalette256_init = true;
  static const int steps[6] = { 0, 95, 135, 175, 215, 255 };
  for (int i = 0; i < 216; i++) {
    int r = steps[(i / 36) % 6];
    int g = steps[(i / 6) % 6];
    int b = steps[i % 6];
    kEgaPalette[16 + i] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
  }
  for (int i = 0; i < 24; i++) {
    int gray = 8 + i * 10;
    kEgaPalette[232 + i] = 0xFF000000u | ((uint32_t)gray << 16) |
                            ((uint32_t)gray << 8) | (uint32_t)gray;
  }
}

static bool vga_ensure_cell_texture(int width_chars) {
  if (width_chars <= 0)
    return false;

  if (g_cell_tex && width_chars == g_cell_cap_w)
    return true;

  if (g_cell_tex)
    R_DeleteTexture(g_cell_tex);

  g_cell_tex = R_CreateTextureRGBA(width_chars, 1, NULL,
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
// TTF loading
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

bool vga_font_add_fallback(const char *path, int font_index) {
  if (!path || g_fallback_count >= VGA_FONT_MAX_FALLBACKS) return false;
  int sz = 0;
  uint8_t *data = load_ttf(path, &sz);
  if (!data) return false;
  int offset = stbtt_GetFontOffsetForIndex(data, font_index);
  if (offset < 0) { free(data); return false; }
  stbtt_fontinfo fb;
  if (!stbtt_InitFont(&fb, data, offset)) { free(data); return false; }
  int idx = g_fallback_count;
  g_fallbacks[idx].data = data;
  g_fallbacks[idx].info = fb;
  // Default scale maps the fallback font's ascent-descent to cell height
  int fb_ascent, fb_descent, fb_linegap;
  stbtt_GetFontVMetrics(&fb, &fb_ascent, &fb_descent, &fb_linegap);
  float fb_ad = (float)(fb_ascent - fb_descent + fb_linegap);
  if (fb_ad < 1.0f) fb_ad = 1.0f;
  g_fallbacks[idx].scale = (float)g_cell_h / fb_ad;
  g_fallback_count++;
  return true;
}

// ============================================================
// Lazy glyph rasterisation
// ============================================================

// Rasterise a glyph from a given font into the atlas cell.
static void rasterize_from_font(stbtt_fontinfo *font, float scale, int baseline,
                                uint16_t glyph, int cell_x, int cell_y,
                                int cell_x1, int cell_y1) {
  int bw, bh, xoff, yoff;
  unsigned char *bmp = stbtt_GetCodepointBitmap(font, 0, scale, glyph,
                                                 &bw, &bh, &xoff, &yoff);
  if (!bmp) return;

  int draw_x = cell_x + xoff;
  int draw_y = cell_y + baseline + yoff;

  int pixels_per_cell = g_cell_w * g_cell_h;
  uint8_t *cell_pixels = (uint8_t *)calloc((size_t)pixels_per_cell * 4, 1);
  if (!cell_pixels) { stbtt_FreeBitmap(bmp, NULL); return; }

  for (int y = 0; y < bh; y++) {
    int py = draw_y + y;
    if (py < cell_y || py >= cell_y1) continue;
    for (int x = 0; x < bw; x++) {
      int px = draw_x + x;
      if (px < cell_x || px >= cell_x1) continue;
      uint8_t a = bmp[y * bw + x];
      if (a > 0) {
        int idx = ((py - cell_y) * g_cell_w + (px - cell_x)) * 4;
        cell_pixels[idx + 0] = 0xFF;
        cell_pixels[idx + 1] = 0xFF;
        cell_pixels[idx + 2] = 0xFF;
        cell_pixels[idx + 3] = a;
      }
    }
  }
  stbtt_FreeBitmap(bmp, NULL);
  R_UpdateTextureRGBA(g_vga_tex, cell_x, cell_y, g_cell_w, g_cell_h, cell_pixels);
  free(cell_pixels);
}

// Rasterise a glyph from a fallback font, centered in the cell, scaled to fit.
static void rasterize_from_fallback(stbtt_fontinfo *font, float default_scale,
                                     uint16_t glyph, int cell_x, int cell_y,
                                     int cell_x1, int cell_y1) {
  // Compute a per-glyph scale so the glyph fits within the cell with padding.
  int bbx0, bby0, bbx1, bby1;
  float scale = default_scale;
  stbtt_GetCodepointBox(font, glyph, &bbx0, &bby0, &bbx1, &bby1);
  if (bbx0 != 0 || bbx1 != 0 || bby0 != 0 || bby1 != 0) {
    float gw = (float)(bbx1 - bbx0);
    float gh = (float)(bby1 - bby0);
    if (gw < 1.0f) gw = 1.0f;
    if (gh < 1.0f) gh = 1.0f;
    float pad = 0.85f;
    float sx = (float)g_cell_w * pad / gw;
    float sy = (float)g_cell_h * pad / gh;
    scale = (sx < sy) ? sx : sy;
    if (scale < 0.001f) scale = 0.001f;
  }

  int bw, bh, xoff, yoff;
  unsigned char *bmp = stbtt_GetCodepointBitmap(font, 0, scale, glyph,
                                                 &bw, &bh, &xoff, &yoff);
  if (!bmp) return;

  // Center bitmap in the cell
  int draw_x = cell_x + (g_cell_w - bw) / 2;
  int draw_y = cell_y + (g_cell_h - bh) / 2;

  int pixels_per_cell = g_cell_w * g_cell_h;
  uint8_t *cell_pixels = (uint8_t *)calloc((size_t)pixels_per_cell * 4, 1);
  if (!cell_pixels) { stbtt_FreeBitmap(bmp, NULL); return; }

  for (int y = 0; y < bh; y++) {
    int py = draw_y + y;
    if (py < cell_y || py >= cell_y1) continue;
    for (int x = 0; x < bw; x++) {
      int px = draw_x + x;
      if (px < cell_x || px >= cell_x1) continue;
      uint8_t a = bmp[y * bw + x];
      if (a > 0) {
        int idx = ((py - cell_y) * g_cell_w + (px - cell_x)) * 4;
        cell_pixels[idx + 0] = 0xFF;
        cell_pixels[idx + 1] = 0xFF;
        cell_pixels[idx + 2] = 0xFF;
        cell_pixels[idx + 3] = a;
      }
    }
  }
  stbtt_FreeBitmap(bmp, NULL);
  R_UpdateTextureRGBA(g_vga_tex, cell_x, cell_y, g_cell_w, g_cell_h, cell_pixels);
  free(cell_pixels);
}

static void vga_font_rasterize_glyph(uint16_t glyph) {
  if (!g_vga_tex || g_glyph_flags[glyph]) return;

  int col = glyph % VGA_ATLAS_COLS;
  int row = glyph / VGA_ATLAS_COLS;
  int cell_x  = col * g_cell_w;
  int cell_y  = row * g_cell_h;
  int cell_x1 = cell_x + g_cell_w;
  int cell_y1 = cell_y + g_cell_h;

  // Try primary font — uses typographic positioning (baseline + side-bearing)
  if (stbtt_FindGlyphIndex(&g_font, glyph) != 0) {
    rasterize_from_font(&g_font, g_scale, g_baseline, glyph, cell_x, cell_y, cell_x1, cell_y1);
    g_glyph_flags[glyph] = 1;
    return;
  }

  // Try fallback fonts — scale to fit cell and center
  for (int i = 0; i < g_fallback_count; i++) {
    if (stbtt_FindGlyphIndex(&g_fallbacks[i].info, glyph) != 0) {
      rasterize_from_fallback(&g_fallbacks[i].info, g_fallbacks[i].scale,
                               glyph, cell_x, cell_y, cell_x1, cell_y1);
      g_glyph_flags[glyph] = 1;
      return;
    }
  }

  // Glyph not found in any font — mark as filled (empty cell)
  g_glyph_flags[glyph] = 1;
}

// Ensure a glyph is rasterised in the atlas. Called before any draw.
void vga_font_ensure_glyph(uint16_t glyph) {
  vga_font_rasterize_glyph(glyph);
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

  int offset = stbtt_GetFontOffsetForIndex(ttf_data, 0);
  if (!stbtt_InitFont(&g_font, ttf_data, offset)) {
    VGA_FONT_LOG("vga_font_init: stbtt_InitFont failed for %s", ttf_path);
    free(ttf_data);
    return false;
  }
  g_ttf_data = ttf_data;  // keep alive for lazy rasterisation

  // Font metrics
  int ascent, descent, linegap;
  stbtt_GetFontVMetrics(&g_font, &ascent, &descent, &linegap);

  g_scale = stbtt_ScaleForMappingEmToPixels(&g_font, font_size);
  g_baseline = (int)(ascent * g_scale + 0.5f);

  // Compute cell height from font metrics
  int cell_h = (int)((ascent - descent + linegap) * g_scale + 0.5f);
  if (cell_h < 1) cell_h = 1;

  // Compute cell width from the maximum advance width across the glyph set
  float max_aw = 0.0f;
  for (int ch = 32; ch < 256; ch++) {
    int aw, lsb;
    int cp = ch < 128 ? ch : kCp437High[ch - 128];
    stbtt_GetCodepointHMetrics(&g_font, cp, &aw, &lsb);
    if (aw > 0) {
      float w = (float)aw * g_scale;
      if (w > max_aw) max_aw = w;
    }
  }
  if (max_aw < 1.0f) max_aw = (float)cell_h * 0.5f;

  int cell_w = (int)ceilf(max_aw);
  if (cell_w < 1) cell_w = 1;

  int sheet_w = VGA_ATLAS_COLS * cell_w;
  int sheet_h = VGA_ATLAS_ROWS * cell_h;

  // ---- create empty atlas texture --------------------------------
  g_vga_tex = R_CreateTextureRGBA(sheet_w, sheet_h, NULL,
                                   R_FILTER_NEAREST, R_WRAP_CLAMP);
  if (!g_vga_tex) {
    VGA_FONT_LOG("vga_font_init: R_CreateTextureRGBA failed");
    return false;
  }

  memset(g_glyph_flags, 0, sizeof(g_glyph_flags));

  g_cell_w  = cell_w;
  g_cell_h  = cell_h;
  g_sheet_w = sheet_w;
  g_sheet_h = sheet_h;

  VGA_FONT_LOG("vga_font_init: loaded %s  (font_size=%.1f  cell=%dx%d  sheet=%dx%d tex=%u)",
         ttf_path, font_size, cell_w, cell_h, sheet_w, sheet_h, g_vga_tex);

  // Load platform-specific fallback fonts for glyphs missing from the primary font.
  // Order matters: symbols first (most likely to be needed), then broad Unicode coverage.
#if defined(__APPLE__)
  vga_font_add_fallback("/System/Library/Fonts/Apple Symbols.ttf", 0);
  vga_font_add_fallback("/System/Library/Fonts/Supplemental/Arial Unicode.ttf", 0);
#elif defined(_WIN32)
  vga_font_add_fallback("C:\\Windows\\Fonts\\seguisym.ttf", 0);
  vga_font_add_fallback("C:\\Windows\\Fonts\\arial.ttf", 0);
#elif defined(__linux__)
  vga_font_add_fallback("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0);
  vga_font_add_fallback("/usr/share/fonts/truetype/noto/NotoSansSymbols.ttf", 0);
#endif

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
  if (g_ttf_data) {
    free(g_ttf_data);
    g_ttf_data = NULL;
  }
  for (int i = 0; i < g_fallback_count; i++) {
    free(g_fallbacks[i].data);
    g_fallbacks[i].data = NULL;
  }
  g_fallback_count = 0;
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

static void glyph_uv(int glyph, float *u0, float *v0, float *u1, float *v1) {
  int col = glyph % VGA_ATLAS_COLS;
  int row = glyph / VGA_ATLAS_COLS;
  *u0 = (float)(col    ) / (float)VGA_ATLAS_COLS;
  *u1 = (float)(col + 1) / (float)VGA_ATLAS_COLS;
  *v0 = (float)(row    ) / (float)VGA_ATLAS_ROWS;
  *v1 = (float)(row + 1) / (float)VGA_ATLAS_ROWS;
}

// ============================================================
// Public: draw a single character cell
// ============================================================

static void vga_draw_char_fallback(int ch, int x, int y, uint32_t fg, uint32_t bg) {
  irect16_t cell = { x, y, g_cell_w, g_cell_h };

  fill_rect(bg, cell);

  if (!g_vga_tex) return;
  if (ch < 0 || ch > 0xFFFF) ch = 0x7F;

  vga_font_ensure_glyph((uint16_t)ch);

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

  uint8_t *cells = (uint8_t *)malloc((size_t)draw_chars * 4u);
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
  for (int i = 0; i < draw_chars; i++) {
    uint16_t glyph = (uint16_t)(unsigned char)text[i];
    vga_font_ensure_glyph(glyph);
    int j = i * 4;
    cells[j + 0] = (uint8_t)(glyph & 0xFF);
    cells[j + 1] = (uint8_t)((glyph >> 8) & 0xFF);
    cells[j + 2] = (uint8_t)fg_idx;
    cells[j + 3] = (uint8_t)bg_idx;
  }

  bool ok = R_UpdateTextureRGBA(g_cell_tex, 0, 0, draw_chars, 1, cells);
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
  ensure_ega_palette256();
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
