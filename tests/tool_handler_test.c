// tests/tool_handler_test.c — Tests for tool handler system (Phase 3)

#include "test_framework.h"
#include "../examples/imageeditor/imageeditor.h"
#include "../examples/imageeditor/tools/tools.h"

static void ie_setup(void) {
  ui_init_graphics(0, \"Test\", 800, 600);
  register_builtin_tools();
}

static void ie_teardown(void) {
  ui_shutdown_graphics();
}

// ── Test tool handler registry ─────────────────────────────────────────────

void test_get_tool_handler_valid(void) {
  TEST("get_tool_handler returns valid handler for implemented tools");
  
  ie_setup();
  
  const tool_handler_t *pencil = get_tool_handler(ID_TOOL_PENCIL);
  ASSERT_NOT_NULL(pencil);
  ASSERT_EQUAL(pencil->id, ID_TOOL_PENCIL);
  ASSERT_NOT_NULL(pencil->name);
  ASSERT_NOT_NULL(pencil->begin);
  
  const tool_handler_t *brush = get_tool_handler(ID_TOOL_BRUSH);
  ASSERT_NOT_NULL(brush);
  ASSERT_EQUAL(brush->id, ID_TOOL_BRUSH);
  
  const tool_handler_t *line = get_tool_handler(ID_TOOL_LINE);
  ASSERT_NOT_NULL(line);
  ASSERT_EQUAL(line->id, ID_TOOL_LINE);
  
  ie_teardown();
  PASS();
}

void test_get_tool_handler_stub(void) {
  TEST("get_tool_handler returns stub handler for unimplemented tools");
  
  ie_setup();
  
  const tool_handler_t *crop = get_tool_handler(ID_TOOL_CROP);
  ASSERT_NOT_NULL(crop);
  ASSERT_EQUAL(crop->id, ID_TOOL_CROP);
  ASSERT_NOT_NULL(crop->begin);  // Stub has no-op functions
  
  ie_teardown();
  PASS();
}

void test_get_tool_handler_invalid(void) {
  TEST("get_tool_handler returns NULL for invalid tool ID");
  
  ie_setup();
  
  const tool_handler_t *invalid = get_tool_handler(9999);
  ASSERT_NULL(invalid);
  
  ie_teardown();
  PASS();
}

// ── Test tool lifecycle ────────────────────────────────────────────────────

void test_pencil_begin_end_lifecycle(void) {
  TEST("Pencil tool: begin/drag/end lifecycle completes successfully");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  window_t *win = create_window("Canvas", 0, MAKERECT(0, 0, 200, 200),
                                NULL, NULL, NULL);
  canvas_win_state_t state = {.doc = doc, .scale = 100, .pan = {0, 0}};
  
  const tool_handler_t *pencil = get_tool_handler(ID_TOOL_PENCIL);
  ASSERT_NOT_NULL(pencil);
  
  // Begin stroke
  pencil->begin(doc, &state, (ipoint16_t){10, 10});
  ASSERT_EQUAL(doc->undo.count, 1);  // Undo pushed
  
  // Drag
  if (pencil->drag) {
    pencil->drag(doc, &state, (ipoint16_t){20, 20});
  }
  
  // End stroke
  if (pencil->end) {
    pencil->end(doc, &state, (ipoint16_t){20, 20});
  }
  
  ASSERT_TRUE(doc->modified);  // Document marked dirty
  
  close_document(doc);
  ie_teardown();
  PASS();
}

void test_line_preview_lifecycle(void) {
  TEST("Line tool: begin/drag (preview)/end (commit) lifecycle");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  window_t *win = create_window("Canvas", 0, MAKERECT(0, 0, 200, 200),
                                NULL, NULL, NULL);
  canvas_win_state_t state = {.doc = doc, .scale = 100, .pan = {0, 0}};
  
  const tool_handler_t *line = get_tool_handler(ID_TOOL_LINE);
  ASSERT_NOT_NULL(line);
  
  // Begin (saves snapshot)
  line->begin(doc, &state, (ipoint16_t){5, 5});
  ASSERT_NOT_NULL(doc->shape.snapshot);  // Snapshot saved for preview
  
  // Drag (shows preview, restores snapshot repeatedly)
  if (line->drag) {
    line->drag(doc, &state, (ipoint16_t){15, 15});
    line->drag(doc, &state, (ipoint16_t){20, 20});
  }
  
  // End (commits final line)
  if (line->end) {
    line->end(doc, &state, (ipoint16_t){25, 25});
  }
  
  ASSERT_TRUE(doc->modified);
  ASSERT_NULL(doc->shape.snapshot);  // Snapshot freed after commit
  
  close_document(doc);
  ie_teardown();
  PASS();
}

void test_tool_cancel_discards(void) {
  TEST("Tool cancel handler discards operation");
  
  ie_setup();
  canvas_doc_t *doc = create_document(NULL, 32, 32);
  ASSERT_NOT_NULL(doc);
  
  window_t *win = create_window("Canvas", 0, MAKERECT(0, 0, 200, 200),
                                NULL, NULL, NULL);
  canvas_win_state_t state = {.doc = doc, .scale = 100, .pan = {0, 0}};
  
  const tool_handler_t *pencil = get_tool_handler(ID_TOOL_PENCIL);
  ASSERT_NOT_NULL(pencil);
  
  // Begin stroke
  pencil->begin(doc, &state, (ipoint16_t){10, 10});
  int undo_count = doc->undo.count;
  ASSERT_TRUE(undo_count > 0);
  
  // Cancel
  if (pencil->cancel) {
    pencil->cancel(doc, &state);
  }
  
  ASSERT_EQUAL(doc->undo.count, 0);  // Undo discarded
  ASSERT_FALSE(doc->modified);  // Document not marked dirty
  
  close_document(doc);
  ie_teardown();
  PASS();
}

// ── Test suite ──────────────────────────────────────────────────────────────

int main(void) {
  TEST_START("Tool Handler System Tests");
  
  test_get_tool_handler_valid();
  test_get_tool_handler_stub();
  test_get_tool_handler_invalid();
  test_pencil_begin_end_lifecycle();
  test_line_preview_lifecycle();
  test_tool_cancel_discards();
  
  TEST_END();
  return 0;
}
