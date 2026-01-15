// Test for Hello World button click functionality
// Tests that button clicks increment the counter and update the display text

#include "test_framework.h"
#include "test_env.h"
#include "../ui.h"

#include <string.h>

// Forward declaration for message queue processing
extern void repost_messages(void);

// Button ID constant (same as in helloworld.c)
#define ID_BUTTON_CLICKME 101

// Test state tracking
static int test_bn_clicked_count = 0;
static uint32_t test_last_button_id = 0;
static int click_count = 0;

// Window procedure that mimics the hello world example
result_t test_hello_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE: {
      // Create a button and assign it an ID
      window_t *button = create_window("Click Me!", WINDOW_NOTITLE, MAKERECT(20, 50, 100, 0), win, win_button, NULL);
      button->id = ID_BUTTON_CLICKME;
      return true;
    }
      
    case WM_PAINT: {
      // We won't actually render but we can test the logic
      return false;
    }
    
    case WM_COMMAND:
      // Handle button click
      if (HIWORD(wparam) == BN_CLICKED && LOWORD(wparam) == ID_BUTTON_CLICKME) {
        click_count++;
        test_bn_clicked_count++;
        test_last_button_id = LOWORD(wparam);
        invalidate_window(win);  // Request repaint to show new count
        return true;
      }
      return false;
    
    case WM_DESTROY:
      return true;
      
    default:
      return false;
  }
}

// Reset test counters
void reset_hello_test_counters(void) {
    test_bn_clicked_count = 0;
    test_last_button_id = 0;
    click_count = 0;
}

// Test that button has correct ID
void test_button_has_id(void) {
    TEST("Button is assigned ID_BUTTON_CLICKME");
    
    test_env_init();
    reset_hello_test_counters();
    
    // Create hello world window
    window_t *parent = test_env_create_window("Hello World Window", 20, 20, 240, 180,
                                               test_hello_window_proc, NULL);
    ASSERT_NOT_NULL(parent);
    
    // Find the button by ID
    window_t *button = get_window_item(parent, ID_BUTTON_CLICKME);
    ASSERT_NOT_NULL(button);
    ASSERT_EQUAL(button->id, ID_BUTTON_CLICKME);
    
    destroy_window(parent);
    test_env_shutdown();
    PASS();
}

// Test that clicking button increments counter
void test_button_click_increments_counter(void) {
    TEST("Button click increments counter");
    
    test_env_init();
    test_env_enable_tracking(true);
    test_env_clear_events();
    reset_hello_test_counters();
    
    // Create hello world window
    window_t *parent = test_env_create_window("Hello World Window", 20, 20, 240, 180,
                                               test_hello_window_proc, NULL);
    ASSERT_NOT_NULL(parent);
    
    // Find the button
    window_t *button = get_window_item(parent, ID_BUTTON_CLICKME);
    ASSERT_NOT_NULL(button);
    
    test_env_clear_events();
    
    // Simulate button click
    int button_center_x = button->frame.x + button->frame.w / 2;
    int button_center_y = button->frame.y + button->frame.h / 2;
    
    test_env_post_message(button, WM_LBUTTONDOWN, MAKEDWORD(button_center_x, button_center_y), NULL);
    repost_messages();
    
    test_env_post_message(button, WM_LBUTTONUP, MAKEDWORD(button_center_x, button_center_y), NULL);
    repost_messages();
    
    // Verify click was registered
    ASSERT_EQUAL(test_bn_clicked_count, 1);
    ASSERT_EQUAL(click_count, 1);
    ASSERT_EQUAL(test_last_button_id, ID_BUTTON_CLICKME);
    
    destroy_window(parent);
    test_env_shutdown();
    PASS();
}

// Test multiple button clicks
void test_multiple_button_clicks(void) {
    TEST("Multiple button clicks increment counter correctly");
    
    test_env_init();
    test_env_enable_tracking(true);
    test_env_clear_events();
    reset_hello_test_counters();
    
    // Create hello world window
    window_t *parent = test_env_create_window("Hello World Window", 20, 20, 240, 180,
                                               test_hello_window_proc, NULL);
    ASSERT_NOT_NULL(parent);
    
    // Find the button
    window_t *button = get_window_item(parent, ID_BUTTON_CLICKME);
    ASSERT_NOT_NULL(button);
    
    test_env_clear_events();
    
    int button_center_x = button->frame.x + button->frame.w / 2;
    int button_center_y = button->frame.y + button->frame.h / 2;
    
    // Click button 5 times
    for (int i = 1; i <= 5; i++) {
        test_env_post_message(button, WM_LBUTTONDOWN, MAKEDWORD(button_center_x, button_center_y), NULL);
        repost_messages();
        
        test_env_post_message(button, WM_LBUTTONUP, MAKEDWORD(button_center_x, button_center_y), NULL);
        repost_messages();
        
        // Verify counter incremented correctly
        ASSERT_EQUAL(click_count, i);
    }
    
    ASSERT_EQUAL(test_bn_clicked_count, 5);
    
    destroy_window(parent);
    test_env_shutdown();
    PASS();
}

int main(void) {
    TEST_START("Hello World Button Click Tests");
    
    test_button_has_id();
    test_button_click_increments_counter();
    test_multiple_button_clicks();
    
    TEST_END();
}
