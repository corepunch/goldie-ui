// UI Framework Hello World Example
// Demonstrates basic window creation and text display using the UI framework

#include <stdio.h>
#include <stdbool.h>

// Include UI framework headers
#include "../ui.h"

// Screen dimensions
int screen_width = 800;
int screen_height = 600;

// Running flag
bool running = true;

// Simple window procedure for our hello world window
result_t hello_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      return true;
      
    case WM_PAINT: {
      // Draw hello world text
      const char *text = "Hello World!";
      int text_x = win->frame.w / 2 - 40;  // Center approximately
      int text_y = win->frame.h / 2 - 10;
      
      // Draw text with shadow effect
      draw_text_small(text, text_x + 1, text_y + 1, COLOR_DARK_EDGE);
      draw_text_small(text, text_x, text_y, COLOR_TEXT_NORMAL);
      return true;
    }
    
    case WM_DESTROY:
      running = false;
      return true;
      
    default:
      return false;
  }
}

// Simple main function
int main(int argc, char* argv[]) {
  printf("UI Framework Hello World Example\n");
  printf("=================================\n\n");

  // Initialize graphics system (SDL + OpenGL abstracted)
  if (!ui_init_graphics(0, "UI Framework - Hello World", 640, 480)) {
    printf("Failed to initialize graphics!\n");
    return 1;
  }

  printf("Graphics initialized successfully\n");
  printf("Creating window with UI framework...\n");

  // Main event loop
  ui_event_t e;
  while (running) {
    // Process events
    while (get_message(&e)) {
      dispatch_message(&e);
    }

    // Process window messages
    repost_messages();

    // Begin frame
    ui_begin_frame();
    
    // Clear screen (blue-ish background)
    ui_clear_screen(0.2f, 0.2f, 0.3f);

    // Draw all windows
    draw_windows(true);

    // End frame and swap buffers
    ui_end_frame();
    ui_swap_buffers();
    
    // Small delay to avoid busy waiting
    ui_delay(16);  // ~60 FPS
  }

  // Cleanup
  printf("Shutting down...\n");
  ui_shutdown_graphics();

  printf("Goodbye!\n");
  return 0;
}
