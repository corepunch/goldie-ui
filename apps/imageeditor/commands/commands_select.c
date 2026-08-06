// commands/commands_select.c — Selection command implementations

#include "commands.h"

// ── Selection commands ─────────────────────────────────────────────────────

void cmd_select_all(canvas_doc_t *doc) {
  if (!doc) return;
  
  ie_doc_begin_op(doc, "Select All");
  canvas_select_all(doc);
  ie_doc_commit_op(doc, true);
}

void cmd_deselect(canvas_doc_t *doc) {
  if (!doc) return;
  
  ie_doc_begin_op(doc, "Deselect");
  canvas_deselect(doc);
  ie_doc_commit_op(doc, true);
}

void cmd_select_clear(canvas_doc_t *doc) {
  if (!doc || !doc->sel.active) return;
  
  ie_doc_begin_op(doc, "Clear Selection");
  canvas_clear_selection(doc, MAKE_COLOR(0, 0, 0, 0));
  ie_doc_commit_op(doc, true);
}

void cmd_select_expand(canvas_doc_t *doc, int amount) {
  if (!doc || !doc->sel.active) return;
  
  ie_doc_begin_op(doc, "Expand Selection");
  bool ok = canvas_expand_selection(doc, amount);
  ie_doc_commit_op(doc, ok);
}

void cmd_select_contract(canvas_doc_t *doc, int amount) {
  if (!doc || !doc->sel.active) return;
  
  ie_doc_begin_op(doc, "Contract Selection");
  bool ok = canvas_contract_selection(doc, amount);
  ie_doc_commit_op(doc, ok);
}

void cmd_crop_to_selection(canvas_doc_t *doc) {
  if (!doc || !doc->sel.active) return;
  
  ie_doc_begin_op(doc, "Crop to Selection");
  canvas_crop_to_selection(doc);
  ie_doc_commit_op(doc, true);
}
