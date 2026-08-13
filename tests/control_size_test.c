#include "test_framework.h"
#include "test_env.h"
#include <orion/ui.h>

static result_t host_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  (void)win; (void)wparam; (void)lparam;
  return msg == evCreate || msg == evDestroy;
}

static void assert_control_height(window_t *parent, winproc_t proc, flags_t size, int expected) {
  irect16_t frame = {0, 0, 100, 47};
  window_t *control = create_window("Control", size, &frame, parent, proc, 0, NULL);
  ASSERT_NOT_NULL(control);
  ASSERT_EQUAL(control->frame.h, expected);
  layout_measure_t measure = { .avail_w = 200, .avail_h = 200 };
  send_message(control, evMeasure, 0, &measure);
  ASSERT_EQUAL(measure.desired_h, expected);
}

void test_intrinsic_control_size_variants(void) {
  TEST("button-like controls own mini/small/regular/large heights");
  test_env_init();
  window_t *host = create_window("host", WINDOW_NOTITLE, MAKERECT(0, 0, 300, 200),
                                 NULL, host_proc, 0, NULL);
  ASSERT_NOT_NULL(host);
  winproc_t controls[] = {win_button, win_textedit, win_combobox};
  for (int i = 0; i < ARRAY_LEN(controls); i++) {
    assert_control_height(host, controls[i], CONTROL_SIZE_MINI, CONTROL_HEIGHT_MINI);
    assert_control_height(host, controls[i], CONTROL_SIZE_SMALL, CONTROL_HEIGHT_SMALL);
    assert_control_height(host, controls[i], CONTROL_SIZE_REGULAR, CONTROL_HEIGHT_REGULAR);
    assert_control_height(host, controls[i], CONTROL_SIZE_LARGE, CONTROL_HEIGHT_LARGE);
  }
  destroy_window(host);
  test_env_shutdown();
  PASS();
}

int main(void) {
  TEST_START("intrinsic control sizes");
  test_intrinsic_control_size_variants();
  TEST_END();
}
