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

int main(void) {
  TEST_START("scener input and command state");
  test_tool_commands_share_document_state();
  TEST_END();
}
