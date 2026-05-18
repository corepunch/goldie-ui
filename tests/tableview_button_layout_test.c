// Test: Verify tableview + button row layout - only tableview should stretch
//
// Issue: In vertical stack with tableview (WINDOW_FLEXSPACE) and button row
// (no WINDOW_FLEXSPACE), both were being treated as stretchable instead of
// only the tableview expanding.

#include "../ui.h"
#include "test_framework.h"
#include "test_env.h"

// Simple window proc for testing
static result_t test_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
    (void)win; (void)wparam; (void)lparam;
    if (msg == evCreate) return 1;
    return 0;
}

// Test: Verify tableview expands, button row stays fixed
void test_tableview_button_layout(void) {
    TEST("Tableview expands vertically, button row fixed height");
    
    // Create main window with vertical stack
    irect16_t main_frame = {100, 100, 400, 300};
    window_t *main = create_window("Test", WINDOW_AUTO_LAYOUT, &main_frame,
                                   NULL, win_stack, 0, NULL);
    ASSERT_NOT_NULL(main);
    
    // Add tableview with WINDOW_FLEXSPACE (should expand)
    irect16_t tv_frame = {0, 0, 400, 200};
    window_t *tableview = create_window("TableView",
                                        WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL | WINDOW_FLEXSPACE,
                                        &tv_frame, main, win_reportview, 0, NULL);
    ASSERT_NOT_NULL(tableview);
    
    // Add separator
    irect16_t sep_frame = {0, 0, 400, 1};
    window_t *separator = create_window("", 0, &sep_frame, main, win_separator, 0, NULL);
    ASSERT_NOT_NULL(separator);
    
    // Add horizontal stack for buttons (no WINDOW_FLEXSPACE - should stay fixed)
    layout_view_config_t actions_cfg = {
        .orientation = WINDOW_STACK_HORIZONTAL,
        .spacing = 4,
        .padding = {0, 0, 0, 0},
        .margin = {0, 0, 0, 0}
    };
    irect16_t actions_frame = {0, 0, 400, 25};
    window_t *actions = create_window("", 0, &actions_frame,
                                      main, win_stack, 0, &actions_cfg);
    ASSERT_NOT_NULL(actions);
    
    // Add some buttons to the actions stack
    irect16_t btn_frame = {0, 0, 60, 19};
    window_t *btn1 = create_window("Button 1", 0, &btn_frame, actions, win_button, 0, NULL);
    window_t *btn2 = create_window("Button 2", 0, &btn_frame, actions, win_button, 0, NULL);
    
    // Add a space element with WINDOW_FLEXSPACE (like in socialfeed)
    window_t *space = create_window("", WINDOW_FLEXSPACE, &btn_frame, actions, win_space, 0, NULL);
    
    ASSERT_NOT_NULL(btn1);
    ASSERT_NOT_NULL(btn2);
    ASSERT_NOT_NULL(space);
    
    // Force layout sync
    window_layout_sync(main);
    
    // Get client rect
    irect16_t cr = get_client_rect(main);
    
    // Verify tableview has WINDOW_FLEXSPACE
    ASSERT((tableview->flags & WINDOW_FLEXSPACE) != 0, "tableview should have WINDOW_FLEXSPACE");
    
    // Verify actions stack does NOT have WINDOW_FLEXSPACE
    ASSERT((actions->flags & WINDOW_FLEXSPACE) == 0, "actions stack should NOT have WINDOW_FLEXSPACE");
    
    // Print layout info for debugging
    printf("\n    Main client rect: y=%d h=%d\n", cr.y, cr.h);
    printf("    Main padding: l=%d t=%d r=%d b=%d\n",
           main->layout.layout_padding.x, main->layout.layout_padding.y,
           main->layout.layout_padding.w, main->layout.layout_padding.h);
    printf("    Tableview: y=%d h=%d flags=0x%x (should expand)\n",
           tableview->frame.y, tableview->frame.h, tableview->flags);
    printf("    Separator: y=%d h=%d\n", separator->frame.y, separator->frame.h);
    printf("    Actions:   y=%d h=%d flags=0x%x (should be ~19-25px)\n",
           actions->frame.y, actions->frame.h, actions->flags);
    printf("    Actions padding: l=%d t=%d r=%d b=%d\n",
           actions->layout.layout_padding.x, actions->layout.layout_padding.y,
           actions->layout.layout_padding.w, actions->layout.layout_padding.h);
    printf("    Button1:   y=%d h=%d\n", btn1->frame.y, btn1->frame.h);
    printf("    Button2:   y=%d h=%d\n", btn2->frame.y, btn2->frame.h);
    printf("    Space:     y=%d h=%d flags=0x%x\n", space->frame.y, space->frame.h, space->flags);
    
    // Calculate expected layout:
    // - Separator: 1px
    // - Actions: ~19-25px (button height + padding)
    // - Tableview: remaining space
    int expected_fixed = separator->frame.h + 25; // separator + actions max
    int min_tableview_height = cr.h - expected_fixed - 16; // minus some padding/spacing
    
    // Verify tableview takes significant vertical space
    ASSERT(tableview->frame.h >= min_tableview_height,
           "tableview should take most of the vertical space");
    
    // Verify actions stack is at fixed height (not expanded)
    ASSERT(actions->frame.h >= 19, "actions height should be at least button height");
    ASSERT(actions->frame.h <= 35, "actions should not expand beyond button height + padding");
    
    // Verify no overlap
    ASSERT(separator->frame.y >= tableview->frame.y + tableview->frame.h,
           "separator should be below tableview");
    ASSERT(actions->frame.y >= separator->frame.y + separator->frame.h,
           "actions should be below separator");
    
    // Cleanup
    destroy_window(main);
    
    PASS();
}

int main(void) {
    TEST_START("tableview button layout");
    
    test_env_init();
    
    test_tableview_button_layout();
    
    test_env_shutdown();
    
    TEST_END();
}
