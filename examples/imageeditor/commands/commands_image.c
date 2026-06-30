// commands/commands_image.c — Image command implementations

#include "commands.h"

// ── Image commands ─────────────────────────────────────────────────────────

void cmd_flip_horizontal(canvas_doc_t *doc) {
  if (!doc) return;
  
  ie_doc_begin_op(doc, "Flip Horizontal");
  canvas_flip_h(doc);
  ie_doc_commit_op(doc, true);
}

void cmd_flip_vertical(canvas_doc_t *doc) {
  if (!doc) return;
  
  ie_doc_begin_op(doc, "Flip Vertical");
  canvas_flip_v(doc);
  ie_doc_commit_op(doc, true);
}

void cmd_invert_colors(canvas_doc_t *doc) {
  if (!doc) return;
  
  ie_doc_begin_op(doc, "Invert Colors");
  canvas_invert_colors(doc);
  ie_doc_commit_op(doc, true);
}

void cmd_resize_image(canvas_doc_t *doc, int new_w, int new_h, image_resize_filter_t filter) {
  if (!doc) return;
  
  ie_doc_begin_op(doc, "Resize Image");
  bool ok = canvas_resize_image(doc, new_w, new_h, filter);
  ie_doc_commit_op(doc, ok);
  
  // Update scrollbars after canvas size changes
  if (ok && doc->canvas_win)
    canvas_win_sync_scrollbars(doc->canvas_win);
}

void cmd_resize_canvas(canvas_doc_t *doc, int new_w, int new_h) {
  if (!doc) return;
  
  ie_doc_begin_op(doc, "Canvas Size");
  bool ok = canvas_resize(doc, new_w, new_h);
  ie_doc_commit_op(doc, ok);
  
  // Update scrollbars after canvas size changes
  if (ok && doc->canvas_win)
    canvas_win_sync_scrollbars(doc->canvas_win);
}
