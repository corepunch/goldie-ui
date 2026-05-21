// Test: Verify space element expands in auto-layout.

#include "../ui.h"
#include "test_framework.h"
#include "test_env.h"

static result_t nop_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
    (void)win;
    (void)msg;
    (void)wparam;
    (void)lparam;
    return false;
}

void test_space_has_flexspace_by_default(void) {
    TEST("Space element gets WINDOW_FLEXSPACE from component descriptor");
    
    test_env_init();

    form_ctrl_def_t children[] = {
        { .class_name = "Space", .id = 1, .size = {0, 0}, .name = "spacer" },
        { .class_name = "Button", .id = 2, .size = {40, 19}, .text = "OK", .name = "ok" },
    };
    form_def_t def = {
        .name = "Space Test",
        .width = 200,
        .height = 40,
        .flags = WINDOW_AUTO_LAYOUT,
        .children = children,
        .child_count = 2,
    };

    window_t *win = create_window_from_form(&def, 0, 0, NULL, nop_proc, 0, NULL);
    if (!win) {
        printf("FAIL: create_window_from_form returned NULL\n");
        test_env_shutdown();
        return;
    }

    window_t *space = get_window_item(win, 1);
    if (!space) {
        printf("FAIL: space child missing\n");
        destroy_window(win);
        test_env_shutdown();
        return;
    }

    if ((space->flags & WINDOW_FLEXSPACE) == 0) {
        printf("FAIL: space child missing WINDOW_FLEXSPACE\n");
        destroy_window(win);
        test_env_shutdown();
        return;
    }
    
    destroy_window(win);
    test_env_shutdown();
    PASS();
}

int main(void) {
    TEST_START("space element flexspace");
    
    test_space_has_flexspace_by_default();
    
    TEST_END();
}
