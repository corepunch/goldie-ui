// Graphics context initialization and management
// Abstraction layer over SDL/OpenGL

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

#include "../user/gl_compat.h"
#include "../user/user.h"
#include "../commctl/commctl.h"
#include "kernel.h"

// Global SDL objects
SDL_Window* window = NULL;
SDL_GLContext ctx;

bool ui_init_prog(void);
void ui_shutdown_prog(void);

// Internal white texture for drawing solid colors
GLuint ui_white_texture = 0;

// Initialize the internal white texture
void init_ui_white_texture(void) {
  if (ui_white_texture == 0) {
    glGenTextures(1, &ui_white_texture);
    glBindTexture(GL_TEXTURE_2D, ui_white_texture);
    uint32_t white_pixel = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
}

void shutdown_white_texture(void) {
  SAFE_DELETE_N(ui_white_texture, glDeleteTextures);
}

// Initialize window and OpenGL context
static bool ui_init_window(const char *title, int width, int height) {
  // Set OpenGL attributes before creating window
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  
  // Check for multiple displays
  int numDisplays = SDL_GetNumVideoDisplays();
  if (numDisplays < 2) {
    window = SDL_CreateWindow(title,
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              width, height,
                              SDL_WINDOW_OPENGL|SDL_WINDOW_INPUT_FOCUS);
  } else {
    SDL_Rect bounds;
    if (SDL_GetDisplayBounds(1, &bounds) != 0) {
      SDL_Log("SDL_GetDisplayBounds failed: %s", SDL_GetError());
      // Fallback to primary display with undefined position
      window = SDL_CreateWindow(title,
                                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                width, height,
                                SDL_WINDOW_OPENGL|SDL_WINDOW_INPUT_FOCUS);
    } else {
      // Centered position on display 1
      int w = width;
      int h = height;
      int x = bounds.x + (bounds.w - w) / 2;
      int y = bounds.y + (bounds.h - h) / 2;
      
      window = SDL_CreateWindow(title, x, y, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_FOCUS);
    }
  }
  
  if (!window) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }
  
  // Create OpenGL context
  ctx = SDL_GL_CreateContext(window);
  if (!ctx) {
    printf("OpenGL context could not be created! SDL_Error: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    window = NULL;
    return false;
  }
  
  printf("GL_VERSION  : %s\n", glGetString(GL_VERSION));
  printf("GLSL_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
  
  return true;
}

static result_t win_desktop(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  extern void fill_rect(int color, int x, int y, int w, int h);
  switch (msg) {
    case WM_PAINT:
      fill_rect(0xff6B3529, 0, 0, ui_get_system_metrics(SM_CXSCREEN), ui_get_system_metrics(SM_CYSCREEN));
      return true;
  }
  return false;
}

result_t win_tray(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Initialize graphics context (SDL + OpenGL)
// This is a convenience function that initializes SDL and creates window/context
bool ui_init_graphics(int flags, const char *title, int width, int height) {
  //  printf("%s\n", cache_lump("MAPINFO"));
  if (SDL_Init(SDL_INIT_VIDEO|flags) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return false;
  }
  // Use ui_init_window to create window and context
  if (!ui_init_window(title, width * UI_WINDOW_SCALE, height * UI_WINDOW_SCALE)) {
    SDL_Quit();
    return false;
  }

  // Enable VSync
  SDL_GL_SetSwapInterval(1);
  
  ui_init_prog();
  
  init_ui_white_texture();

  init_console();
  
  if (flags & UI_INIT_DESKTOP) {
    show_window(create_window("Desktop",
                              WINDOW_NOTITLE|WINDOW_ALWAYSINBACK|WINDOW_NOTRAYBUTTON,
                              MAKERECT(0, 0, ui_get_system_metrics(SM_CXSCREEN), ui_get_system_metrics(SM_CYSCREEN)),
                              NULL, win_desktop, NULL), true);
  }
  
  if (flags & UI_INIT_TRAY) {
    show_window(create_window("Tray",
                              WINDOW_NOTITLE|WINDOW_NOTRAYBUTTON,
                              MAKERECT(0, 0, 0, 0),
                              NULL, win_tray, NULL), true);
  }

  running = true;

  return true;
}

// Cleanup all windows
static void cleanup_all_windows(void) {
  extern window_t *windows;
  
  // Destroy all remaining top-level windows
  while (windows) {
    destroy_window(windows);
  }
}

// Shutdown graphics context
void ui_shutdown_graphics(void) {
  // Clean up all windows before shutting down subsystems
  cleanup_all_windows();
  
  // Clean up any remaining hooks
  extern void cleanup_all_hooks(void);
  cleanup_all_hooks();
  
  // Shutdown joystick if it was initialized
  if (ui_joystick_available()) {
    ui_joystick_shutdown();
  }
  
  ui_shutdown_prog();
  
  shutdown_white_texture();

  shutdown_console();

  if (ctx) {
    SDL_GL_DeleteContext(ctx);
    ctx = NULL;
  }
  
  if (window) {
    SDL_DestroyWindow(window);
    window = NULL;
  }
  
  SDL_Quit();
}

// Delay execution (abstraction over SDL_Delay)
void ui_delay(unsigned int milliseconds) {
  SDL_Delay(milliseconds);
}
