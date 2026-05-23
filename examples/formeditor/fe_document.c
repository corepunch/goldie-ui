// Document helper stubs during window-first migration.
// The runtime source of truth is the form XML node + live windows, not
// form_element_t arrays.

#include "formeditor.h"

window_t *fe_doc_create(const char *form_id, int w, int h) {
  (void)form_id;
  (void)w;
  (void)h;
  return NULL;
}

void fe_doc_destroy(window_t *doc) {
  (void)doc;
}

void fe_doc_mark_modified(window_t *doc) {
  form_doc_state_t *st = fe_doc_state(doc);
  if (!doc || !st)
    return;
  st->modified = true;
  fe_notify(FE_EVENT_DOCUMENT_MODIFIED, doc);
}

void fe_doc_update_title(window_t *doc) {
  (void)doc;
}

int fe_doc_add_element(window_t *doc, int type, irect16_t frame, uint32_t parent_id) {
  (void)doc;
  (void)type;
  (void)frame;
  (void)parent_id;
  return -1;
}

bool fe_doc_delete_element(window_t *doc, int idx) {
  (void)doc;
  (void)idx;
  return false;
}

bool fe_doc_set_element_text(window_t *doc, int element_id, const char *text) {
  (void)doc;
  (void)element_id;
  (void)text;
  return false;
}

bool fe_doc_set_element_frame(window_t *doc, int element_id, irect16_t frame) {
  (void)doc;
  (void)element_id;
  (void)frame;
  return false;
}

bool fe_doc_set_element_name(window_t *doc, int element_id, const char *name) {
  (void)doc;
  (void)element_id;
  (void)name;
  return false;
}

bool fe_doc_set_element_align(window_t *doc, int element_id, uint8_t h_align, uint8_t v_align) {
  (void)doc;
  (void)element_id;
  (void)h_align;
  (void)v_align;
  return false;
}

bool fe_doc_set_element_font(window_t *doc, int element_id, uint8_t font) {
  (void)doc;
  (void)element_id;
  (void)font;
  return false;
}

bool fe_doc_set_element_color(window_t *doc, int element_id, uint8_t color) {
  (void)doc;
  (void)element_id;
  (void)color;
  return false;
}

bool fe_doc_set_element_db_field(window_t *doc, int element_id, const char *field) {
  (void)doc;
  (void)element_id;
  (void)field;
  return false;
}

bool fe_doc_set_element_db_source(window_t *doc, int element_id, const char *source) {
  (void)doc;
  (void)element_id;
  (void)source;
  return false;
}

bool fe_doc_set_element_db_display(window_t *doc, int element_id, const char *display) {
  (void)doc;
  (void)element_id;
  (void)display;
  return false;
}

bool fe_doc_set_element_db_value(window_t *doc, int element_id, const char *value) {
  (void)doc;
  (void)element_id;
  (void)value;
  return false;
}

form_element_t *fe_doc_find_element(window_t *doc, uint32_t id) {
  (void)doc;
  (void)id;
  return NULL;
}

int fe_doc_find_element_index(window_t *doc, uint32_t id) {
  (void)doc;
  (void)id;
  return -1;
}

form_element_t *fe_doc_get_element(window_t *doc, int idx) {
  (void)doc;
  (void)idx;
  return NULL;
}

int fe_doc_element_count(const window_t *doc) {
  (void)doc;
  return 0;
}

int fe_doc_resolve_control_id(window_t *doc, const char *expr) {
  (void)doc;
  (void)expr;
  return 0;
}

void fe_doc_make_control_id_expr(char *out, size_t out_sz,
                                 const char *form_id,
                                 const char *name,
                                 const char *class_name,
                                 int ordinal) {
  (void)form_id;
  (void)name;
  (void)class_name;
  (void)ordinal;
  if (!out || out_sz == 0)
    return;
  out[0] = '\0';
}
