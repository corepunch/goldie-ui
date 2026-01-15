// UI Framework Hello World Example
// Demonstrates basic window creation and text display using the UI framework

#include <stdio.h>
#include <stdbool.h>

// Include UI framework headers
#include "../ui.h"

// Running flag
// bool running = true;
extern bool running;

// Simple window procedure for our hello world window
result_t hello_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      // Create a label
      create_window("UI Framework Demo:", WINDOW_NOTITLE, MAKERECT(20, 20, 200, 20), win, win_label, NULL);
      // Create a button
      create_window("Click Me!", WINDOW_NOTITLE, MAKERECT(20, 50, 100, 30), win, win_button, NULL);
      // Create first checkbox
      create_window("Enable Feature A", WINDOW_NOTITLE, MAKERECT(20, 90, 150, 20), win, win_checkbox, NULL);      
      // Create second checkbox
      create_window("Enable Feature B", WINDOW_NOTITLE, MAKERECT(20, 120, 150, 20), win, win_checkbox, NULL);
      return true;
      
     case WM_PAINT: {
       // Draw hello world text
       const char *text = "Hello World!";
       int text_x = win->frame.w / 2 - 40;  // Center approximately
       int text_y = win->frame.h / 2 - 10;
      
       // Draw text with shadow effect
       draw_text_small(text, text_x + 1, text_y + 1, COLOR_DARK_EDGE);
       draw_text_small(text, text_x, text_y, COLOR_TEXT_NORMAL);
       return false;
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

  // Initialize graphics system (SDL + OpenGL abstracted)
  if (!ui_init_graphics(0, "UI Framework - Hello World", 640, 480)) {
    printf("Failed to initialize graphics!\n");
    return 1;
  }

  printf("Graphics initialized successfully\n");
  printf("Creating window with UI framework...\n");

  // Create main window
  window_t *main_window = create_window(
    "Hello World Window",          // Window title
    0,                             // Window flags
    MAKERECT(20, 20, 240, 180),    // Position and size
    NULL,                          // No parent window
    hello_window_proc,             // Window procedure
    NULL
  );

  if (!main_window) {
    printf("Failed to create window!\n");
    ui_shutdown_graphics();
    return 1;
  }

  show_window(main_window, true);

  printf("Window created successfully\n");

  // Main event loop
  ui_event_t e;
  while (running) {
    // Process events
    while (get_message(&e)) {
      dispatch_message(&e);
    }

    // Process window messages
    repost_messages();
  }

  destroy_window(main_window);

  // Cleanup
  printf("Shutting down...\n");
  ui_shutdown_graphics();

  printf("Goodbye!\n");
  return 0;
}
