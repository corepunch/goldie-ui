#include "test_framework.h"
#include "scener.h"

app_state_t *g_app;

static void test_tool_commands_share_document_state(void) {
  TEST("scener tools: command IDs update the document source of truth");
  app_state_t app = {0};
  scene_doc_t doc = {0};
  g_app = &app;
  app.docs = app.active_doc = &doc;

  handle_menu_command(ID_TOOL_SELECT); ASSERT_EQUAL(doc.scene.editMode, EDIT_Q_SELECT); ASSERT_EQUAL(scener_active_tool(), ID_TOOL_SELECT);
  handle_menu_command(ID_TOOL_MOVE);   ASSERT_EQUAL(doc.scene.editMode, EDIT_W_MOVE);   ASSERT_EQUAL(scener_active_tool(), ID_TOOL_MOVE);
  handle_menu_command(ID_TOOL_ROTATE); ASSERT_EQUAL(doc.scene.editMode, EDIT_E_ROTATE); ASSERT_EQUAL(scener_active_tool(), ID_TOOL_ROTATE);
  handle_menu_command(ID_TOOL_SCALE);  ASSERT_EQUAL(doc.scene.editMode, EDIT_R_SCALE);  ASSERT_EQUAL(scener_active_tool(), ID_TOOL_SCALE);

  g_app = NULL;
  PASS();
}

static void test_prefab_files_open_as_documents(void) {
  TEST("scener documents: prefab files load as editable document roots");
  Scene prefab = {0};
  ASSERT_TRUE(load_scene("apps/scener/prefabs/items/book.blk", &prefab));
  ASSERT_TRUE(scene_is_prefab_mode(&prefab));
  ASSERT_EQUAL(prefab.editDepth, 0);
  ASSERT_TRUE(prefab.nobjs > 0);
  ASSERT_EQUAL(prefab.nlights, 2);
  scene_free(&prefab);

  Scene scene = {0};
  ASSERT_TRUE(load_scene("apps/scener/scenes/test_prefab_tint.blks", &scene));
  scene.selectedObj = 0;
  scene.selectedNode = scene.objs[0].editNode;
  char path[512] = {0};
  ASSERT_TRUE(scene_selected_prefab_path(&scene, path, sizeof(path)));
  ASSERT_TRUE(strstr(path, "prefabs/items/book.blk") != NULL);
  scene_free(&scene);
  PASS();
}

int main(void) {
  TEST_START("scener input and command state");
  test_tool_commands_share_document_state();
  test_prefab_files_open_as_documents();
  TEST_END();
}
