#include "test_framework.h"
#include "test_env.h"
#include "../ui.h"

// Simple window procedure for test forms
static result_t test_form_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  (void)win; (void)msg; (void)wparam; (void)lparam;
  return false;
}

// Test that reportview always has WINDOW_FLEXSPACE regardless of explicit flags.
// Class defaults should be merged with instance flags, not replaced.
static void test_reportview_class_flags_not_overrideable(void) {
  TEST("reportview: WINDOW_FLEXSPACE added by class even when absent from instance flags");
  test_env_init();

  form_ctrl_def_t reportview_form = {
    .class_name = "reportview",
    .id = 1,
    .size = {0, 0},
    .flags = WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_VSCROLL,
    .text = "",
    .name = "test_rv",
  };
  form_def_t def = {
    .name = "Test Class Defaults",
    .width = 300,
    .height = 200,
    .flags = (0) | WINDOW_AUTO_LAYOUT,
    .children = &reportview_form,
    .child_count = 1,
  };

  window_t *win = create_window_from_form(&def, 0, 0, NULL, test_form_proc, 0, NULL);
  ASSERT_NOT_NULL(win);

  window_t *rv = win->children;
  ASSERT_NOT_NULL(rv);
  ASSERT_EQUAL(rv->id, 1);
  ASSERT_TRUE((rv->flags & WINDOW_FLEXSPACE) != 0);
  ASSERT_TRUE((rv->flags & WINDOW_NOTITLE)   != 0);
  ASSERT_TRUE((rv->flags & WINDOW_NORESIZE)  != 0);
  ASSERT_TRUE((rv->flags & WINDOW_VSCROLL)   != 0);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

// Test that space element gets WINDOW_FLEXSPACE from class defaults.
static void test_space_class_has_flexspace(void) {
  TEST("space: WINDOW_FLEXSPACE set from class defaults");
  test_env_init();

  form_ctrl_def_t space_form = {
    .class_name = "space",
    .id = 2,
    .size = {0, 0},
    .flags = 0,
    .text = "",
    .name = "spacer",
  };
  form_def_t def = {
    .name = "Test Space",
    .width = 300,
    .height = 100,
    .flags = (0) | WINDOW_AUTO_LAYOUT,
    .children = &space_form,
    .child_count = 1,
  };

  window_t *win = create_window_from_form(&def, 0, 0, NULL, test_form_proc, 0, NULL);
  ASSERT_NOT_NULL(win);

  window_t *space = win->children;
  ASSERT_NOT_NULL(space);
  ASSERT_TRUE((space->flags & WINDOW_FLEXSPACE) != 0);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

// Test that multiedit always has WINDOW_FLEXSPACE and keeps explicit flags.
static void test_multiedit_class_has_flexspace(void) {
  TEST("multiedit: WINDOW_FLEXSPACE from class; explicit WINDOW_NOTITLE preserved");
  test_env_init();

  form_ctrl_def_t multiedit_form = {
    .class_name = "multiedit",
    .id = 3,
    .size = {0, 0},
    .flags = WINDOW_NOTITLE,
    .text = "",
    .name = "editor",
  };
  form_def_t def = {
    .name = "Test Multiedit",
    .width = 300,
    .height = 200,
    .flags = (0) | WINDOW_AUTO_LAYOUT,
    .children = &multiedit_form,
    .child_count = 1,
  };

  window_t *win = create_window_from_form(&def, 0, 0, NULL, test_form_proc, 0, NULL);
  ASSERT_NOT_NULL(win);

  window_t *me = win->children;
  ASSERT_NOT_NULL(me);
  ASSERT_TRUE((me->flags & WINDOW_FLEXSPACE) != 0);
  ASSERT_TRUE((me->flags & WINDOW_NOTITLE)   != 0);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

// Test that WINDOW_FLEXSPACE propagates from child to parent through nested
// layout containers: reportview -> column (stack) -> grid -> form.
static void test_flexspace_propagates_up(void) {
  TEST("WINDOW_FLEXSPACE propagates up: reportview -> column -> grid -> form");
  test_env_init();

  form_ctrl_def_t reportview_form = {
    .class_name = "reportview",
    .id = 10,
    .size = {0, 0},
    .flags = WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_VSCROLL,
    .text = "",
    .name = "items",
  };
  form_ctrl_def_t column_form = {
    .class_name = "stack",
    .id = 11,
    .size = {0, 0},
    .flags = 0,
    .text = "",
    .name = "col",
    .children = &reportview_form,
    .child_count = 1,
  };
  form_ctrl_def_t grid_form = {
    .class_name = "grid",
    .id = 12,
    .size = {0, 0},
    .flags = 0,
    .text = "",
    .name = "main",
    .children = &column_form,
    .child_count = 1,
  };
  form_def_t def = {
    .name = "Test Propagation",
    .width = 400,
    .height = 300,
    .flags = (0) | WINDOW_AUTO_LAYOUT,
    .children = &grid_form,
    .child_count = 1,
  };

  window_t *win = create_window_from_form(&def, 0, 0, NULL, test_form_proc, 0, NULL);
  ASSERT_NOT_NULL(win);

  // Navigate form -> grid -> column -> reportview (each ASSERT_NOT_NULL returns
  // from the function if the pointer is NULL, preventing null-deref below).
  window_t *grid = win->children;
  ASSERT_NOT_NULL(grid);

  window_t *column = grid->children;
  ASSERT_NOT_NULL(column);

  window_t *rv = column->children;
  ASSERT_NOT_NULL(rv);
  ASSERT_EQUAL(rv->id, 10);

  ASSERT_TRUE((rv->flags     & WINDOW_FLEXSPACE) != 0);
  ASSERT_TRUE((column->flags & WINDOW_FLEXSPACE) != 0);
  ASSERT_TRUE((grid->flags   & WINDOW_FLEXSPACE) != 0);
  ASSERT_TRUE((win->flags    & WINDOW_FLEXSPACE) != 0);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  TEST_START("Form class default flags merge");

  test_reportview_class_flags_not_overrideable();
  test_space_class_has_flexspace();
  test_multiedit_class_has_flexspace();
  test_flexspace_propagates_up();

  TEST_END();
}
