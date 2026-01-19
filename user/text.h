#ifndef __UI_TEXT_H__
#define __UI_TEXT_H__

#include <stdint.h>
#include <stdbool.h>

#define CHAR_HEIGHT 8

// Forward declaration
typedef struct rect_s rect_t;

// Initialize the text rendering system
void init_text_rendering(void);

// Clean up text rendering resources
void shutdown_text_rendering(void);

// Small bitmap font rendering (6x8 font)
void draw_text_small(const char* text, int x, int y, uint32_t col);
int strwidth(const char* text);
int strnwidth(const char* text, int text_length);

// Advanced text rendering with wrapping and viewport clipping
int calc_text_height(const char* text, int width);
void draw_text_wrapped(const char* text, rect_t const *viewport, uint32_t col);

#endif // __UI_TEXT_H__
