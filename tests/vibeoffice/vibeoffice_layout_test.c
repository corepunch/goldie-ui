#include "test_framework.h"
#include "test_env.h"
#include "build/generated/examples/vibeoffice/vibeoffice.h"

static result_t inspector_test_proc(window_t *win, uint32_t msg,
                                    uint32_t wparam, void *lparam) {
  (void)win; (void)wparam; (void)lparam;
  return msg == evCreate;
}

static void assert_above(window_t *upper, window_t *lower, const char *message) {
  ASSERT_NOT_NULL(upper); ASSERT_NOT_NULL(lower);
  ASSERT(upper->frame.y + upper->frame.h <= lower->frame.y, message);
}

static void test_inspector_form_has_non_overlapping_layout(void) {
  TEST("VibeOffice: inspector form lays out controls without overlap");
  test_env_init();
  window_t *inspector = create_window_from_form(&vibeoffice_inspector_form, 0, 0, NULL,
                                                 inspector_test_proc, 0, NULL);
  ASSERT_NOT_NULL(inspector);
  window_layout_sync(inspector);
  window_t *desk = get_window_item(inspector, ID_INSPECTOR_DESK);
  window_t *status = get_window_item(inspector, ID_INSPECTOR_STATUS);
  window_t *message = get_window_item(inspector, ID_INSPECTOR_MESSAGE_LABEL);
  window_t *row = get_window_item(inspector, ID_INSPECTOR_INPUT_ROW);
  window_t *input = get_window_item(inspector, ID_INSPECTOR_INPUT);
  window_t *submit = get_window_item(inspector, ID_INSPECTOR_SUBMIT);
  window_t *response = get_window_item(inspector, ID_INSPECTOR_RESPONSE_LABEL);
  window_t *output = get_window_item(inspector, ID_INSPECTOR_OUTPUT);
  assert_above(desk, status, "desk and status labels overlap");
  assert_above(status, message, "status and message labels overlap");
  assert_above(message, row, "message label and input row overlap");
  assert_above(row, response, "input row and response label overlap");
  assert_above(response, output, "response label and output overlap");
  ASSERT(input->frame.x + input->frame.w <= submit->frame.x,
         "input and Send button overlap");
  irect16_t client = get_client_rect(inspector);
  ASSERT(output->frame.y + output->frame.h <= client.y + client.h,
         "response output extends beyond inspector client area");
  destroy_window(inspector); test_env_shutdown(); PASS();
}

int main(void) {
  TEST_START("VibeOffice Layout");
  test_inspector_form_has_non_overlapping_layout();
  TEST_END();
}
