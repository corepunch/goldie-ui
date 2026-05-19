// tests/commands_test.c — Tests for command module extraction (Phase 4)

#include "test_framework.h"
#include "../examples/imageeditor/imageeditor.h"
#include "../examples/imageeditor/commands/commands.h"

static void ie_setup(void) {
  ui_init_graphics(0, \"Test\", 800, 600);
}

static void ie_teardown(void) {
  ui_shutdown_graphics();
}

// ── Test edit commands ─────────────────────────────────────────────────────

void test_undo_redo(void) {
  TEST("cmd_undo/cmd_redo reverse and replay operations");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  // Make a change
  ie_doc_begin_op(doc, "Test");
  canvas_draw_pixel(doc, 5, 5, MAKE_COLOR(255, 0, 0, 255));
  ie_doc_commit_op(doc, true);
  ASSERT_TRUE(doc->modified);
  
  // Undo
  cmd_undo(doc);
  ASSERT_FALSE(doc->modified);  // Title updated after undo
  ASSERT_EQUAL(doc->undo.count, 0);
  ASSERT_EQUAL(doc->redo.count, 1);
  
  // Redo
  cmd_redo(doc);
  ASSERT_TRUE(doc->modified);
  ASSERT_EQUAL(doc->undo.count, 1);
  ASSERT_EQUAL(doc->redo.count, 0);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

void test_copy_paste(void) {
  TEST("cmd_copy/cmd_paste duplicates selection");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  // Create selection
  doc->sel.active = true;
  doc->sel.start = (ipoint16_t){5, 5};
  doc->sel.end = (ipoint16_t){15, 15};
  canvas_update_selection_mask(doc);
  
  // Copy
  cmd_copy(doc);  // Should not mark dirty (read-only)
  ASSERT_FALSE(doc->modified);
  
  // Paste
  cmd_paste(doc);
  ASSERT_TRUE(doc->modified);  // Paste marks dirty
  
  close_document(doc);
  ie_teardown();
  PASS();
}

// ── Test selection commands ────────────────────────────────────────────────

void test_select_all_deselect(void) {
  TEST("cmd_select_all/cmd_deselect manage selection state");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  ASSERT_FALSE(doc->sel.active);
  
  // Select all
  cmd_select_all(doc);
  ASSERT_TRUE(doc->sel.active);
  ASSERT_EQUAL(doc->sel.start.x, 0);
  ASSERT_EQUAL(doc->sel.start.y, 0);
  ASSERT_EQUAL(doc->sel.end.x, 32);
  ASSERT_EQUAL(doc->sel.end.y, 32);
  
  // Deselect
  cmd_deselect(doc);
  ASSERT_FALSE(doc->sel.active);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

void test_expand_contract_selection(void) {
  TEST("cmd_expand_selection/cmd_contract_selection adjust selection bounds");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  // Create initial selection
  doc->sel.active = true;
  doc->sel.start = (ipoint16_t){10, 10};
  doc->sel.end = (ipoint16_t){20, 20};
  
  // Expand
  cmd_expand_selection(doc, 2);
  ASSERT_EQUAL(doc->sel.start.x, 8);   // Expanded by 2 pixels
  ASSERT_EQUAL(doc->sel.start.y, 8);
  ASSERT_EQUAL(doc->sel.end.x, 22);
  ASSERT_EQUAL(doc->sel.end.y, 22);
  
  // Contract
  cmd_contract_selection(doc, 3);
  ASSERT_EQUAL(doc->sel.start.x, 11);  // Contracted by 3 pixels
  ASSERT_EQUAL(doc->sel.start.y, 11);
  ASSERT_EQUAL(doc->sel.end.x, 19);
  ASSERT_EQUAL(doc->sel.end.y, 19);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

// ── Test image commands ────────────────────────────────────────────────────

void test_flip_operations(void) {
  TEST("cmd_flip_horizontal/cmd_flip_vertical transform image");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  // Draw a pixel
  canvas_draw_pixel(doc, 5, 10, MAKE_COLOR(255, 0, 0, 255));
  uint32_t original = canvas_get_pixel(doc, 5, 10);
  
  // Flip horizontal
  cmd_flip_horizontal(doc);
  uint32_t flipped_h = canvas_get_pixel(doc, 32 - 5 - 1, 10);
  ASSERT_EQUAL(original, flipped_h);  // Pixel moved to opposite side
  
  // Flip vertical
  cmd_flip_vertical(doc);
  uint32_t flipped_v = canvas_get_pixel(doc, 32 - 5 - 1, 32 - 10 - 1);
  ASSERT_EQUAL(original, flipped_v);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

void test_invert(void) {
  TEST("cmd_invert inverts colors");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  // Draw a white pixel
  canvas_draw_pixel(doc, 10, 10, MAKE_COLOR(255, 255, 255, 255));
  
  // Invert
  cmd_invert(doc);
  uint32_t inverted = canvas_get_pixel(doc, 10, 10);
  
  // White should become black (with alpha preserved)
  uint8_t r = (inverted >> 24) & 0xFF;
  uint8_t g = (inverted >> 16) & 0xFF;
  uint8_t b = (inverted >> 8) & 0xFF;
  ASSERT_EQUAL(r, 0);
  ASSERT_EQUAL(g, 0);
  ASSERT_EQUAL(b, 0);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

// ── Test layer commands ────────────────────────────────────────────────────

void test_new_delete_layer(void) {
  TEST("cmd_new_layer/cmd_delete_layer manage layer stack");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  int initial_count = doc->layer.count;
  
  // Add layer
  cmd_new_layer(doc);
  ASSERT_EQUAL(doc->layer.count, initial_count + 1);
  ASSERT_TRUE(doc->modified);
  
  // Delete layer
  doc->modified = false;  // Reset
  cmd_delete_layer(doc);
  ASSERT_EQUAL(doc->layer.count, initial_count);
  ASSERT_TRUE(doc->modified);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

void test_duplicate_layer(void) {
  TEST("cmd_duplicate_layer copies active layer");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  // Draw on active layer
  canvas_draw_pixel(doc, 5, 5, MAKE_COLOR(128, 64, 32, 255));
  
  int count_before = doc->layer.count;
  
  // Duplicate
  cmd_duplicate_layer(doc);
  ASSERT_EQUAL(doc->layer.count, count_before + 1);
  
  // Check pixel exists in both layers
  layer_t *original = doc->layer.stack[count_before - 1];
  layer_t *duplicate = doc->layer.stack[count_before];
  
  int idx = (5 * doc->canvas_w + 5) * DOC_BPP;
  ASSERT_EQUAL(original->pixels[idx], duplicate->pixels[idx]);
  
  close_document(doc);
  ie_teardown();
  PASS();
}

// ── Test suite ──────────────────────────────────────────────────────────────

int main(void) {
  TEST_START("Command Module Tests");
  
  test_undo_redo();
  test_copy_paste();
  test_select_all_deselect();
  test_expand_contract_selection();
  test_flip_operations();
  test_invert();
  test_new_delete_layer();
  test_duplicate_layer();
  
  TEST_END();
  return 0;
}
