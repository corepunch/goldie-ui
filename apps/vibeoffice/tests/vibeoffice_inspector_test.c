#define BUILD_AS_GEM 1
#include "apps/vibeoffice/main.c"
#undef BUILD_AS_GEM
#undef main

#include "test_framework.h"
#include "test_env.h"

static result_t test_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  (void)win; (void)wparam; (void)lparam;
  return msg == evCreate || msg == evDestroy;
}

static int window_order(window_t *needle) {
  int order = 0;
  for (window_t *win = g_ui_runtime.windows; win; win = win->next, order++)
    if (win == needle) return order;
  return -1;
}

static void setup_vibe_test(database_t **out_models) {
  test_env_init();
  database_t *models = vibe_models_create();
  ASSERT_NOT_NULL(models);
  g_models_db = models; g_hinstance = 0;
  ui_set_database(models);
  for (int i = 0; i < (int)ARRAY_LEN(g_icons); i++) {
    memset(&g_icons[i].inspector, 0, sizeof(g_icons[i].inspector));
    g_icons[i].win = NULL;
    g_icons[i].model_id = i + 1;
  }
  g_controller = create_window("", WINDOW_HIDDEN | WINDOW_NOTITLE | WINDOW_NORESIZE |
                               WINDOW_NOACTIVATE | WINDOW_NOTRAYBUTTON,
                               MAKERECT(0, 0, 1, 1), NULL,
                               win_vibe_controller, 0, NULL);
  ASSERT_NOT_NULL(g_controller);
  *out_models = models;
}

static void teardown_vibe_test(database_t *models, window_t *desktop) {
  for (int i = 0; i < (int)ARRAY_LEN(g_icons); i++) {
    inspector_t *inspector = &g_icons[i].inspector;
    if (inspector->win && is_window(inspector->win)) destroy_window(inspector->win);
    g_icons[i].win = NULL;
  }
  if (desktop && is_window(desktop)) destroy_window(desktop);
  if (g_controller && is_window(g_controller)) destroy_window(g_controller);
  g_controller = NULL; g_models_db = NULL;
  ui_set_database(NULL); destroy_database(models);
  test_env_shutdown();
}

static void test_each_agent_owns_an_inspector(void) {
  TEST("VibeOffice: each agent owns an independent inspector view");
  database_t *models = NULL; setup_vibe_test(&models);
  ASSERT_NOT_NULL(models);

  window_t *manager_win = inspector_open(&g_icons[0]);
  ASSERT_NOT_NULL(manager_win);
  inspector_t *manager = &g_icons[0].inspector;
  ASSERT_EQUAL(manager->icon, &g_icons[0]);
  ASSERT_NOT_NULL(manager->desk_label); ASSERT_NOT_NULL(manager->status_label);
  ASSERT_NOT_NULL(manager->model); ASSERT_NOT_NULL(manager->input);
  ASSERT_NOT_NULL(manager->submit); ASSERT_NOT_NULL(manager->output);
  ASSERT_STR_EQUAL(manager->desk_label->title, "Desk: Manager");
  ASSERT_EQUAL(manager->model->cursor_pos, 58);
  ASSERT_STR_EQUAL(manager->model->title, "MiMo V2.5 Free");

  send_message(manager->model, cbSetCurrentSelection, 3, NULL);
  send_message(manager_win, evCommand, MAKEDWORD(ID_INSPECTOR_MODEL, cbSelectionChange), manager->model);
  ASSERT_EQUAL(g_icons[0].model_id, 4);

  window_t *developer_win = inspector_open(&g_icons[1]);
  ASSERT_NOT_NULL(developer_win);
  ASSERT_NOT_EQUAL(developer_win, manager_win);
  ASSERT_EQUAL(g_icons[1].inspector.icon, &g_icons[1]);
  ASSERT_STR_EQUAL(g_icons[1].inspector.desk_label->title, "Desk: Developer");
  ASSERT_EQUAL(g_icons[0].inspector.win, manager_win);

  teardown_vibe_test(models, NULL);
  PASS();
}

static void test_icon_click_and_double_click_window_behavior(void) {
  TEST("VibeOffice: click selects; double-click opens or raises agent inspector");
  database_t *models = NULL; setup_vibe_test(&models);
  ASSERT_NOT_NULL(models);
  window_t *desktop = test_env_create_window("Desktop", 0, 0, 800, 600,
                                              test_window_proc, NULL);
  ASSERT_NOT_NULL(desktop);
  for (int i = 0; i < 2; i++) {
    icon_params_t params = { .item_data = &g_icons[i], .notify_window = g_controller };
    g_icons[i].win = create_window(g_icons[i].title,
                                   WINDOW_NOTITLE | WINDOW_NORESIZE,
                                   MAKERECT(20 + i * 140, 20, 128, 144), desktop,
                                   win_icon, 0, &params);
    ASSERT_NOT_NULL(g_icons[i].win);
  }

  send_message(g_icons[0].win, evLeftButtonDown, MAKEDWORD(10, 10), NULL);
  send_message(g_icons[0].win, evLeftButtonUp, MAKEDWORD(10, 10), NULL);
  ASSERT_TRUE(send_message(g_icons[0].win, icGetSelected, 0, NULL));
  ASSERT_NULL(g_icons[0].inspector.win);

  send_message(g_icons[0].win, evLeftButtonDoubleClick, MAKEDWORD(10, 10), NULL);
  window_t *manager = g_icons[0].inspector.win;
  ASSERT_NOT_NULL(manager);
  ASSERT_TRUE(window_has_state(manager, WINDOW_STATE_VISIBLE));

  window_t *other = test_env_create_window("Other", 10, 10, 100, 100,
                                            test_window_proc, NULL);
  ASSERT_NOT_NULL(other);
  move_to_top(other); set_focus(other);
  ASSERT_EQUAL(g_ui_runtime.focused, other);
  send_message(g_icons[0].win, evLeftButtonDoubleClick, MAKEDWORD(10, 10), NULL);
  ASSERT_EQUAL(g_icons[0].inspector.win, manager);
  ASSERT_EQUAL(g_ui_runtime.focused, manager);
  ASSERT(window_order(manager) > window_order(other), "existing inspector was not raised");

  send_message(manager, evClose, 0, NULL);
  ASSERT(!window_has_state(manager, WINDOW_STATE_VISIBLE), "window still visible after evClose"); // DEBUG: A
  send_message(g_icons[0].win, evLeftButtonDoubleClick, MAKEDWORD(10, 10), NULL);
  ASSERT_EQUAL(g_icons[0].inspector.win, manager);
  ASSERT_TRUE(window_has_state(manager, WINDOW_STATE_VISIBLE));

  send_message(g_icons[1].win, evLeftButtonDown, MAKEDWORD(10, 10), NULL);
  send_message(g_icons[1].win, evLeftButtonUp, MAKEDWORD(10, 10), NULL);
  ASSERT_TRUE(send_message(g_icons[1].win, icGetSelected, 0, NULL));
  ASSERT(!send_message(g_icons[0].win, icGetSelected, 0, NULL), "icon 0 still selected after clicking icon 1"); // DEBUG: B
  ASSERT_NULL(g_icons[1].inspector.win);
  send_message(g_icons[1].win, evLeftButtonDoubleClick, MAKEDWORD(10, 10), NULL);
  ASSERT_NOT_NULL(g_icons[1].inspector.win);
  ASSERT_NOT_EQUAL(g_icons[1].inspector.win, manager);

  destroy_window(other);
  teardown_vibe_test(models, desktop);
  PASS();
}

int main(void) {
  TEST_START("VibeOffice Inspector");
  test_each_agent_owns_an_inspector();
  test_icon_click_and_double_click_window_behavior();
  TEST_END();
}
