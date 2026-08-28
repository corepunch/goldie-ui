// UI Framework Hello World Example
// Demonstrates basic window creation and text display using the UI framework

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// Include UI framework headers
#include <orion/ui.h>
#include <orion/gem.h>

// Button ID constant
#define ID_BUTTON_CLICKME 101

// Click counter
static int click_count = 0;

// Simple window procedure for our hello world window
result_t hello_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate: {
      // Create a label
      create_window("UI Framework Demo:", WINDOW_NOTITLE, MAKERECT(20, 20, 200, 20), win, win_label, 0, NULL);
      // Create a button and assign it an ID
      window_t *button = create_window("Click Me!", WINDOW_NOTITLE, MAKERECT(20, 40, 100, 0), win, win_button, 0, NULL);
      button->id = ID_BUTTON_CLICKME;
      // Create first checkbox
      create_window("Enable Feature A", WINDOW_NOTITLE, MAKERECT(20, 60, 150, 20), win, win_checkbox, 0, NULL);      
      // Create second checkbox
      create_window("Enable Feature B", WINDOW_NOTITLE, MAKERECT(20, 80, 150, 20), win, win_checkbox, 0, NULL);
      return true;
    }
      
     case evPaint: {
       // Draw click counter text or hello world
       char text[64];
       if (click_count == 0) {
         strcpy(text, "Hello World!");
       } else if (click_count == 1) {
         strcpy(text, "Clicked 1 time");
       } else {
         snprintf(text, sizeof(text), "Clicked %d times", click_count);
       }
       
       int text_x = (win->frame.w - strwidth(text)) / 2;
       int text_y = 8;
      
       // Draw text with shadow effect
       draw_text_small(text, text_x + 1, text_y + 1, get_sys_color(brDarkEdge));
       draw_text_small(text, text_x, text_y, get_sys_color(brTextNormal));
       return false;
     }
    
     case evCommand:
       // Handle button click
       if (HIWORD(wparam) == btnClicked && LOWORD(wparam) == ID_BUTTON_CLICKME) {
         click_count++;
         invalidate_window(win);  // Request repaint to show new count
         return true;
       }
       return false;
    
    case evDestroy:
      ui_request_quit();
      return true;
      
    default:
      return false;
  }
}

// ---------------------------------------------------------------------------
// .gem entry points — called by the shell when loaded as a .gem
// ---------------------------------------------------------------------------

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
  (void)argc; (void)argv;
  register_commctl_classes();
  window_t *win = create_window(
    "Hello World Window",
    0,
    MAKERECT(20, 20, 240, 180 + TITLEBAR_HEIGHT),
    NULL,
    hello_window_proc,
    hinstance,
    NULL
  );
  if (!win) return false;
  show_window(win, true);
  return true;
}

void gem_shutdown(void) {
}

GEM_DEFINE("Hello World", "1.0", gem_init, gem_shutdown, NULL)

GEM_STANDALONE_MAIN("Hello World", UI_INIT_DESKTOP | UI_INIT_TRAY,
                    320, 240, NULL, NULL)
