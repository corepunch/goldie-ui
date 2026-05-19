// tools/tool_stubs.c — Stub handlers for remaining unimplemented tools
// NOTE: brush, eraser, fill, spray, and shape tools are now in separate files

#include "tools.h"

// ── Stub lifecycle functions ───────────────────────────────────────────────

static void stub_begin(canvas_doc_t *doc, canvas_win_state_t *view, ipoint16_t doc_pt) {
  (void)doc; (void)view; (void)doc_pt;
}

static void stub_drag(canvas_doc_t *doc, canvas_win_state_t *view, ipoint16_t doc_pt) {
  (void)doc; (void)view; (void)doc_pt;
}

static void stub_end(canvas_doc_t *doc, canvas_win_state_t *view, ipoint16_t doc_pt) {
  (void)doc; (void)view; (void)doc_pt;
}

static void stub_cancel(canvas_doc_t *doc, canvas_win_state_t *view) {
  (void)doc; (void)view;
}

static bool stub_key(canvas_doc_t *doc, canvas_win_state_t *view, uint32_t key, uint32_t mods) {
  (void)doc; (void)view; (void)key; (void)mods;
  return false;
}

// ── Stub handler definitions (tools without implementations yet) ───────────

const tool_handler_t tool_crop_handler = {
  .id = ID_TOOL_CROP, .name = "Crop",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_move_handler = {
  .id = ID_TOOL_MOVE, .name = "Move",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_polygon_handler = {
  .id = ID_TOOL_POLYGON, .name = "Polygon",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_hand_handler = {
  .id = ID_TOOL_HAND, .name = "Hand",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_zoom_handler = {
  .id = ID_TOOL_ZOOM, .name = "Zoom",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_eyedropper_handler = {
  .id = ID_TOOL_EYEDROPPER, .name = "Eyedropper",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_magnifier_handler = {
  .id = ID_TOOL_MAGNIFIER, .name = "Magnifier",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_text_handler = {
  .id = ID_TOOL_TEXT, .name = "Text",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_magic_wand_handler = {
  .id = ID_TOOL_MAGIC_WAND, .name = "Magic Wand",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_eyedropper_handler = {
  .id = ID_TOOL_EYEDROPPER, .name = "Eyedropper",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_magnifier_handler = {
  .id = ID_TOOL_MAGNIFIER, .name = "Magnifier",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_text_handler = {
  .id = ID_TOOL_TEXT, .name = "Text",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};

const tool_handler_t tool_magic_wand_handler = {
  .id = ID_TOOL_MAGIC_WAND, .name = "Magic Wand",
  .begin = stub_begin, .drag = stub_drag, .end = stub_end,
  .cancel = stub_cancel, .key = stub_key,
};
