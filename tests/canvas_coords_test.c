// tests/canvas_coords_test.c — Tests for coordinate conversion helpers (Phase 1)

#include "test_framework.h"
#include "../examples/imageeditor/imageeditor.h"

static void ie_setup(void) {
  ui_init_graphics(0, \"Test\", 800, 600);
}

static void ie_teardown(void) {
  ui_shutdown_graphics();
}

// ── Test coordinate conversion ─────────────────────────────────────────────

void test_view_to_doc_at_100_percent_no_pan(void) {
  TEST("canvas_view_to_doc_point: 100% scale, no pan");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 64, 64);
  ASSERT_NOT_NULL(doc);
  
  window_t *win = create_window("Canvas", 0, MAKERECT(0, 0, 200, 200),
                                NULL, NULL, NULL);
  canvas_win_state_t state = {
    .doc = doc,
    .scale = 100,
    .pan = {0, 0},
  };
  
  // At 100% with no pan, view coords should map 1:1 to doc coords
  ipoint16_t doc_pt = canvas_view_to_doc_point(win, &state, 10, 20);
  ASSERT_EQUAL(doc_pt.x, 10);
  ASSERT_EQUAL(doc_pt.y, 20);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

void test_view_to_doc_at_200_percent(void) {
  TEST("canvas_view_to_doc_point: 200% scale doubles coordinates");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 64, 64);
  ASSERT_NOT_NULL(doc);
  
  window_t *win = create_window("Canvas", 0, MAKERECT(0, 0, 200, 200),
                                NULL, NULL, NULL);
  canvas_win_state_t state = {
    .doc = doc,
    .scale = 200,
    .pan = {0, 0},
  };
  
  // At 200%, view pixel 20 should map to doc pixel 10
  ipoint16_t doc_pt = canvas_view_to_doc_point(win, &state, 20, 40);
  ASSERT_EQUAL(doc_pt.x, 10);
  ASSERT_EQUAL(doc_pt.y, 20);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

void test_view_to_doc_with_pan(void) {
  TEST("canvas_view_to_doc_point: pan offset shifts coordinates");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 64, 64);
  ASSERT_NOT_NULL(doc);
  
  window_t *win = create_window("Canvas", 0, MAKERECT(0, 0, 200, 200),
                                NULL, NULL, NULL);
  canvas_win_state_t state = {
    .doc = doc,
    .scale = 100,
    .pan = {10, 20},  // Panned 10 pixels right, 20 pixels down
  };
  
  // Pan shifts the mapping
  ipoint16_t doc_pt = canvas_view_to_doc_point(win, &state, 0, 0);
  ASSERT_EQUAL(doc_pt.x, 10);   // View (0,0) maps to doc (10, 20)
  ASSERT_EQUAL(doc_pt.y, 20);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

void test_doc_to_view_inverse_of_view_to_doc(void) {
  TEST("canvas_doc_to_view_point: inverse of view_to_doc");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 64, 64);
  ASSERT_NOT_NULL(doc);
  
  window_t *win = create_window("Canvas", 0, MAKERECT(0, 0, 200, 200),
                                NULL, NULL, NULL);
  canvas_win_state_t state = {
    .doc = doc,
    .scale = 150,
    .pan = {5, 10},
  };
  
  // Forward and back should give original coordinates
  ipoint16_t doc_pt = canvas_view_to_doc_point(win, &state, 30, 45);
  ipoint16_t view_pt = canvas_doc_to_view_point(win, &state, doc_pt.x, doc_pt.y);
  
  ASSERT_EQUAL(view_pt.x, 30);
  ASSERT_EQUAL(view_pt.y, 45);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

void test_rect_conversion(void) {
  TEST("canvas_doc_rect_to_view: converts rectangle coordinates");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 64, 64);
  ASSERT_NOT_NULL(doc);
  
  window_t *win = create_window("Canvas", 0, MAKERECT(0, 0, 200, 200),
                                NULL, NULL, NULL);
  canvas_win_state_t state = {
    .doc = doc,
    .scale = 200,  // 2x zoom
    .pan = {0, 0},
  };
  
  // Doc rect (10,10)-(20,20) should map to view rect (20,20)-(40,40) at 2x
  irect16_t view_rect = canvas_doc_rect_to_view(win, &state, 10, 10, 20, 20);
  ASSERT_EQUAL(view_rect.x, 20);
  ASSERT_EQUAL(view_rect.y, 20);
  ASSERT_EQUAL(view_rect.w, 20);  // Width/height also scale
  ASSERT_EQUAL(view_rect.h, 20);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

// ── Test suite ──────────────────────────────────────────────────────────────

int main(void) {
  TEST_START("Canvas Coordinate Conversion Tests");
  
  test_view_to_doc_at_100_percent_no_pan();
  test_view_to_doc_at_200_percent();
  test_view_to_doc_with_pan();
  test_doc_to_view_inverse_of_view_to_doc();
  test_rect_conversion();
  
  TEST_END();
  return 0;
}
