// Document model implementation
// Pure data layer - no UI dependencies except type definitions.

#include "fe_document.h"
#include "fe_notifications.h"
#include "formeditor.h"
#include "../../commctl/commctl.h"
#include <ctype.h>
#include <string.h>

// External reference to global app state (temporary, will be refactored).
extern app_state_t *g_app;

// ============================================================
// Internal Helpers
// ============================================================

static bool parse_numeric_expr(const char *s, int *out) {
  if (!s || !*s || !out) return false;
  char *end = NULL;
  long n = strtol(s, &end, 0);
  if (!end || *end != '\0') return false;
  *out = (int)n;
  return true;
}

static void upper_ident(char *s) {
  if (!s) return;
  for (; *s; s++) {
    if (*s >= 'a' && *s <= 'z')
      *s = (char)(*s - 'a' + 'A');
  }
}

// ============================================================
// Document Lifecycle
// ============================================================

form_doc_t *fe_doc_create(const char *form_id, int w, int h) {
  if (w <= 0 || h <= 0 || w > INT16_MAX || h > INT16_MAX)
    return NULL;

  form_doc_t *doc = (form_doc_t *)calloc(1, sizeof(form_doc_t));
  if (!doc)
    return NULL;

  // Initialize document fields
  doc->form_size.w = w;
  doc->form_size.h = h;
  doc->flags = 0;
  doc->modified = false;
  doc->next_id = CTRL_ID_BASE;
  doc->grid_size = 8;
  doc->show_grid = true;
  doc->snap_to_grid = true;

  // Set default form_id if provided
  if (form_id && *form_id) {
    snprintf(doc->form_id, sizeof(doc->form_id), "%s", form_id);
  }
  
  // Initialize database context
  doc->database_name[0] = '\0';
  doc->table_name[0] = '\0';

  // Auto-layout defaults
  if (fe_default_auto_layout_enabled()) {
    doc->flags |= WINDOW_AUTO_LAYOUT;
    doc->layout_mode = 1;
  } else {
    doc->layout_mode = 0;
  }
  doc->flags &= ~WINDOW_STACK_HORIZONTAL;
  doc->layout_columns = 0;
  doc->layout_spacing = 4;
  doc->padding = (irect16_t){0, 0, 0, 0};
  doc->margin = (irect16_t){0, 0, 0, 0};

  return doc;
}

void fe_doc_destroy(form_doc_t *doc) {
  if (!doc)
    return;

  // Note: UI window destruction is handled by caller (fe_project layer).
  // This function only frees the document data structure itself.
  free(doc);
}

void fe_doc_mark_modified(form_doc_t *doc) {
  if (!doc)
    return;

  if (!doc->modified) {
    doc->modified = true;
    fe_doc_update_title(doc);
    fe_notify(FE_EVENT_DOCUMENT_MODIFIED, doc);
  }
}

void fe_doc_update_title(form_doc_t *doc) {
  if (!doc || !doc->doc_win)
    return;

  const char *name = doc->form_title[0] ? doc->form_title :
                     (doc->form_id[0] ? doc->form_id : "Untitled");
  const char *slash = strrchr(name, '/');
  if (slash)
    name = slash + 1;

  snprintf(doc->doc_win->title, sizeof(doc->doc_win->title), "%s%s",
           name, doc->modified ? " *" : "");
  invalidate_window(doc->doc_win);
}

// ============================================================
// Element Mutation
// ============================================================

int fe_doc_add_element(form_doc_t *doc, int type, irect16_t frame, uint32_t parent_id) {
  if (!doc || doc->element_count >= MAX_ELEMENTS)
    return -1;

  int idx = doc->element_count++;
  form_element_t *el = &doc->elements[idx];
  memset(el, 0, sizeof(*el));

  el->type = type;
  el->id = doc->next_id++;
  el->parent = parent_id;
  el->frame = frame;
  el->h_align = LAYOUT_ALIGN_STRETCH;
  el->v_align = LAYOUT_ALIGN_STRETCH;
  el->color = brTextNormal;
  
  // Initialize database fields
  el->db_field[0] = '\0';
  el->db_source[0] = '\0';
  el->db_display[0] = '\0';
  el->db_value[0] = '\0';

  // Generate default name
  const fe_component_desc_t *desc = fe_component_by_id(type);
  if (desc && desc->name_prefix) {
    int counter = doc->type_counters[type]++;
    snprintf(el->name, sizeof(el->name), "%s%d", desc->name_prefix, counter + 1);
  } else {
    snprintf(el->name, sizeof(el->name), "control%d", idx + 1);
  }

  // Generate ID expression
  const char *class_name = desc ? desc->class_name : "control";
  fe_doc_make_control_id_expr(el->id_expr, sizeof(el->id_expr),
                               doc->form_id, el->name, class_name, idx + 1);

  fe_doc_mark_modified(doc);
  return idx;
}

bool fe_doc_delete_element(form_doc_t *doc, int idx) {
  if (!doc || idx < 0 || idx >= doc->element_count)
    return false;

  // Shift elements down
  for (int i = idx; i < doc->element_count - 1; i++) {
    doc->elements[i] = doc->elements[i + 1];
  }
  doc->element_count--;

  fe_doc_mark_modified(doc);
  return true;
}

bool fe_doc_set_element_text(form_doc_t *doc, int idx, const char *text) {
  if (!doc || idx < 0 || idx >= doc->element_count || !text)
    return false;

  form_element_t *el = &doc->elements[idx];
  snprintf(el->text, sizeof(el->text), "%s", text);
  fe_doc_mark_modified(doc);
  return true;
}

bool fe_doc_set_element_frame(form_doc_t *doc, int idx, irect16_t frame) {
  if (!doc || idx < 0 || idx >= doc->element_count)
    return false;

  doc->elements[idx].frame = frame;
  fe_doc_mark_modified(doc);
  return true;
}

bool fe_doc_set_element_name(form_doc_t *doc, int idx, const char *name) {
  if (!doc || idx < 0 || idx >= doc->element_count || !name)
    return false;

  form_element_t *el = &doc->elements[idx];
  snprintf(el->name, sizeof(el->name), "%s", name);
  fe_doc_mark_modified(doc);
  return true;
}

bool fe_doc_set_element_align(form_doc_t *doc, int idx, uint8_t h_align, uint8_t v_align) {
  if (!doc || idx < 0 || idx >= doc->element_count)
    return false;

  form_element_t *el = &doc->elements[idx];
  el->h_align = h_align;
  el->v_align = v_align;
  fe_doc_mark_modified(doc);
  return true;
}

bool fe_doc_set_element_font(form_doc_t *doc, int idx, uint8_t font) {
  if (!doc || idx < 0 || idx >= doc->element_count)
    return false;

  form_element_t *el = &doc->elements[idx];
  el->font = font;
  el->font_set = true;
  fe_doc_mark_modified(doc);
  return true;
}

bool fe_doc_set_element_color(form_doc_t *doc, int idx, uint8_t color) {
  if (!doc || idx < 0 || idx >= doc->element_count)
    return false;

  form_element_t *el = &doc->elements[idx];
  el->color = color;
  el->color_set = true;
  fe_doc_mark_modified(doc);
  return true;
}

// ============================================================
// Database Binding Setters
// ============================================================

bool fe_doc_set_element_db_field(form_doc_t *doc, int idx, const char *field) {
  if (!doc || idx < 0 || idx >= doc->element_count || !field)
    return false;

  form_element_t *el = &doc->elements[idx];
  snprintf(el->db_field, sizeof(el->db_field), "%s", field);
  fe_doc_mark_modified(doc);
  return true;
}

bool fe_doc_set_element_db_source(form_doc_t *doc, int idx, const char *source) {
  if (!doc || idx < 0 || idx >= doc->element_count || !source)
    return false;

  form_element_t *el = &doc->elements[idx];
  snprintf(el->db_source, sizeof(el->db_source), "%s", source);
  fe_doc_mark_modified(doc);
  return true;
}

bool fe_doc_set_element_db_display(form_doc_t *doc, int idx, const char *display) {
  if (!doc || idx < 0 || idx >= doc->element_count || !display)
    return false;

  form_element_t *el = &doc->elements[idx];
  snprintf(el->db_display, sizeof(el->db_display), "%s", display);
  fe_doc_mark_modified(doc);
  return true;
}

bool fe_doc_set_element_db_value(form_doc_t *doc, int idx, const char *value) {
  if (!doc || idx < 0 || idx >= doc->element_count || !value)
    return false;

  form_element_t *el = &doc->elements[idx];
  snprintf(el->db_value, sizeof(el->db_value), "%s", value);
  fe_doc_mark_modified(doc);
  return true;
}

// ============================================================
// Element Query
// ============================================================

form_element_t *fe_doc_find_element(form_doc_t *doc, uint32_t id) {
  if (!doc)
    return NULL;

  for (int i = 0; i < doc->element_count; i++) {
    if (doc->elements[i].id == id)
      return &doc->elements[i];
  }
  return NULL;
}

int fe_doc_find_element_index(form_doc_t *doc, uint32_t id) {
  if (!doc)
    return -1;

  for (int i = 0; i < doc->element_count; i++) {
    if (doc->elements[i].id == id)
      return i;
  }
  return -1;
}

form_element_t *fe_doc_get_element(form_doc_t *doc, int idx) {
  if (!doc || idx < 0 || idx >= doc->element_count)
    return NULL;
  return &doc->elements[idx];
}

int fe_doc_element_count(const form_doc_t *doc) {
  return doc ? doc->element_count : 0;
}

// ============================================================
// ID Generation
// ============================================================

int fe_doc_resolve_control_id(form_doc_t *doc, const char *expr) {
  int id = 0;
  if (parse_numeric_expr(expr, &id))
    return id;
  return doc ? doc->next_id++ : CTRL_ID_BASE;
}

void fe_doc_make_control_id_expr(char *out, size_t out_sz,
                                  const char *form_id,
                                  const char *name,
                                  const char *class_name,
                                  int ordinal) {
  char form_buf[96];
  char name_buf[96];
  size_t i = 0;

  if (!out || out_sz == 0)
    return;

  // Sanitize form_id
  for (const char *p = form_id && *form_id ? form_id : "form";
       *p && i + 1 < sizeof(form_buf); p++) {
    char c = (*p >= 'a' && *p <= 'z') ? (char)(*p - 'a' + 'A') : *p;
    form_buf[i++] = (isalnum((unsigned char)c) || c == '_') ? c : '_';
  }
  if (i == 0)
    form_buf[i++] = 'F';
  form_buf[i] = '\0';
  upper_ident(form_buf);

  // Sanitize name
  i = 0;
  for (const char *p = name && *name ? name : (class_name && *class_name ? class_name : "control");
       *p && i + 1 < sizeof(name_buf); p++) {
    char c = (*p >= 'a' && *p <= 'z') ? (char)(*p - 'a' + 'A') : *p;
    name_buf[i++] = (isalnum((unsigned char)c) || c == '_') ? c : '_';
  }
  if (i == 0) {
    snprintf(name_buf, sizeof(name_buf), "CONTROL%d", ordinal);
    upper_ident(name_buf);
  } else {
    name_buf[i] = '\0';
    upper_ident(name_buf);
  }

  snprintf(out, out_sz, "ID_%s_%s", form_buf, name_buf);
}
