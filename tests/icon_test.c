#include "test_framework.h"
#include "test_env.h"
#include "../ui.h"

static int selection_count, click_count, open_count, artifact_drop_count, dropped_artifact_id;
static bool artifact_drop_accepted;
static window_t *last_source;
static window_t *last_drop_source, *last_drop_target;

static result_t icon_parent_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  (void)win;
  if (msg == evCreate || msg == evDestroy) return true;
  if (msg != evCommand) return false;
  if (HIWORD(wparam) == icnSelectionChange) selection_count++;
  if (HIWORD(wparam) == icnClicked) click_count++;
  if (HIWORD(wparam) == icnOpen) open_count++;
  if (HIWORD(wparam) == icnArtifactDrop) {
    icon_artifact_drop_t *drop = (icon_artifact_drop_t *)lparam;
    artifact_drop_count++; dropped_artifact_id = drop->artifact_id;
    last_drop_source = drop->source; last_drop_target = drop->target;
    return artifact_drop_accepted;
  }
  last_source = (window_t *)lparam;
  return true;
}

static void reset_counts(void) {
  selection_count = click_count = open_count = artifact_drop_count = dropped_artifact_id = 0;
  artifact_drop_accepted = true;
  last_source = last_drop_source = last_drop_target = NULL;
}

static window_t *artifact_drag_ghost(window_t *parent) {
  for (window_t *win = g_ui_runtime.windows; win; win = win->next)
    if (win != parent && !win->parent && (win->flags & WINDOW_ALWAYSONTOP)) return win;
  return NULL;
}

static void test_icon_selection_is_exclusive(void) {
  TEST("Icon: selection is exclusive among sibling icons");
  test_env_init(); reset_counts();
  window_t *parent = test_env_create_window("parent", 0, 0, 320, 200, icon_parent_proc, NULL);
  window_t *a = create_window("A", 0, MAKERECT(0, 0, 128, 128), parent, win_icon, 0, NULL);
  window_t *b = create_window("B", 0, MAKERECT(130, 0, 128, 128), parent, win_icon, 0, NULL);
  a->id = 10; b->id = 11;
  send_message(a, evLeftButtonDown, MAKEDWORD(10, 10), NULL);
  ASSERT_TRUE(send_message(a, icGetSelected, 0, NULL));
  send_message(b, evLeftButtonDown, MAKEDWORD(10, 10), NULL);
  ASSERT_FALSE(send_message(a, icGetSelected, 0, NULL));
  ASSERT_TRUE(send_message(b, icGetSelected, 0, NULL));
  ASSERT_EQUAL(selection_count, 2);
  destroy_window(parent); test_env_shutdown(); PASS();
}

static void test_icon_notifications_and_item_data(void) {
  TEST("Icon: click/open notifications and item data");
  int model = 42;
  icon_params_t params = { .item_data = &model };
  test_env_init(); reset_counts();
  window_t *parent = test_env_create_window("parent", 0, 0, 200, 160, icon_parent_proc, NULL);
  window_t *icon = create_window("Desk", 0, MAKERECT(0, 0, 128, 128), parent, win_icon, 0, &params);
  icon->id = 20;
  ASSERT_EQUAL((void *)send_message(icon, icGetItemData, 0, NULL), &model);
  send_message(icon, evLeftButtonUp, MAKEDWORD(10, 10), NULL);
  send_message(icon, evLeftButtonDoubleClick, MAKEDWORD(10, 10), NULL);
  ASSERT_EQUAL(click_count, 1); ASSERT_EQUAL(open_count, 1); ASSERT_EQUAL(last_source, icon);
  destroy_window(parent); test_env_shutdown(); PASS();
}

static void test_icon_badges_copy_input(void) {
  TEST("Icon: badges copy caller data and support clearing");
  test_env_init();
  window_t *parent = test_env_create_window("parent", 0, 0, 200, 160, icon_parent_proc, NULL);
  window_t *icon = create_window("Manager", 0, MAKERECT(0, 0, 128, 128), parent, win_icon, 0, NULL);
  char text[] = "7";
  icon_badge_t badge = { text, 0xff0000ff, 0xffffffff, ICON_BADGE_TOP_RIGHT };
  ASSERT_TRUE(send_message(icon, icSetBadge, 0, &badge));
  text[0] = '9';
  ASSERT_TRUE(send_message(icon, icSetBadge, 1, &badge));
  ASSERT_TRUE(send_message(icon, icSetBadge, 0, NULL));
  ASSERT_TRUE(send_message(icon, icClearBadges, 0, NULL));
  ASSERT_FALSE(send_message(icon, icSetBadge, ICON_MAX_BADGES, &badge));
  destroy_window(parent); test_env_shutdown(); PASS();
}

static void test_icon_status_image_can_be_set_and_cleared(void) {
  TEST("Icon: status image can be set beside the label and cleared");
  icon_image_t status = { 123, 48, 48 };
  test_env_init();
  window_t *parent = test_env_create_window("parent", 0, 0, 200, 160, icon_parent_proc, NULL);
  window_t *icon = create_window("Available", 0, MAKERECT(0, 0, 128, 128), parent, win_icon, 0, NULL);
  ASSERT_TRUE(send_message(icon, icSetStatusImage, 0, &status));
  ASSERT_TRUE(send_message(icon, icSetStatusImage, 0, NULL));
  destroy_window(parent); test_env_shutdown(); PASS();
}

static void test_draggable_icon_moves_without_clicking(void) {
  TEST("Icon: draggable icons move within their parent without clicking");
  icon_params_t params = { .draggable = true };
  test_env_init(); reset_counts();
  window_t *parent = test_env_create_window("parent", 0, 0, 300, 220, icon_parent_proc, NULL);
  window_t *icon = create_window("Desk", 0, MAKERECT(20, 30, 100, 100), parent, win_icon, 0, &params);
  send_message(icon, evLeftButtonDown, MAKEDWORD(10, 12), NULL);
  send_message(icon, evMouseMove, MAKEDWORD(50, 62), NULL);
  ASSERT_EQUAL(icon->frame.x, 60); ASSERT_EQUAL(icon->frame.y, 80);
  send_message(icon, evMouseMove, MAKEDWORD(300, 300), NULL);
  ASSERT_EQUAL(icon->frame.x, 200); ASSERT_EQUAL(icon->frame.y, 120);
  send_message(icon, evLeftButtonUp, MAKEDWORD(10, 12), NULL);
  ASSERT_EQUAL(click_count, 0); ASSERT_EQUAL(selection_count, 1);
  destroy_window(parent); test_env_shutdown(); PASS();
}

static void test_draggable_icon_still_clicks_without_moving(void) {
  TEST("Icon: draggable icons still click when released without moving");
  icon_params_t params = { .draggable = true };
  test_env_init(); reset_counts();
  window_t *parent = test_env_create_window("parent", 0, 0, 200, 160, icon_parent_proc, NULL);
  window_t *icon = create_window("Desk", 0, MAKERECT(20, 20, 100, 100), parent, win_icon, 0, &params);
  send_message(icon, evLeftButtonDown, MAKEDWORD(10, 10), NULL);
  send_message(icon, evMouseMove, MAKEDWORD(12, 12), NULL);
  send_message(icon, evLeftButtonUp, MAKEDWORD(12, 12), NULL);
  ASSERT_EQUAL(icon->frame.x, 20); ASSERT_EQUAL(icon->frame.y, 20); ASSERT_EQUAL(click_count, 1);
  destroy_window(parent); test_env_shutdown(); PASS();
}

static void test_artifact_drag_notifies_drop_target(void) {
  TEST("Icon: artefacts are copied, counted, and draggable to sibling icons");
  icon_artifact_t artifacts[] = {{ .id = 7, .count = 2, .label = "Ticket", .item_data = (void *)42 }};
  test_env_init(); reset_counts();
  window_t *parent = test_env_create_window("parent", 0, 0, 420, 200, icon_parent_proc, NULL);
  window_t *source = create_window("Manager", 0, MAKERECT(0, 0, 180, 160), parent, win_icon, 0, NULL);
  window_t *target = create_window("Developer", 0, MAKERECT(220, 0, 180, 160), parent, win_icon, 0, NULL);
  ASSERT_TRUE(send_message(source, icSetArtifacts, 1, artifacts));
  artifacts[0].id = 99; artifacts[0].count = 9;
  send_message(source, evLeftButtonDown, MAKEDWORD(150, 70), NULL);
  send_message(source, evMouseMove, MAKEDWORD(240, 70), NULL);
  window_t *ghost = artifact_drag_ghost(parent);
  ASSERT_NOT_NULL(ghost);
  ASSERT_EQUAL(ghost->frame.x, window_screen_x(source) + 233);
  ASSERT_EQUAL(ghost->frame.y, window_screen_y(source) + 59);
  send_message(source, evLeftButtonUp, MAKEDWORD(240, 70), NULL);
  ASSERT_EQUAL(artifact_drop_count, 1); ASSERT_EQUAL(dropped_artifact_id, 7);
  ASSERT_EQUAL(last_drop_source, source); ASSERT_EQUAL(last_drop_target, target);
  ASSERT_NULL(artifact_drag_ghost(parent)); ASSERT_EQUAL(click_count, 0); ASSERT_EQUAL(source->frame.x, 0);

  artifact_drop_accepted = false;
  send_message(source, evLeftButtonDown, MAKEDWORD(150, 70), NULL);
  send_message(source, evMouseMove, MAKEDWORD(240, 70), NULL);
  ASSERT_NOT_NULL(artifact_drag_ghost(parent));
  send_message(source, evLeftButtonUp, MAKEDWORD(240, 70), NULL);
  ASSERT_EQUAL(artifact_drop_count, 2); ASSERT_NULL(artifact_drag_ghost(parent));
  ASSERT_NULL(g_ui_runtime.captured); ASSERT_EQUAL(source->frame.x, 0);
  ASSERT_FALSE(send_message(source, icSetArtifacts, ICON_MAX_ARTIFACTS + 1, artifacts));
  destroy_window(parent); test_env_shutdown(); PASS();
}

int main(void) {
  TEST_START("Icon Control");
  test_icon_selection_is_exclusive();
  test_icon_notifications_and_item_data();
  test_icon_badges_copy_input();
  test_icon_status_image_can_be_set_and_cleared();
  test_draggable_icon_moves_without_clicking();
  test_draggable_icon_still_clicks_without_moving();
  test_artifact_drag_notifies_drop_target();
  TEST_END();
}
