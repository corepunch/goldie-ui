// Window-first document mutations. Live windows are the edit model and the
// XML tree on doc->userdata2 is the persistence model.

#include "formeditor.h"

bool fe_doc_drop_create_component(int component_id, window_t *parent_target) {
  window_t *doc = parent_target ? get_root_window(parent_target) : NULL;
  const fe_component_desc_t *desc = fe_component_at(component_id);
  if (!doc || !desc || !desc->class_name || !desc->proc) {
    fprintf(stderr, "fe_doc_drop_create_component: invalid component_id %d\n", component_id);
    return false;
  }
  if ((desc->capabilities & FE_COMPONENT_PLACEABLE) == 0) {
    fprintf(stderr, "fe_doc_drop_create_component: component '%s' is not placeable\n",
            desc->class_name);
    return false;
  }

  int w = desc->default_layout_size.w > 0 ? desc->default_layout_size.w : 96;
  int h = desc->default_layout_size.h > 0 ? desc->default_layout_size.h : 24;
  window_t *child = create_window(desc->class_name, 0, MAKERECT(0, 0, w, h),
                                  parent_target, desc->class_name, 0, NULL);
  if (!child) return false;
  if (!fe_project_append_component_node(doc, parent_target, child, desc->class_name)) {
    destroy_window(child);
    return false;
  }

  window_layout_sync(doc);
  invalidate_window(doc);
  fe_doc_mark_modified(doc);
  if (g_app) g_app->project.modified = true;
  fe_notify(FE_EVENT_ELEMENT_ADDED, doc);
  return true;
}

void fe_doc_mark_modified(window_t *doc) {
  form_doc_state_t *st = fe_doc_state(doc);
  if (!st) return;
  st->modified = true;
  fe_notify(FE_EVENT_DOCUMENT_MODIFIED, doc);
}
