#include "../ui.h"
#include "test_env.h"

#define TEST_NAME "Form class default flags merge"

static int kTestsPassed = 0;
static int kTestsTotal = 0;

#define ASSERT(cond, msg) do { \
  kTestsTotal++; \
  if (cond) { \
    kTestsPassed++; \
  } else { \
    fprintf(stderr, "FAIL: %s\n", msg); \
  } \
} while (0)

// Simple window procedure for test forms
static result_t test_form_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  (void)win; (void)msg; (void)wparam; (void)lparam;
  return false;
}

/**
 * Test that reportview always has WINDOW_FLEXSPACE regardless of explicit flags.
 * Class defaults should be merged with instance flags, not replaced.
 */
static void test_reportview_class_flags_not_overrideable(void) {
  // Create a reportview with explicit flags that DON'T include WINDOW_FLEXSPACE
  // The class should add WINDOW_FLEXSPACE anyway
  form_ctrl_def_t reportview_form = {
    .class_name = "reportview",
    .id = 1,
    .size = {0, 0},
    .flags = WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_VSCROLL,  // NO FLEXSPACE
    .text = "",
    .name = "test_rv",
  };
  
  form_def_t def = {
    .name = "Test Class Defaults",
    .width = 300,
    .height = 200,
    .flags = 0,
    .auto_layout = true,
    .layout_kind = "stack",
    .layout_orientation = WINDOW_STACK_VERTICAL,
    .children = &reportview_form,
    .child_count = 1,
  };
  
  window_t *win = create_window_from_form(&def, 0, 0, NULL, test_form_proc, 0, NULL);
  ASSERT(win != NULL, "Window created");
  
  // Find the reportview child
  window_t *rv = win->children;
  ASSERT(rv != NULL, "Reportview child created");
  ASSERT(rv->id == 1, "Child has correct ID");
  
  // The reportview should have WINDOW_FLEXSPACE from class defaults
  // even though explicit instance flags didn't include it
  bool has_flexspace = (rv->flags & WINDOW_FLEXSPACE) != 0;
  ASSERT(has_flexspace, "Reportview has WINDOW_FLEXSPACE from class defaults");
  
  // Should also have the explicitly specified flags
  bool has_notitle = (rv->flags & WINDOW_NOTITLE) != 0;
  bool has_noresize = (rv->flags & WINDOW_NORESIZE) != 0;
  bool has_vscroll = (rv->flags & WINDOW_VSCROLL) != 0;
  ASSERT(has_notitle, "Reportview kept WINDOW_NOTITLE");
  ASSERT(has_noresize, "Reportview kept WINDOW_NORESIZE");
  ASSERT(has_vscroll, "Reportview kept WINDOW_VSCROLL");
  
  // Cleanup
  destroy_window(win);
}

/**
 * Test that space element gets WINDOW_FLEXSPACE from class defaults
 */
static void test_space_class_has_flexspace(void) {
  form_ctrl_def_t space_form = {
    .class_name = "space",
    .id = 2,
    .size = {0, 0},
    .flags = 0,  // No flags specified
    .text = "",
    .name = "spacer",
  };
  
  form_def_t def = {
    .name = "Test Space",
    .width = 300,
    .height = 100,
    .flags = 0,
    .auto_layout = true,
    .layout_kind = "stack",
    .layout_orientation = WINDOW_STACK_VERTICAL,
    .children = &space_form,
    .child_count = 1,
  };
  
  window_t *win = create_window_from_form(&def, 0, 0, NULL, test_form_proc, 0, NULL);
  ASSERT(win != NULL, "Window created");
  
  window_t *space = win->children;
  ASSERT(space != NULL, "Space child created");
  
  bool has_flexspace = (space->flags & WINDOW_FLEXSPACE) != 0;
  ASSERT(has_flexspace, "Space has WINDOW_FLEXSPACE from class defaults");
  
  destroy_window(win);
}

/**
 * Test that multiedit always has WINDOW_FLEXSPACE
 */
static void test_multiedit_class_has_flexspace(void) {
  form_ctrl_def_t multiedit_form = {
    .class_name = "multiedit",
    .id = 3,
    .size = {0, 0},
    .flags = WINDOW_NOTITLE,  // Only notitle, no flexspace
    .text = "",
    .name = "editor",
  };
  
  form_def_t def = {
    .name = "Test Multiedit",
    .width = 300,
    .height = 200,
    .flags = 0,
    .auto_layout = true,
    .layout_kind = "stack",
    .layout_orientation = WINDOW_STACK_VERTICAL,
    .children = &multiedit_form,
    .child_count = 1,
  };
  
  window_t *win = create_window_from_form(&def, 0, 0, NULL, test_form_proc, 0, NULL);
  ASSERT(win != NULL, "Window created");
  
  window_t *me = win->children;
  ASSERT(me != NULL, "Multiedit child created");
  
  bool has_flexspace = (me->flags & WINDOW_FLEXSPACE) != 0;
  fprintf(stderr, "DEBUG: multiedit flags = 0x%x, has FLEXSPACE = %d\n", me->flags, has_flexspace);
  ASSERT(has_flexspace, "Multiedit has WINDOW_FLEXSPACE from class defaults");
  
  bool has_notitle = (me->flags & WINDOW_NOTITLE) != 0;
  fprintf(stderr, "DEBUG: multiedit has NOTITLE = %d\n", has_notitle);
  ASSERT(has_notitle, "Multiedit kept WINDOW_NOTITLE");
  
  destroy_window(win);
}

/**
 * Test that WINDOW_FLEXSPACE propagates from child to parent
 */
static void test_flexspace_propagates_up(void) {
  // Create a form with a reportview inside a column inside a grid
  form_ctrl_def_t reportview_form = {
    .class_name = "reportview",
    .id = 10,
    .size = {0, 0},
    .flags = WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_VSCROLL,  // No flexspace in flags
    .text = "",
    .name = "items",
  };
  
  form_ctrl_def_t column_form = {
    .class_name = "stack",
    .id = 11,
    .size = {0, 0},
    .flags = 0,  // No flags
    .text = "",
    .name = "col",
    .children = &reportview_form,
    .child_count = 1,
    .layout_kind = "stack",
    .layout_orientation = WINDOW_STACK_VERTICAL,
  };
  
  form_ctrl_def_t grid_form = {
    .class_name = "grid",
    .id = 12,
    .size = {0, 0},
    .flags = 0,  // No flags
    .text = "",
    .name = "main",
    .children = &column_form,
    .child_count = 1,
    .layout_kind = "grid",
  };
  
  form_def_t def = {
    .name = "Test Propagation",
    .width = 400,
    .height = 300,
    .flags = 0,
    .auto_layout = true,
    .layout_kind = "stack",
    .layout_orientation = WINDOW_STACK_VERTICAL,
    .children = &grid_form,
    .child_count = 1,
  };
  
  window_t *win = create_window_from_form(&def, 0, 0, NULL, test_form_proc, 0, NULL);
  ASSERT(win != NULL, "Window created");
  
  // Navigate the tree: form -> grid -> column -> reportview
  window_t *grid = win->children;
  ASSERT(grid != NULL, "Grid child exists");
  
  window_t *column = grid->children;
  ASSERT(column != NULL, "Column child exists");
  
  window_t *rv = column->children;
  ASSERT(rv != NULL, "Reportview child exists");
  ASSERT(rv->id == 10, "Reportview has correct ID");
  
  // Reportview should have FLEXSPACE from class
  bool rv_has_flex = (rv->flags & WINDOW_FLEXSPACE) != 0;
  ASSERT(rv_has_flex, "Reportview has WINDOW_FLEXSPACE from class");
  
  // FLEXSPACE should propagate up: reportview -> column
  bool column_has_flex = (column->flags & WINDOW_FLEXSPACE) != 0;
  ASSERT(column_has_flex, "Column has WINDOW_FLEXSPACE (propagated from reportview)");
  
  // And from column -> grid
  bool grid_has_flex = (grid->flags & WINDOW_FLEXSPACE) != 0;
  ASSERT(grid_has_flex, "Grid has WINDOW_FLEXSPACE (propagated from column)");
  
  // And from grid -> form (parent)
  bool form_has_flex = (win->flags & WINDOW_FLEXSPACE) != 0;
  ASSERT(form_has_flex, "Form has WINDOW_FLEXSPACE (propagated from grid)");
  
  destroy_window(win);
}

int main(void) {
  FILE *dbg = fopen("/tmp/test_debug.log", "w");
  fprintf(dbg, "Starting test...\n");
  fflush(dbg);
  
  test_env_init();
  fprintf(dbg, "Test env initialized.\n");
  fflush(dbg);
  
  fprintf(dbg, "Running test_reportview_class_flags_not_overrideable...\n");
  fflush(dbg);
  test_reportview_class_flags_not_overrideable();
  fprintf(dbg, "Done.\n");
  fflush(dbg);
  
  fprintf(dbg, "Running test_space_class_has_flexspace...\n");
  fflush(dbg);
  test_space_class_has_flexspace();
  fprintf(dbg, "Done.\n");
  fflush(dbg);
  
  fprintf(dbg, "Running test_multiedit_class_has_flexspace...\n");
  fflush(dbg);
  test_multiedit_class_has_flexspace();
  fprintf(dbg, "Done.\n");
  fflush(dbg);
  
  fprintf(dbg, "Running test_flexspace_propagates_up...\n");
  fflush(dbg);
  test_flexspace_propagates_up();
  fprintf(dbg, "Done.\n");
  fflush(dbg);
  
  fprintf(dbg, "Shutting down test environment...\n");
  fflush(dbg);
  test_env_shutdown();
  
  fprintf(dbg, "\nTest Results:\n");
  fprintf(dbg, "  Total:  %d\n", kTestsTotal);
  fprintf(dbg, "  Passed: %d\n", kTestsPassed);
  
  if (kTestsPassed == kTestsTotal) {
    fprintf(dbg, "\nAll tests passed!\n");
    fclose(dbg);
    return 0;
  } else {
    fprintf(dbg, "\n%d tests failed!\n", kTestsTotal - kTestsPassed);
    fclose(dbg);
    return 1;
  }
}
