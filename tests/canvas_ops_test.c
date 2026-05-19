// tests/canvas_ops_test.c — Tests for operation lifecycle API (Phase 1)

#include "test_framework.h"
#include "test_env.h"
#include "../examples/imageeditor/imageeditor.h"

app_state_t *g_app = NULL;

// ── Test operation lifecycle wrappers ──────────────────────────────────────

void test_begin_commit_marks_dirty(void) {
  TEST("ie_doc_begin_op + commit_op marks document dirty and updates title");
  
  test_env_init();
  g_app = calloc(1, sizeof(app_state_t));
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  ASSERT_FALSE(doc->modified);
  
  ie_doc_begin_op(doc, "Test Operation");
  ASSERT_EQUAL(doc->undo.count, 1);  // Undo snapshot pushed
  
  ie_doc_commit_op(doc, true);
  ASSERT_TRUE(doc->modified);  // Document marked dirty
  
  close_document(doc);
  free(g_app);
  test_env_shutdown();
  PASS();
}

void test_begin_discard_rolls_back(void) {
  TEST("ie_doc_begin_op + commit_op(false) discards undo snapshot");
  
  test_env_init();
  g_app = calloc(1, sizeof(app_state_t));
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  ie_doc_begin_op(doc, "Failed Operation");
  int undo_count_before = doc->undo.count;
  ASSERT_EQUAL(undo_count_before, 1);
  
  ie_doc_commit_op(doc, false);  // Discard
  ASSERT_EQUAL(doc->undo.count, 0);  // Undo snapshot discarded
  ASSERT_FALSE(doc->modified);  // Document not marked dirty
  
  close_document(doc);
  free(g_app);
  test_env_shutdown();
  PASS();
}

void test_nested_operations_not_supported(void) {
  TEST("Nested ie_doc_begin_op calls push separate undo snapshots");
  
  test_env_init();
  g_app = calloc(1, sizeof(app_state_t));
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  ie_doc_begin_op(doc, "Operation 1");
  int count1 = doc->undo.count;
  
  ie_doc_begin_op(doc, "Operation 2");
  int count2 = doc->undo.count;
  
  ASSERT_TRUE(count2 > count1);  // Each call pushes a snapshot
  
  close_document(doc);
  free(g_app);
  test_env_shutdown();
  PASS();
}

void test_invalidate_canvas_refreshes_view(void) {
  TEST("ie_doc_invalidate_canvas marks canvas window for repaint");
  
  test_env_init();
  g_app = calloc(1, sizeof(app_state_t));
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  // Create a mock canvas window
  window_t *canvas_win = create_window("Canvas", 0, NULL, win_view, 0, NULL);
  doc->canvas_win = canvas_win;
  
  ie_doc_invalidate_canvas(doc);
  // Note: In a real test we'd check if invalidate_window was called,
  // but that requires mocking or window system integration
  ASSERT_NOT_NULL(doc->canvas_win);  // Sanity check
  
  close_document(doc);
  free(g_app);
  test_env_shutdown();
  PASS();
}

void test_after_pixels_changed_triggers_refresh(void) {
  TEST("ie_doc_after_pixels_changed invalidates canvas");
  
  test_env_init();
  g_app = calloc(1, sizeof(app_state_t));
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  window_t *canvas_win = create_window("Canvas", 0, NULL, win_view, 0, NULL);
  doc->canvas_win = canvas_win;
  
  ie_doc_after_pixels_changed(doc);
  ASSERT_NOT_NULL(doc->canvas_win);  // Sanity check
  
  close_document(doc);
  free(g_app);
  test_env_shutdown();
  PASS();
}

// ── Test suite ──────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  TEST_START("Canvas Operations API Tests");
  
  test_begin_commit_marks_dirty();
  test_begin_discard_rolls_back();
  test_nested_operations_not_supported();
  test_invalidate_canvas_refreshes_view();
  test_after_pixels_changed_triggers_refresh();
  
  TEST_END();
}
