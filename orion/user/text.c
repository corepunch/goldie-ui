// Text rendering implementation — multi-font design.
//
// Named atlases at UI_WINDOW_SCALE == 1:
//   big   = ChiKareGo2 (16x16 cells, foNT metrics) — FONT_SYSTEM chrome
//   small = Geneva12 / SmallFont                   — FONT_SMALL content
//   icon  = Geneva9 / SmallFont                    — FONT_ICON large icon labels
//
// At UI_WINDOW_SCALE >= 2 the system and content roles both use SmallFont.

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "text.h"
#include "user.h"
#include "image.h"
#include <orion/kernel/kernel.h>

// foNT chunk reader — no writer/stbtt deps needed.
#include <tools/tiny_png.h>

#define MAX_TEXT_LENGTH   4096
#define VERTICES_PER_CHAR 6

#define WIN_PADDING 4

typedef struct {
  int16_t  x, y;
  float    u, v;
  uint32_t col;
} text_vertex_t;

// Per-character rendering metrics, indexed by the full char code (0-255).
typedef struct {
  uint8_t advance[256];  // cursor advance in pixels
  int8_t  x0[256];       // bitmap box left offset (can be negative)
  uint8_t draw_w[256];   // bitmap width in pixels
} glyph_metrics_t;

// One font: atlas data and all metrics needed to lay it out.
typedef struct {
  R_Mesh          mesh;
  R_Texture       texture;   // GL_RED with swizzle: R→alpha
  int             cell_w;
  int             cell_h;
  int             chars_per_row;
  glyph_metrics_t metrics;
  int             line_height;
  int             space_width;
} font_atlas_t;

static struct {
  font_atlas_t    big;          // FONT_SYSTEM: ChiKareGo2 at scale=1, SmallFont at scale>=2
  font_atlas_t    small;        // FONT_SMALL: Geneva12/SmallFont 0-255 at scale=1
  font_atlas_t    icon;         // FONT_ICON: Geneva9/SmallFont 0-255 at scale=1
} text_state = {0};

// ── Dynamic metric accessors ──────────────────────────────────────────────────
// All space/line/height values are loaded from each font's foNT metadata.

static inline font_atlas_t *font_for_role(ui_font_t font) {
  if (font == FONT_ICON)  return &text_state.icon;
  if (font == FONT_SMALL) return &text_state.small;
  return &text_state.big;
}

static inline int font_space(ui_font_t font) {
  font_atlas_t *atlas = font_for_role(font);
  return atlas->space_width ? atlas->space_width : 3;
}

static inline int font_line(ui_font_t font) {
  font_atlas_t *atlas = font_for_role(font);
  return atlas->line_height ? atlas->line_height : 12;
}

// Legacy getters: return FONT_SYSTEM (big atlas) metrics.

int get_char_height(void) { return text_state.big.cell_h ? text_state.big.cell_h : 8; }
int get_line_height(void) { return text_state.big.line_height ? text_state.big.line_height : 12; }
int get_space_width(void) { return text_state.big.space_width ? text_state.big.space_width : 3; }

// ── Helper: read a raw file into a heap buffer ────────────────────────────────

static unsigned char *read_file(const char *path, size_t *out_size) {
  FILE *f = fopen(path, "rb");
  if (!f) { *out_size = 0; return NULL; }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  rewind(f);
  if (sz <= 0) { fclose(f); *out_size = 0; return NULL; }
  unsigned char *buf = (unsigned char *)malloc((size_t)sz);
  if (!buf) { fclose(f); *out_size = 0; return NULL; }
  if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
    free(buf); fclose(f); *out_size = 0; return NULL;
  }
  fclose(f);
  *out_size = (size_t)sz;
  return buf;
}

// ── Initialise a mesh for the given atlas ─────────────────────────────────────

static void init_atlas_mesh(font_atlas_t *atlas) {
  R_VertexAttrib attribs[] = {
    {0, 2, GL_SHORT,         GL_FALSE, offsetof(text_vertex_t, x)},
    {1, 2, GL_FLOAT,         GL_FALSE, offsetof(text_vertex_t, u)},
    {2, 4, GL_UNSIGNED_BYTE, GL_TRUE,  offsetof(text_vertex_t, col)},
  };
  R_MeshInit(&atlas->mesh, attribs, 3, sizeof(text_vertex_t), GL_TRIANGLES);
}

// ── Load one atlas from a PNG file ───────────────────────────────────────────
//
// first_char/last_char: range of char codes to populate metrics for.
// Requires foNT metadata chunk in PNG; fails if not present.

static bool load_atlas(font_atlas_t *atlas, const char *path,
                       int first_char, int last_char) {
  // ── Load pixel data ───────────────────────────────────────────────────────
  int img_w = 0, img_h = 0;
  uint8_t *rgba = load_image(path, &img_w, &img_h);
  if (!rgba) {
    printf("text: failed to load font image: %s\n", path);
    return false;
  }

  // ── Cell layout will come from foNT chunk ─────────────────────────────────
  int cell_w = 16, cell_h = 16;      // placeholder; will be set from foNT
  int chars_per_row = img_w / cell_w;
  int baseline = cell_h;

  // ── Read foNT chunk (required) ─────────────────────────────────────────────
  TinyPngFontInfo fi = {0};
  TinyPngGlyph *glyphs = NULL;
  size_t raw_sz = 0;
  unsigned char *raw = read_file(path, &raw_sz);
  if (!raw || tiny_png_read_font_chunk(raw, raw_sz, &fi, &glyphs) != 1) {
    printf("text: font %s missing required foNT chunk\n", path);
    image_free(rgba);
    if (raw) free(raw);
    return false;
  }
  free(raw);
  cell_w        = fi.cell_w;
  cell_h        = fi.cell_h;
  if (cell_w <= 0 || cell_h <= 0 || (img_w % cell_w) != 0 || (img_h % cell_h) != 0) {
    printf("text: font %s has invalid cell dimensions (%dx%d) for image (%dx%d)\n",
           path, cell_w, cell_h, img_w, img_h);
    if (glyphs) free(glyphs);
    image_free(rgba);
    return false;
  }
  chars_per_row = img_w / cell_w;
  baseline      = fi.baseline;

  // ── Extract R channel → single-byte GL_RED buffer ────────────────────────
  size_t npx = (size_t)(img_w * img_h);
  unsigned char *red = (unsigned char *)malloc(npx);
  if (!red) {
    image_free(rgba);
    if (glyphs) free(glyphs);
    return false;
  }
  for (size_t i = 0; i < npx; i++)
    red[i] = rgba[i * 4];   // R channel; for greyscale PNG loaded as RGBA this is the grayval
  image_free(rgba);

  // ── Populate glyph metrics from foNT ──────────────────────────────────────
  for (int c = first_char; c <= last_char; c++) {
    if (c >= fi.first_char && c < fi.first_char + fi.num_chars) {
      int idx = c - fi.first_char;
      atlas->metrics.x0[c]      = glyphs[idx].x0;
      atlas->metrics.draw_w[c]  = glyphs[idx].w;
      atlas->metrics.advance[c] = glyphs[idx].advance;
    } else {
      // Char outside foNT range — use defaults
      atlas->metrics.x0[c]      = 0;
      atlas->metrics.draw_w[c]  = (uint8_t)cell_w;
      atlas->metrics.advance[c] = (uint8_t)cell_w;
    }
  }

  // ── Upload GL_RED texture ─────────────────────────────────────────────────
  atlas->texture.width  = img_w;
  atlas->texture.height = img_h;
  atlas->texture.format = GL_RED;
  R_AllocateFontTexture(&atlas->texture, red);
  free(red);

  if (glyphs) free(glyphs);

  atlas->cell_w       = cell_w;
  atlas->cell_h       = cell_h;
  atlas->chars_per_row = chars_per_row;
  atlas->line_height = fi.line_height ? fi.line_height : cell_h + 4;
  atlas->space_width = fi.space_width ? fi.space_width :
                       (atlas->metrics.advance[' '] ? atlas->metrics.advance[' '] : 3);

  // ── Initialise vertex mesh ────────────────────────────────────────────────
  init_atlas_mesh(atlas);

  (void)baseline; /* reserved: used for vertical alignment once baseline rendering is added */
  return true;
}

// ── init_text_rendering ───────────────────────────────────────────────────────

void init_text_rendering(void) {
  memset(&text_state, 0, sizeof(text_state));

  const char *exe = ui_get_exe_dir();
  char system_path[4096], small_path[4096], icon_path[4096];

#if UI_WINDOW_SCALE == 1
  snprintf(system_path, sizeof(system_path), "%s/../share/orion/fonts/Chicago-12.png", exe);
  snprintf(small_path,  sizeof(small_path),  "%s/../share/orion/fonts/Geneva-12.png", exe);
#endif
#if UI_WINDOW_SCALE >= 2
  snprintf(system_path, sizeof(system_path), "%s/../share/orion/fonts/SmallFont.png", exe);
  snprintf(small_path,  sizeof(small_path),  "%s/../share/orion/fonts/SmallFont.png", exe);
#endif
  snprintf(icon_path, sizeof(icon_path), "%s/../share/orion/fonts/Geneva-9.png", exe);

  bool loaded = load_atlas(&text_state.big, system_path, 0, 255);
  loaded = load_atlas(&text_state.small, small_path, 0, 255) && loaded;
  loaded = load_atlas(&text_state.icon, icon_path, 0, 255) && loaded;
  if (!loaded) {
    fprintf(stderr, "[text] required font assets failed to load:\n"
                    "  %s\n  %s\n  %s\n", system_path, small_path, icon_path);
    fflush(stderr);
    exit(1);
  }
  printf("text: fonts loaded system=%dx%d small=%dx%d icon=%dx%d\n",
         text_state.big.cell_w, text_state.big.cell_h,
         text_state.small.cell_w, text_state.small.cell_h,
         text_state.icon.cell_w, text_state.icon.cell_h);
}

// ── Internal helpers ──────────────────────────────────────────────────────────

// Return the atlas + metrics for the given font role and char code.
// For FONT_SYSTEM: c < 128 → big atlas (ChiKareGo2); c >= 128 → small (icons).
// For FONT_SMALL:  all chars → small atlas.
// For FONT_ICON:   all chars → icon atlas.
static inline font_atlas_t *atlas_for_font(ui_font_t font, unsigned char c,
                                           glyph_metrics_t **met_out) {
  font_atlas_t *atlas = font == FONT_SYSTEM && c >= 128
                        ? &text_state.small : font_for_role(font);
  *met_out = &atlas->metrics;
  return atlas;
}

static inline int char_advance(unsigned char c) {
  glyph_metrics_t *m;
  atlas_for_font(FONT_SYSTEM, c, &m);
  return m->advance[c];
}

static inline int fallback_char_advance(unsigned char c, ui_font_t font) {
  if (c == '\n') return 0;
  if (c == ' ') {
    int sw = get_space_width();
    return sw > 0 ? sw : 3;
  }

  int ch = text_char_height(font);
  int aw = ch / 2 + 2;
  return aw > 0 ? aw : 6;
}

static inline int wrapped_line_advance(ui_font_t font) {
  int adv = text_char_height(font);
  if (adv <= 0) adv = text_state.small.cell_h ? text_state.small.cell_h : 8;
  return adv > 0 ? adv : 1;
}

static inline int wrap_char_advance(ui_font_t font, unsigned char c) {
  if (c == '\n') return 0;
  if (c == ' ') {
    int sw = font_space(font);
    return sw > 0 ? sw : 3;
  }
  if (text_state.big.cell_h) {
    glyph_metrics_t *met;
    atlas_for_font(font, c, &met);
    return met->advance[c];
  }
  return fallback_char_advance(c, font);
}

// Public API: pixel width of one glyph from the FONT_SYSTEM atlas.
int char_width(unsigned char c) {
  if (!text_state.big.cell_h) return 0;
  return char_advance(c);
}

// ── Render a filled vertex batch for one atlas ────────────────────────────────

extern void push_sprite_args(int tex, int x, int y, int w, int h, float alpha);

static void flush_batch(font_atlas_t *atlas, text_vertex_t *buf, int count) {
  if (count == 0) return;
  R_SetBlendMode(true);
  push_sprite_args((int)atlas->texture.id, 0, 0, 1, 1, 1.0f);
  R_TextureBind(&atlas->texture);
  R_MeshDrawDynamic(&atlas->mesh, buf, (size_t)count);
}

// ── Build vertices for one character ─────────────────────────────────────────

static int emit_char_verts(text_vertex_t *buf, int cursor_x, int y,
                           unsigned char c, uint32_t col,
                           font_atlas_t *atlas, glyph_metrics_t *met) {
  int cell_col = c % atlas->chars_per_row;
  int cell_row = c / atlas->chars_per_row;
  int cx0      = cell_col * atlas->cell_w;
  int cy0      = cell_row * atlas->cell_h;
  int dw       = met->draw_w[c];
  int dh       = atlas->cell_h;
  if (dw == 0) return 0;

  float tw = (float)atlas->texture.width;
  float th = (float)atlas->texture.height;
  // Glyphs are stored left-aligned in cells; x0 is a pen offset for rendering, not texture coords.
  float u1 = cx0                 / tw;
  float v1 = cy0                 / th;
  float u2 = (cx0 + dw)          / tw;
  float v2 = (cy0 + dh)          / th;

  int x = cursor_x;
  buf[0] = (text_vertex_t){ (int16_t)x,      (int16_t)y,      u1, v1, col };
  buf[1] = (text_vertex_t){ (int16_t)x,      (int16_t)(y+dh), u1, v2, col };
  buf[2] = (text_vertex_t){ (int16_t)(x+dw), (int16_t)y,      u2, v1, col };
  buf[3] = (text_vertex_t){ (int16_t)x,      (int16_t)(y+dh), u1, v2, col };
  buf[4] = (text_vertex_t){ (int16_t)(x+dw), (int16_t)(y+dh), u2, v2, col };
  buf[5] = (text_vertex_t){ (int16_t)(x+dw), (int16_t)y,      u2, v1, col };
  return VERTICES_PER_CHAR;
}

// ── strwidth / strnwidth ──────────────────────────────────────────────────────



// Internal font-parameterised width measurement.
static int strnwidth_impl(ui_font_t font, const char *text, int len) {
  if (!text || !*text) return 0;
  if (len > MAX_TEXT_LENGTH) len = MAX_TEXT_LENGTH;
  int sw = font_space(font);
  int w = 0;
  for (int i = 0; i < len; i++) {
    unsigned char c = (unsigned char)text[i];
    if (c == ' ') { w += sw; continue; }
    if (c == '\n') continue;
    glyph_metrics_t *met;
    atlas_for_font(font, c, &met);
    w += met->advance[c];
  }
  return w;
}

int strnwidth(const char *text, int text_length) {
  return strnwidth_impl(FONT_SYSTEM, text, text_length);
}

int strwidth(const char *text) {
  if (!text || !*text) return 0;
  return strnwidth_impl(FONT_SYSTEM, text, (int)strlen(text));
}

// ── New explicit-font metric API ──────────────────────────────────────────────

int text_char_height(ui_font_t font) {
  font_atlas_t *atlas = font_for_role(font);
  return atlas->cell_h ? atlas->cell_h : 8;
}

int text_strwidth(ui_font_t font, const char *text) {
  if (!text || !*text) return 0;
  return strnwidth_impl(font, text, (int)strlen(text));
}

int text_strnwidth(ui_font_t font, const char *text, int len) {
  if (!text) return 0;
  int slen = (int)strlen(text);
  if (len > slen) len = slen;
  return strnwidth_impl(font, text, len);
}

// ── draw_text / draw_text_small ───────────────────────────────────────────────

void draw_text(ui_font_t font, const char *text, int x, int y, uint32_t col) {
  if (!text || !*text || !g_ui_runtime.running) return;

  int text_length = (int)strlen(text);
  if (text_length > MAX_TEXT_LENGTH) text_length = MAX_TEXT_LENGTH;

  // Static vertex buffers — one per atlas.
  // FONT_SMALL and FONT_ICON use their role atlases. FONT_SYSTEM uses the
  // system atlas for text and the small atlas for characters 128-255.
  static text_vertex_t buf_big  [MAX_TEXT_LENGTH * VERTICES_PER_CHAR];
  static text_vertex_t buf_small[MAX_TEXT_LENGTH * VERTICES_PER_CHAR];
  static text_vertex_t buf_icon [MAX_TEXT_LENGTH * VERTICES_PER_CHAR];
  int vc_big = 0, vc_small = 0, vc_icon = 0;

  int sw = font_space(font);
  int lh = font_line(font);

  int cursor_x = x;
  for (int i = 0; i < text_length; i++) {
    unsigned char c = (unsigned char)text[i];
    if (c == ' ')  { cursor_x += sw; continue; }
    if (c == '\n') { cursor_x = x; y += lh; continue; }

    glyph_metrics_t *met;
    font_atlas_t    *atlas = atlas_for_font(font, c, &met);

    bool use_icon  = atlas == &text_state.icon;
    bool use_small = atlas == &text_state.small;
    text_vertex_t *buf = use_icon ? buf_icon : (use_small ? buf_small : buf_big);
    int           *vc  = use_icon ? &vc_icon : (use_small ? &vc_small : &vc_big);

    *vc += emit_char_verts(buf + *vc, cursor_x, y, c, col, atlas, met);
    cursor_x += met->advance[c];
  }

  if (vc_big > 0)
    flush_batch(&text_state.big,   buf_big,   vc_big);
  if (vc_small > 0)
    flush_batch(&text_state.small, buf_small, vc_small);
  if (vc_icon > 0)
    flush_batch(&text_state.icon,  buf_icon,  vc_icon);
}

void draw_text_clipped(ui_font_t font, const char *text,
                       irect16_t const *viewport, uint32_t col, uint32_t flags) {
  if (!text || !*text || !g_ui_runtime.running || !viewport) return;
  int cell_h = text_char_height(font);
  int x = viewport->x;
  int y = viewport->y + (viewport->h - cell_h) / 2;
  if (flags & TEXT_ALIGN_RIGHT)
    x = viewport->x + viewport->w - text_strwidth(font, text);
  else if (flags & TEXT_ALIGN_CENTER)
    x = viewport->x + (viewport->w - text_strwidth(font, text)) / 2;
  else if (flags & TEXT_PADDING_LEFT)
    x += WIN_PADDING;
  draw_text(font, text, x, y, col);
}

// ── Legacy FONT_SYSTEM wrappers ───────────────────────────────────────────────

void draw_text_small(const char *text, int x, int y, uint32_t col) {
  draw_text(FONT_SYSTEM, text, x, y, col);
}

void draw_text_small_clipped(const char *text, irect16_t const *viewport,
                              uint32_t col, uint32_t flags) {
  draw_text_clipped(FONT_SYSTEM, text, viewport, col, flags);
}

// ── calc_text_height ──────────────────────────────────────────────────────────

text_wrap_result_t text_wrap_layout_font(ui_font_t font, const char *text,
                                         irect16_t const *viewport,
                                         uint32_t col, bool draw) {
  text_wrap_result_t out = {0, 0, false};
  if (!text || !*text || !viewport || viewport->w <= 0) return out;
  if (draw && (!g_ui_runtime.running || !text_state.small.cell_h)) return out;

  static text_vertex_t buf[MAX_TEXT_LENGTH * VERTICES_PER_CHAR];
  int vc = 0;
  int x = viewport->x;
  int y = viewport->y;
  int w = viewport->w;
  int lh = wrapped_line_advance(font);
  int sw = text_strwidth(font, " ");
  if (sw <= 0) sw = font_space(font);
  if (sw <= 0) sw = 3;

  int cx = x;
  int cy = y;
  int line_w = 0;
  int max_line_w = 0;
  int lines = 1;
  font_atlas_t *cur_atlas = NULL;

  for (const char *p = text; *p; p++) {
    unsigned char c = (unsigned char)*p;

    if (c == '\n') {
      if (line_w > max_line_w) max_line_w = line_w;
      line_w = 0;
      cx = x;
      cy += lh;
      lines++;
      out.wrapped = true;
      continue;
    }

    if (c == ' ') {
      int cw = sw;
      if (cx + cw > x + w) {
        if (line_w > max_line_w) max_line_w = line_w;
        line_w = 0;
        cx = x;
        cy += lh;
        lines++;
        out.wrapped = true;
      } else {
        cx += cw;
        line_w += cw;
      }
      continue;
    }

    int cw = wrap_char_advance(font, c);
    if (cx + cw > x + w) {
      if (line_w > max_line_w) max_line_w = line_w;
      line_w = 0;
      cx = x;
      cy += lh;
      lines++;
      out.wrapped = true;
    }

    if (draw && vc < MAX_TEXT_LENGTH * VERTICES_PER_CHAR - VERTICES_PER_CHAR) {
      glyph_metrics_t *met;
      font_atlas_t *atlas = atlas_for_font(font, c, &met);
      if (cur_atlas && atlas != cur_atlas && vc > 0) {
        flush_batch(cur_atlas, buf, vc);
        vc = 0;
      }
      cur_atlas = atlas;
      vc += emit_char_verts(buf + vc, cx, cy, c, col, atlas, met);
    }
    cx += cw;
    line_w += cw;
  }

  if (line_w > max_line_w) max_line_w = line_w;
  out.width = max_line_w;
  out.height = lines * lh;

  if (draw && vc > 0 && cur_atlas)
    flush_batch(cur_atlas, buf, vc);

  return out;
}

text_wrap_result_t text_wrap_layout(const char *text, irect16_t const *viewport,
                                    uint32_t col, bool draw) {
  return text_wrap_layout_font(FONT_SMALL, text, viewport, col, draw);
}

int calc_text_height_font(ui_font_t font, const char *text, int width) {
  irect16_t vp = {0, 0, width, 1};
  return text_wrap_layout_font(font, text, &vp, 0, false).height;
}

int calc_text_height(const char *text, int width) {
  return calc_text_height_font(FONT_SMALL, text, width);
}

// ── draw_text_wrapped ─────────────────────────────────────────────────────────
// Always uses Geneva-12 (FONT_SMALL) for wrapped content text.

void draw_text_wrapped_font(ui_font_t font, const char *text,
                            irect16_t const *viewport, uint32_t col) {
  (void)text_wrap_layout_font(font, text, viewport, col, true);
}

void draw_text_wrapped(const char *text, irect16_t const *viewport, uint32_t col) {
  draw_text_wrapped_font(FONT_SMALL, text, viewport, col);
}

// ── shutdown_text_rendering ───────────────────────────────────────────────────

void shutdown_text_rendering(void) {
  R_DeleteTexture((uint32_t)text_state.big.texture.id);
  text_state.big.texture.id = 0;
  R_MeshDestroy(&text_state.big.mesh);

  R_DeleteTexture((uint32_t)text_state.small.texture.id);
  text_state.small.texture.id = 0;
  R_MeshDestroy(&text_state.small.mesh);

  R_DeleteTexture((uint32_t)text_state.icon.texture.id);
  text_state.icon.texture.id = 0;
  R_MeshDestroy(&text_state.icon.mesh);

  memset(&text_state, 0, sizeof(text_state));
}
