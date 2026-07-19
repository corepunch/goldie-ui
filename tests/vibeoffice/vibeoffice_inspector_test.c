#define BUILD_AS_GEM 1
#include "examples/vibeoffice/main.c"
#undef BUILD_AS_GEM
#undef main

#include "test_framework.h"
#include "test_env.h"

static void test_real_inspector_binds_and_refreshes_generated_controls(void) {
  TEST("VibeOffice: real inspector binds and refreshes generated controls");
  test_env_init();
  database_t *models = vibe_models_create(); ASSERT_NOT_NULL(models);
  ui_set_database(models);
  memset(&g_vibe_inspector, 0, sizeof(g_vibe_inspector));
  window_t *inspector = create_window_from_form(&vibeoffice_inspector_form, 0, 0, NULL,
                                                 win_inspector, 0, NULL);
  ASSERT_NOT_NULL(inspector);
  ASSERT_NOT_NULL(g_vibe_inspector.desk_label);
  ASSERT_NOT_NULL(g_vibe_inspector.status_label);
  ASSERT_NOT_NULL(g_vibe_inspector.model);
  ASSERT_NOT_NULL(g_vibe_inspector.input);
  ASSERT_NOT_NULL(g_vibe_inspector.submit);
  ASSERT_NOT_NULL(g_vibe_inspector.output);
  g_vibe_inspector.selected = 0;
  g_vibe_inspector.submit = NULL; // refresh must safely recover generated bindings
  inspector_refresh(true);
  ASSERT_NOT_NULL(g_vibe_inspector.submit);
  ASSERT_STR_EQUAL(g_vibe_inspector.desk_label->title, "Desk: Manager");
  vibe_task_t task; char expected_status[64];
  vibe_task_read(1, &task);
  snprintf(expected_status, sizeof(expected_status), "Status: %s", vibe_task_status_name(task.status));
  ASSERT_STR_EQUAL(g_vibe_inspector.status_label->title, expected_status);
  ASSERT_EQUAL(g_vibe_inspector.model->cursor_pos, 58);
  ASSERT_STR_EQUAL(g_vibe_inspector.model->title, "MiMo V2.5 Free");
  const vibe_model_info_t *deepseek = vibe_model_by_opencode_id("opencode/deepseek-v4-flash");
  ASSERT_NOT_NULL(deepseek); ASSERT_STR_EQUAL(deepseek->name, "DeepSeek V4 Flash");
  send_message(g_vibe_inspector.model, cbSetCurrentSelection, 3, NULL);
  send_message(inspector, evCommand, MAKEDWORD(ID_INSPECTOR_MODEL, cbSelectionChange),
               g_vibe_inspector.model);
  ASSERT_EQUAL(g_icons[0].model_id, 4);
  show_window(inspector, true);
  ASSERT_TRUE(send_message(inspector, evClose, 0, NULL));
  ASSERT_TRUE(is_window(inspector));
  ASSERT_TRUE(!window_has_state(inspector, WINDOW_STATE_VISIBLE));
  inspector_select(&g_icons[1]);
  ASSERT_TRUE(is_window(inspector));
  ASSERT_TRUE(window_has_state(inspector, WINDOW_STATE_VISIBLE));
  ASSERT_STR_EQUAL(g_vibe_inspector.desk_label->title, "Desk: Developer");
  destroy_window(inspector); ui_set_database(NULL); destroy_database(models);
  test_env_shutdown(); PASS();
}

int main(void) {
  TEST_START("VibeOffice Inspector");
  test_real_inspector_binds_and_refreshes_generated_controls();
  TEST_END();
}
