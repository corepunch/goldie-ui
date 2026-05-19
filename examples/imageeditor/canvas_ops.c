// canvas_ops.c — Document mutation lifecycle API
// Centralizes the repeated undo/dirty/refresh pattern across ImageEditor

#include "imageeditor.h"

// ── Operation lifecycle state ──────────────────────────────────────────────

typedef struct {
  canvas_doc_t *doc;
  const char   *op_name;  // for debugging/logging (not used yet)
  bool          active;   // true between begin_op and commit_op
} doc_op_ctx_t;

static doc_op_ctx_t g_op_ctx = {0};

// ── Core API ───────────────────────────────────────────────────────────────

void ie_doc_begin_op(canvas_doc_t *doc, const char *op_name) {
  if (!doc) return;
  if (g_op_ctx.active) {
    // Nested operation — not supported, but don't crash
    return;
  }
  g_op_ctx.doc = doc;
  g_op_ctx.op_name = op_name;
  g_op_ctx.active = true;
  doc_push_undo(doc);
}

void ie_doc_commit_op(canvas_doc_t *doc, bool success) {
  if (!doc || !g_op_ctx.active || g_op_ctx.doc != doc) {
    return;
  }
  g_op_ctx.active = false;
  g_op_ctx.doc = NULL;
  g_op_ctx.op_name = NULL;

  if (success) {
    ie_doc_mark_dirty(doc);
    ie_doc_update_title(doc);
    ie_doc_invalidate_all(doc);
  } else {
    doc_discard_undo(doc);
  }
}

// ── Dirty state management ─────────────────────────────────────────────────

void ie_doc_mark_dirty(canvas_doc_t *doc) {
  if (!doc) return;
  doc->modified = true;
}

void ie_doc_update_title(canvas_doc_t *doc) {
  if (!doc) return;
  doc_update_title(doc);
}

// ── Invalidation helpers ───────────────────────────────────────────────────

void ie_doc_invalidate_all(canvas_doc_t *doc) {
  if (!doc) return;
  ie_doc_invalidate_canvas(doc);
  ie_doc_invalidate_layers(doc);
  ie_doc_invalidate_timeline(doc);
}

void ie_doc_invalidate_canvas(canvas_doc_t *doc) {
  if (!doc || !doc->canvas_win) return;
  invalidate_window(doc->canvas_win);
}

void ie_doc_invalidate_layers(canvas_doc_t *doc) {
  if (!doc) return;
  layers_win_refresh();
}

void ie_doc_invalidate_timeline(canvas_doc_t *doc) {
  if (!doc) return;
  timeline_win_refresh();
}

// ── Targeted refresh hooks ─────────────────────────────────────────────────

void ie_doc_after_pixels_changed(canvas_doc_t *doc) {
  if (!doc) return;
  ie_doc_invalidate_canvas(doc);
}

void ie_doc_after_layers_changed(canvas_doc_t *doc) {
  if (!doc) return;
  ie_doc_invalidate_canvas(doc);
  ie_doc_invalidate_layers(doc);
}

void ie_doc_after_selection_changed(canvas_doc_t *doc) {
  if (!doc) return;
  ie_doc_invalidate_canvas(doc);
}
