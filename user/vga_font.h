// VGA-style monospace font renderer.
//
// Loads a TTF/OTF font and generates an RGBA character sheet at runtime:
// a 16x16 grid of glyph cells, each cell sized to the font's natural advance
// width and height.  Uses stb_truetype.h to rasterise glyphs into the sheet
// on first load.
//
// The sheet is then used with the existing VGA rendering pipeline (RG8 cell
// buffer + vga.frag.glsl shader) for fast fixed-width text output.
//
// Colours follow the framework's 0xAARRGGBB convention (same as
// get_sys_color / fill_rect / draw_text_small).

#ifndef __UI_VGA_FONT_H__
#define __UI_VGA_FONT_H__

#include <stdbool.h>
#include <stdint.h>

// Font sheet layout derived from the loaded TTF at init time.
typedef struct {
  uint32_t texture;   // GL texture ID of the glyph sheet
  int cell_w;          // pixel width of one glyph cell
  int cell_h;          // pixel height of one glyph cell
  int sheet_w;         // total sheet width  (= 16 * cell_w)
  int sheet_h;         // total sheet height (= 16 * cell_h)
} VgaFontLayout;

// Initialise the VGA font renderer by loading a TTF font and generating the
// character sheet on the fly.  ttf_path points to the .ttf file (relative to
// the exe dir or absolute).  font_size is the EM-square height in pixels (same
// convention as lite-master / stbtt_ScaleForMappingEmToPixels).  Pass 0 to
// auto-size to a reasonable default (13.5, matching lite-master's code font).
// Must be called after ui_init_graphics().
// Returns true on success, false when the file is missing or the GL texture
// cannot be created.  When false, all draw calls become no-ops.
bool vga_font_init(const char *ttf_path, float font_size);

// Release the GL texture.  Safe to call even when init failed.
void vga_font_shutdown(void);

// Returns true when the font sheet was successfully loaded.
bool vga_font_loaded(void);

// Font layout queries (valid after vga_font_init returns true).
int           vga_char_width(void);
int           vga_char_height(void);
uint32_t      vga_font_texture_id(void);
VgaFontLayout vga_get_font_layout(void);

// Draw a single character cell at screen position (x, y).
// fg / bg are 0xAARRGGBB; bg is drawn as a solid filled rectangle.
void vga_draw_char(int ch, int x, int y, uint32_t fg, uint32_t bg);

// Draw a NUL-terminated string on a single line at (x, y).
// Interprets only printable ASCII; other bytes are drawn as a solid block.
// Returns the total pixel width rendered.
int vga_draw_text(const char *text, int x, int y, uint32_t fg, uint32_t bg);

// Draw at most max_chars characters from text, honouring the same rules as
// vga_draw_text.
int vga_draw_textn(const char *text, int max_chars,
                   int x, int y, uint32_t fg, uint32_t bg);

// Pixel width of a string (= char_count * cell width).
int vga_text_width(int char_count);

#endif /* __UI_VGA_FONT_H__ */
