// Two Windows Test - Tests multiple window visibility
// This is to verify the fix for the stencil buffer rendering order issue

#include <stdio.h>
#include <stdbool.h>

// Include UI framework headers
#include "../ui.h"

// Running flag
extern bool running;

// Simple window procedure for first window
result_t window1_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      // Create a label
      create_window("First Window Content", WINDOW_NOTITLE, MAKERECT(10, 10, 150, 20), win, win_label, NULL);
      // Create a button
      create_window("Button 1", WINDOW_NOTITLE, MAKERECT(10, 40, 80, 25), win, win_button, NULL);
      return true;
      
     case WM_PAINT: {
       // Draw window 1 background
       fill_rect(0xFF4444FF, 0, 0, win->frame.w, win->frame.h);
       return false;
     }
    
    case WM_DESTROY:
      return true;
      
    default:
      return false;
  }
}

// Simple window procedure for second window
result_t window2_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      // Create a label
      create_window("Second Window Content", WINDOW_NOTITLE, MAKERECT(10, 10, 150, 20), win, win_label, NULL);
      // Create a button
      create_window("Button 2", WINDOW_NOTITLE, MAKERECT(10, 40, 80, 25), win, win_button, NULL);
      return true;
      
     case WM_PAINT: {
       // Draw window 2 background
       fill_rect(0xFF44FF44, 0, 0, win->frame.w, win->frame.h);
       return false;
     }
    
    case WM_DESTROY:
      return true;
      
    default:
      return false;
  }
}

// Simple main function
int main(int argc, char* argv[]) {
  printf("Two Windows Test - Testing Multiple Window Visibility\n");

  // Initialize graphics system
  if (!ui_init_graphics(UI_INIT_DESKTOP, "Two Windows Test", 320, 240)) {
    printf("Failed to initialize graphics!\n");
    return 1;
  }

  printf("Graphics initialized successfully\n");
  printf("Creating two windows...\n");

  // Create first window
  window_t *window1 = create_window(
    "First Window",                // Window title
    0,                             // Window flags
    MAKERECT(20, 20, 140, 100),    // Position and size
    NULL,                          // No parent window
    window1_proc,                  // Window procedure
    NULL
  );

  if (!window1) {
    printf("Failed to create first window!\n");
    ui_shutdown_graphics();
    return 1;
  }

  // Create second window
  window_t *window2 = create_window(
    "Second Window",               // Window title
    0,                             // Window flags
    MAKERECT(100, 80, 140, 100),   // Position and size - overlapping
    NULL,                          // No parent window
    window2_proc,                  // Window procedure
    NULL
  );

  if (!window2) {
    printf("Failed to create second window!\n");
    destroy_window(window1);
    ui_shutdown_graphics();
    return 1;
  }

  show_window(window1, true);
  show_window(window2, true);

  printf("Both windows created successfully\n");
  printf("You should see TWO windows on screen\n");
  printf("Window 1 has red background, Window 2 has green background\n");

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

  destroy_window(window1);
  destroy_window(window2);

  // Cleanup
  printf("Shutting down...\n");
  ui_shutdown_graphics();

  printf("Goodbye!\n");
  return 0;
}
