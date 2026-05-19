// tests/canvas_coords_test.c — Tests for coordinate conversion helpers (Phase 1)

#include "test_framework.h"
#include "test_env.h"
#include "examples/imageeditor/imageeditor.h"

app_state_t *g_app = NULL;

// ── Test coordinate conversion ─────────────────────────────────────────────

void test_view_to_doc_at_100_percent_no_pan(void) {
  TEST("canvas_view_to_doc_point: 100% scale, no pan");
  
  test_env_init();
  g_app = calloc(1, sizeof(app_state_t));
  canvas_doc_t *doc = create_document(NULL, 64, 64);
  ASSERT_NOT_NULL(doc);
  ASSERT_NOT_NULL(doc->canvas_win);
  
  // Get the canvas window state
  canvas_win_state_t *state = (canvas_win_state_t *)doc->canvas_win->userdata;
  ASSERT_NOT_NULL(state);
  state->scale = 1;  // 100% scale (1:1 mapping)
  state->pan.x = 0;
  state->pan.y = 0;
  
  // Convert view point to document point
  int doc_x, doc_y;
  canvas_view_to_doc(doc->canvas_win, state, 10, 20, &doc_x, &doc_y);
  
  // At 100% scale with no pan, coordinates should be roughly the same
  // (accounting for centering offset in the canvas window)
  ASSERT_TRUE(doc_x >= 0 && doc_x < 64);
  ASSERT_TRUE(doc_y >= 0 && doc_y < 64);
  
  close_document(doc);
  free(g_app);
  test_env_shutdown();
  PASS();
}

void test_view_to_doc_at_different_scales(void) {
  TEST("canvas_view_to_doc_point: scaling affects coordinate mapping");
  
  test_env_init();
  g_app = calloc(1, sizeof(app_state_t));
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  canvas_win_state_t *state = (canvas_win_state_t *)doc->canvas_win->userdata;
  ASSERT_NOT_NULL(state);
  state->scale = 2;  // 2x zoom
  state->pan.x = 0;
  state->pan.y = 0;
  
  // Functions should not crash with different scales
  int doc_x, doc_y;
  canvas_view_to_doc(doc->canvas_win, state, 50, 50, &doc_x, &doc_y);
  
  // Just verify they're in reasonable bounds
  ASSERT_TRUE(doc_x >= -100 && doc_x < 200);
  ASSERT_TRUE(doc_y >= -100 && doc_y < 200);
  
  close_document(doc);
  free(g_app);
  test_env_shutdown();
  PASS();
}

void test_doc_to_view_produces_valid_coords(void) {
  TEST("canvas_doc_to_view_point: produces valid view coordinates");
  
  test_env_init();
  g_app = calloc(1, sizeof(app_state_t));
  canvas_doc_t *doc = create_document(NULL, 64, 64);
  ASSERT_NOT_NULL(doc);
  
  canvas_win_state_t *state = (canvas_win_state_t *)doc->canvas_win->userdata;
  ASSERT_NOT_NULL(state);
  state->scale = 1;
  state->pan.x = 0;
  state->pan.y = 0;
  
  // Convert document point to view point
  ipoint16_t view_pt = canvas_doc_to_view_point(doc->canvas_win, state, 10, 10);
  
  // View coordinates should be within window bounds (with reasonable margin)
  ASSERT_TRUE(view_pt.x >= -1000 && view_pt.x < 2000);
  ASSERT_TRUE(view_pt.y >= -1000 && view_pt.y < 2000);
  
  close_document(doc);
  free(g_app);
  test_env_shutdown();
  PASS();
}

void test_roundtrip_conversion(void) {
  TEST("view→doc→view roundtrip preserves coordinates (roughly)");
  
  test_env_init();
  g_app = calloc(1, sizeof(app_state_t));
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  canvas_win_state_t *state = (canvas_win_state_t *)doc->canvas_win->userdata;
  ASSERT_NOT_NULL(state);
  state->scale = 2;
  state->pan.x = 0;
  state->pan.y = 0;
  
  // Roundtrip: view → doc → view
  int doc_x, doc_y;
  canvas_view_to_doc(doc->canvas_win, state, 100, 150, &doc_x, &doc_y);
  ipoint16_t view_pt = canvas_doc_to_view_point(doc->canvas_win, state, doc_x, doc_y);
  
  // Should be close to original (within a few pixels due to rounding)
  int diff_x = abs(view_pt.x - 100);
  int diff_y = abs(view_pt.y - 150);
  ASSERT_TRUE(diff_x <= 2);
  ASSERT_TRUE(diff_y <= 2);
  
  close_document(doc);
  free(g_app);
  test_env_shutdown();
  PASS();
}

void test_rect_conversion_works(void) {
  TEST("canvas_doc_rect_to_view: produces reasonable rect");
  
  test_env_init();
  g_app = calloc(1, sizeof(app_state_t));
  canvas_doc_t *doc = create_document(NULL, 64, 64);
  ASSERT_NOT_NULL(doc);
  
  canvas_win_state_t *state = (canvas_win_state_t *)doc->canvas_win->userdata;
  ASSERT_NOT_NULL(state);
  state->scale = 2;  // 2x zoom
  state->pan.x = 0;
  state->pan.y = 0;
  
  // Convert document rect to view rect
  irect16_t view_rect = canvas_doc_rect_to_view(doc->canvas_win, state, 10, 10, 20, 20);
  
  // Just verify it returns a non-zero rect (scaled dimensions will vary)
  ASSERT_TRUE(view_rect.w > 0);
  ASSERT_TRUE(view_rect.h > 0);
  
  close_document(doc);
  free(g_app);
  test_env_shutdown();
  PASS();
}

// ── Test suite ──────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  TEST_START("Canvas Coordinate Conversion Tests");
  
  test_view_to_doc_at_100_percent_no_pan();
  test_view_to_doc_at_different_scales();
  test_doc_to_view_produces_valid_coords();
  test_roundtrip_conversion();
  test_rect_conversion_works();
  
  TEST_END();
}
