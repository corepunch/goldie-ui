#define BUILD_AS_GEM 1
#include "examples/vibeoffice/main.c"
#undef BUILD_AS_GEM
#undef main

#include "test_framework.h"
#include "test_env.h"

static void test_real_inspector_binds_and_refreshes_generated_controls(void) {
  TEST("VibeOffice: real inspector binds and refreshes generated controls");
  test_env_init();
  memset(&g_vibe_inspector, 0, sizeof(g_vibe_inspector));
  window_t *inspector = create_window_from_form(&vibeoffice_inspector_form, 0, 0, NULL,
                                                 win_inspector, 0, NULL);
  ASSERT_NOT_NULL(inspector);
  ASSERT_NOT_NULL(g_vibe_inspector.desk_label);
  ASSERT_NOT_NULL(g_vibe_inspector.status_label);
  ASSERT_NOT_NULL(g_vibe_inspector.input);
  ASSERT_NOT_NULL(g_vibe_inspector.submit);
  ASSERT_NOT_NULL(g_vibe_inspector.output);
  g_vibe_inspector.selected = 0;
  g_vibe_inspector.submit = NULL; // refresh must safely recover generated bindings
  inspector_refresh(true);
  ASSERT_NOT_NULL(g_vibe_inspector.submit);
  ASSERT_STR_EQUAL(g_vibe_inspector.desk_label->title, "Desk: Manager");
  ASSERT_STR_EQUAL(g_vibe_inspector.status_label->title, "Status: available");
  destroy_window(inspector); test_env_shutdown(); PASS();
}

int main(void) {
  TEST_START("VibeOffice Inspector");
  test_real_inspector_binds_and_refreshes_generated_controls();
  TEST_END();
}
