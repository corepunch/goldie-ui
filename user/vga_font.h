// VGA-style monospace font renderer.
//
// Loads a TTF/OTF font and generates a 128x256 RGBA character sheet at
// runtime (16 columns x 16 rows, each cell 8x16 pixels).  Uses
// stb_truetype.h to rasterise glyphs into the sheet on first load.
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

// Physical dimensions of one character cell in screen pixels.
// These values control on-screen character size; increase them (for example
// to 128x128) to render larger glyphs while preserving font texture detail.
#define VGA_CHAR_W   8
#define VGA_CHAR_H   16

// Initialise the VGA font renderer by loading a TTF font and generating the
// character sheet on the fly.  ttf_path points to the .ttf file (relative to
// the exe dir or absolute).  pixel_height controls the em-square height in
// pixels (pass 0 for a sensible default of VGA_CHAR_H).
// Must be called after ui_init_graphics().
// Returns true on success, false when the file is missing or the GL texture
// cannot be created.  When false, all draw calls become no-ops.
bool vga_font_init(const char *ttf_path, int pixel_height);


// Release the GL texture.  Safe to call even when init failed.
void vga_font_shutdown(void);

// Returns true when the font sheet was successfully loaded.
bool vga_font_loaded(void);

// Returns the renderer texture ID for the loaded VGA sheet, or 0 if not loaded.
uint32_t vga_font_texture_id(void);

// Draw a single character cell at screen position (x, y).
// fg / bg are 0xAARRGGBB; bg is drawn as a solid filled rectangle.
void vga_draw_char(int ch, int x, int y, uint32_t fg, uint32_t bg);

// Draw a NUL-terminated string on a single line at (x, y).
// Interprets only printable ASCII; other bytes are drawn as a solid block.
// Returns the total pixel width rendered (= strlen(text) * VGA_CHAR_W).
int vga_draw_text(const char *text, int x, int y, uint32_t fg, uint32_t bg);

// Draw at most max_chars characters from text, honouring the same rules as
// vga_draw_text.
int vga_draw_textn(const char *text, int max_chars,
                   int x, int y, uint32_t fg, uint32_t bg);

// Pixel width of a string (= strlen * VGA_CHAR_W, independent of content).
int vga_text_width(int char_count);

#endif /* __UI_VGA_FONT_H__ */
