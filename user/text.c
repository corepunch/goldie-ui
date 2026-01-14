// Text rendering implementation
// Extracted from mapview/windows/console.c
// Contains only the small embedded font rendering (console_font_6x8: 6-bit wide, 8 pixels tall)

#include <SDL2/SDL.h>
#include "gl_compat.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "text.h"

#define FONT_TEX_SIZE 128
#define MAX_TEXT_LENGTH 256
#define SMALL_FONT_WIDTH 8
#define SMALL_FONT_HEIGHT 8

typedef struct {
  int16_t x, y;
  float u, v;
  uint32_t col;
} text_vertex_t;

// Font atlas structure
typedef struct {
  GLuint vao, vbo;
  GLuint texture;    // Atlas texture ID
  uint8_t char_from[256];    // Start position of each character in pixels
  uint8_t char_to[256];      // End position of each character in pixels
  uint8_t char_height;       // Height of each character in pixels
  uint8_t chars_per_row;     // Number of characters per row in atlas
  uint8_t total_chars;       // Total number of characters in atlas
} font_atlas_t;

// Text rendering state
static struct {
  font_atlas_t small_font;   // Small 6x8 font atlas
} text_state = {0};

// Forward declarations for external functions
extern void push_sprite_args(int tex, int x, int y, int w, int h, float alpha);

// Create texture atlas for the small 6x8 font
static bool create_font_atlas(void) {
  extern unsigned char console_font_6x8[];
  // Font atlas dimensions
  const int char_width = SMALL_FONT_WIDTH;
  const int char_height = SMALL_FONT_HEIGHT;
  const int chars_per_row = 16;
  const int rows = 8;      // 16 * 8 = 128 ASCII characters (0-127)
  
  // Create a buffer for the atlas texture
  unsigned char* atlas_data = (unsigned char*)calloc(FONT_TEX_SIZE * FONT_TEX_SIZE, sizeof(unsigned char));
  if (!atlas_data) {
    printf("Error: Could not allocate memory for font atlas\n");
    return false;
  }
  
  // Fill the atlas with character data from the font_6x8 array
  for (int c = 0; c < 128; c++) {
    int atlas_x = (c % chars_per_row) * char_width;
    int atlas_y = (c / chars_per_row) * char_height;
    // Copy character bits from font data to atlas
    text_state.small_font.char_to[c] = 0;
    text_state.small_font.char_from[c] = 0xff;
    for (int y = 0; y < char_height; y++) {
      for (int x = 0; x < char_width; x++) {
        // Get bit from font data (assuming 1 byte per row, 8 rows per character)
        int bit_pos = x;
        int font_byte = console_font_6x8[c * char_height + y];
        int bit_value = ((font_byte >> (char_width - 1 - bit_pos)) & 1);
        // Set corresponding pixel in atlas (convert 1-bit to 8-bit)
        atlas_data[(atlas_y + y) * FONT_TEX_SIZE + atlas_x + x] = bit_value ? 255 : 0;
        if (bit_value) {
          text_state.small_font.char_from[c] = (x < text_state.small_font.char_from[c]) ? x : text_state.small_font.char_from[c];
          text_state.small_font.char_to[c] = ((x+2) > text_state.small_font.char_to[c]) ? (x+2) : text_state.small_font.char_to[c];
        }
      }
    }
  }
  
  extern unsigned char icons_bits[];
  size_t half = FONT_TEX_SIZE * FONT_TEX_SIZE / 2;
  memcpy(atlas_data + half, icons_bits, half);
  
  for (int i = 128; i < 256; i++) {
    text_state.small_font.char_to[i] = 8;
    text_state.small_font.char_from[i] = 0;
  }
  
  // Create OpenGL texture for the atlas
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  
  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  
  // Use texture swizzling for proper rendering (R channel becomes alpha)
  GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
  
  // Upload texture data
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_TEX_SIZE, FONT_TEX_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, atlas_data);
  
  // Store atlas information
  text_state.small_font.texture = texture;
  text_state.small_font.char_height = char_height;
  text_state.small_font.chars_per_row = chars_per_row;
  text_state.small_font.total_chars = chars_per_row * rows;
  
  // Free temporary buffer
  free(atlas_data);
  
  printf("Small font atlas created successfully\n");
  
  glGenVertexArrays(1, &text_state.small_font.vao);
  glGenBuffers(1, &text_state.small_font.vbo);
  
  glBindVertexArray(text_state.small_font.vao);
  glBindBuffer(GL_ARRAY_BUFFER, text_state.small_font.vbo);

  return true;
}

// Initialize text rendering system
void init_text_rendering(void) {
  memset(&text_state, 0, sizeof(text_state));
  create_font_atlas();
}

// Get width of text substring with small font
int strnwidth(const char* text, int text_length) {
  if (!text || !*text) return 0; // Early return for empty strings
  
  if (text_length > MAX_TEXT_LENGTH) text_length = MAX_TEXT_LENGTH;
  
  int cursor_x = 0;
  
  // Pre-calculate all vertices for the entire string
  for (int i = 0; i < text_length; i++) {
    char c = text[i];
    if (c == ' ') {
      cursor_x += 3;
      continue;
    }
    uint8_t w = text_state.small_font.char_to[c] - text_state.small_font.char_from[c];
    // Advance cursor position
    cursor_x += w;
  }
  return cursor_x;
}

// Get width of text with small font
int strwidth(const char* text) {
  if (!text || !*text) return 0; // Early return for empty strings
  return strnwidth(text, (int)strlen(text));
}

// Draw text using small bitmap font
void draw_text_small(const char* text, int x, int y, uint32_t col) {
  if (!text || !*text) return; // Early return for empty strings
  
  int text_length = (int)strlen(text);
  if (text_length > MAX_TEXT_LENGTH) text_length = MAX_TEXT_LENGTH;
  
  static text_vertex_t buffer[MAX_TEXT_LENGTH * 6]; // 6 vertices per character (2 triangles)
  int vertex_count = 0;
  
  int cursor_x = x;
  
  // Pre-calculate all vertices for the entire string
  for (int i = 0; i < text_length; i++) {
    unsigned char c = text[i];
    
    if (c == ' ') {
      cursor_x += 3;
      continue;
    }
    if (c == '\n') {
      cursor_x = x;
      y += SMALL_FONT_HEIGHT;
      continue;
    }
    
    // Calculate texture coordinates
    int atlas_x = (c % text_state.small_font.chars_per_row) * SMALL_FONT_WIDTH;
    int atlas_y = (c / text_state.small_font.chars_per_row) * SMALL_FONT_HEIGHT;
    
    // Convert to normalized UV coordinates (0-255 range for uint8_t)
    float u1 = (atlas_x + text_state.small_font.char_from[c])/(float)FONT_TEX_SIZE;
    float v1 = (atlas_y)/(float)FONT_TEX_SIZE;
    float u2 = (atlas_x + text_state.small_font.char_to[c])/(float)FONT_TEX_SIZE;
    float v2 = (atlas_y + SMALL_FONT_HEIGHT)/(float)FONT_TEX_SIZE;
    
    uint8_t w = text_state.small_font.char_to[c] - text_state.small_font.char_from[c];
    uint8_t h = SMALL_FONT_HEIGHT;
    
    // Skip spaces to save rendering effort
    if (c != ' ') {
      // First triangle (bottom-left, top-left, bottom-right)
      buffer[vertex_count++] = (text_vertex_t) { cursor_x, y, u1, v1, col };
      buffer[vertex_count++] = (text_vertex_t) { cursor_x, y + h, u1, v2, col };
      buffer[vertex_count++] = (text_vertex_t) { cursor_x + w, y, u2, v1, col };
      
      // Second triangle (top-left, top-right, bottom-right)
      buffer[vertex_count++] = (text_vertex_t) { cursor_x, y + h, u1, v2, col };
      buffer[vertex_count++] = (text_vertex_t) { cursor_x + w, y + h, u2, v2, col };
      buffer[vertex_count++] = (text_vertex_t) { cursor_x + w, y, u2, v1, col };
    }
    
    // Advance cursor position
    cursor_x += w;
  }
  
  // Early return if nothing to draw
  if (vertex_count == 0) return;
  
  // Set up GL state
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  
  // Get locations for shader uniforms
  push_sprite_args(text_state.small_font.texture, 0, 0, 1, 1, 1);
  
  // Bind font texture
  glBindTexture(GL_TEXTURE_2D, text_state.small_font.texture);
  
  // Bind and update the VBO with the vertex data
  glBindVertexArray(text_state.small_font.vao);
  glBindBuffer(GL_ARRAY_BUFFER, text_state.small_font.vbo);
  glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(text_vertex_t), buffer, GL_DYNAMIC_DRAW);
  
  // Set up vertex attributes
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0, 2, GL_SHORT, GL_FALSE, sizeof(text_vertex_t), (void*)offsetof(text_vertex_t, x)); // Position
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(text_vertex_t), (void*)offsetof(text_vertex_t, u)); // UV
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(text_vertex_t), (void*)offsetof(text_vertex_t, col)); // Color

  // Draw all vertices in one call
  glDrawArrays(GL_TRIANGLES, 0, vertex_count);
  
  // Clean up
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  
  // Restore GL state
  // Note: Commented out to match original behavior
  // glEnable(GL_DEPTH_TEST);
  // glDisable(GL_BLEND);
}

// Clean up text rendering resources
void shutdown_text_rendering(void) {
  // Delete small font resources
  if (text_state.small_font.texture != 0) {
    glDeleteTextures(1, &text_state.small_font.texture);
    text_state.small_font.texture = 0;
  }
  if (text_state.small_font.vao != 0) {
    glDeleteVertexArrays(1, &text_state.small_font.vao);
    text_state.small_font.vao = 0;
  }
  if (text_state.small_font.vbo != 0) {
    glDeleteBuffers(1, &text_state.small_font.vbo);
    text_state.small_font.vbo = 0;
  }
}
