// commands/commands_edit.c — Edit command implementations

#include "commands.h"

// ── Edit commands ──────────────────────────────────────────────────────────

void cmd_undo(canvas_doc_t *doc) {
  if (!doc) return;
  if (doc_undo(doc)) {
    ie_doc_update_title(doc);
    ie_doc_invalidate_canvas(doc);
  }
}

void cmd_redo(canvas_doc_t *doc) {
  if (!doc) return;
  if (doc_redo(doc)) {
    ie_doc_update_title(doc);
    ie_doc_invalidate_canvas(doc);
  }
}

void cmd_cut(canvas_doc_t *doc) {
  if (!doc || !doc->sel.active) return;
  
  ie_doc_begin_op(doc, "Cut");
  canvas_cut_selection(doc, MAKE_COLOR(0, 0, 0, 0));
  ie_doc_commit_op(doc, true);
}

void cmd_copy(canvas_doc_t *doc) {
  if (!doc || !doc->sel.active) return;
  canvas_copy_selection(doc);
}

void cmd_paste(canvas_doc_t *doc) {
  if (!doc) return;
  
  // Commit any in-progress selection move before pasting
  if (doc->sel.move.active)
    canvas_commit_move(doc);
  
  ie_doc_begin_op(doc, "Paste");
  canvas_paste_clipboard(doc);
  ie_doc_commit_op(doc, true);
}
