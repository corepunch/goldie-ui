#include <orion/ui.h>
#include "test_env.h"
#include "test_framework.h"

static result_t nop_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  (void)win; (void)msg; (void)wparam; (void)lparam;
  return false;
}

static window_t *find_splitter(window_t *win) {
  // NOTE: Must compare by class name, not by proc pointer.
  // On Windows (MinGW DLLs), a function pointer obtained from outside the DLL
  // points to the import stub, while the same pointer inside the DLL points to
  // the real function.  Direct child->proc == win_splitter comparison would
  // fail.  Using find_window_class_desc_by_proc() avoids this because both the
  // stored registry pointer and the child->proc are the real function address.
  for (window_t *child = win ? win->children : NULL; child; child = child->next) {
    const fe_component_desc_t *desc = find_window_class_desc_by_proc(child->proc);
    if (desc && strcmp(desc->class_name, "Splitter") == 0)
      return child;
  }
  return NULL;
}

static void dispatch_mouse(int x, int y, uint32_t message) {
  ui_event_t event = { .message = message,
                       .x = (uint16_t)(x * UI_WINDOW_SCALE),
                       .y = (uint16_t)(y * UI_WINDOW_SCALE) };
  dispatch_message(&event);
}

static void test_form_splitview_uses_arranged_frame(void) {
  TEST("SplitView: form layout assigns a non-zero frame and arranges both panes");
  test_env_init();
  static const form_ctrl_def_t panes[] = {
    { .class_name = "Label", .id = 2, .text = "left" },
    { .class_name = "Label", .id = 3, .text = "right" },
  };
  static const form_ctrl_def_t children[] = {
    { .class_name = "SplitView", .id = 1, .flags = WINDOW_FLEXSPACE,
      .children = panes, .child_count = 2, .lparam = (void *)SPLIT_VERT },
  };
  static const form_def_t def = {
    .name = "split", .width = 300, .height = 120, .flags = WINDOW_AUTO_LAYOUT,
    .children = children, .child_count = 1,
  };

  window_t *root = create_window_from_form(&def, 0, 0, NULL, nop_proc, 0, NULL);
  window_t *split = get_window_item(root, 1);
  window_t *left = splitview_get_left(split), *right = splitview_get_right(split);
  ASSERT_NOT_NULL(root); ASSERT_NOT_NULL(split); ASSERT_NOT_NULL(left); ASSERT_NOT_NULL(right);
  ASSERT_TRUE(split->frame.w > 0); ASSERT_TRUE(split->frame.h > 0);
  ASSERT_TRUE(left->frame.w > 0); ASSERT_TRUE(right->frame.w > 0);
  ASSERT_EQUAL(left->frame.y, right->frame.y);
  ASSERT_TRUE(right->frame.x > left->frame.x);

  destroy_window(root);
  test_env_shutdown();
  PASS();
}

static void test_form_splitview_honors_horizontal_divider(void) {
  TEST("SplitView: form lparam selects top/bottom panes");
  test_env_init();
  static const form_ctrl_def_t panes[] = {
    { .class_name = "Label", .id = 12, .text = "top" },
    { .class_name = "Label", .id = 13, .text = "bottom" },
  };
  static const form_ctrl_def_t children[] = {
    { .class_name = "SplitView", .id = 11, .flags = WINDOW_FLEXSPACE,
      .children = panes, .child_count = 2, .lparam = (void *)SPLIT_HORZ },
  };
  static const form_def_t def = {
    .name = "split", .width = 300, .height = 120, .flags = WINDOW_AUTO_LAYOUT,
    .children = children, .child_count = 1,
  };

  window_t *root = create_window_from_form(&def, 0, 0, NULL, nop_proc, 0, NULL);
  window_t *split = get_window_item(root, 11);
  window_t *top = splitview_get_left(split), *bottom = splitview_get_right(split);
  ASSERT_NOT_NULL(root); ASSERT_NOT_NULL(split); ASSERT_NOT_NULL(top); ASSERT_NOT_NULL(bottom);
  ASSERT_TRUE(top->frame.h > 0); ASSERT_TRUE(bottom->frame.h > 0);
  ASSERT_EQUAL(top->frame.x, bottom->frame.x);
  ASSERT_TRUE(bottom->frame.y > top->frame.y);

  destroy_window(root);
  test_env_shutdown();
  PASS();
}

static void test_nested_splitter_hit_hover_and_drag(void) {
  TEST("SplitView: nested divider owns hover and drags in parent-local coordinates");
  test_env_init();
  static const form_ctrl_def_t inner_panes[] = {
    { .class_name = "Label", .id = 23, .text = "middle" },
    { .class_name = "Label", .id = 24, .text = "right" },
  };
  static const form_ctrl_def_t outer_panes[] = {
    { .class_name = "Label", .id = 21, .text = "left" },
    { .class_name = "SplitView", .id = 22, .flags = WINDOW_FLEXSPACE,
      .children = inner_panes, .child_count = 2, .lparam = (void *)SPLIT_VERT },
  };
  static const form_ctrl_def_t children[] = {
    { .class_name = "SplitView", .id = 20, .flags = WINDOW_FLEXSPACE,
      .children = outer_panes, .child_count = 2, .lparam = (void *)SPLIT_VERT },
  };
  static const form_def_t def = {
    .name = "nested split", .width = 600, .height = 160,
    .flags = WINDOW_AUTO_LAYOUT | WINDOW_NOTITLE,
    .children = children, .child_count = 1,
  };

  window_t *root = create_window_from_form(&def, 40, 30, NULL, nop_proc, 0, NULL);
  window_t *inner = get_window_item(root, 22), *splitter = find_splitter(inner);
  ASSERT_NOT_NULL(root); ASSERT_NOT_NULL(inner); ASSERT_NOT_NULL(splitter);
  int sx = window_screen_x(splitter) + splitter->frame.w / 2;
  int sy = window_screen_y(splitter) + splitter->frame.h / 2;
  ASSERT(find_window(sx, sy) == splitter, "nested splitter was not the hover target");

  dispatch_mouse(sx, sy, kEventMouseMoved);
  ASSERT(g_ui_runtime.tracked == splitter, "nested splitter did not retain mouse tracking");
  int old_left_w = splitview_get_left(inner)->frame.w;
  dispatch_mouse(sx, sy, kEventLeftButtonDown);
  ASSERT(g_ui_runtime.captured == inner, "nested splitview did not capture its drag");
  dispatch_mouse(sx + 30, sy, kEventLeftButtonDragged);
  dispatch_mouse(sx + 30, sy, kEventLeftButtonUp);
  ASSERT(splitview_get_left(inner)->frame.w > old_left_w, "nested divider did not move right");
  ASSERT_NULL(g_ui_runtime.captured);

  destroy_window(root);
  test_env_shutdown();
  PASS();
}

int main(void) {
  TEST_START("splitview layout");
  test_form_splitview_uses_arranged_frame();
  test_form_splitview_honors_horizontal_divider();
  test_nested_splitter_hit_hover_and_drag();
  TEST_END();
}
