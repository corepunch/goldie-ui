// Document helper stubs during window-first migration.
// Editor interactions mutate live windows; XML is used here for load-time
// metadata lookup while the old form_element_t arrays are retired.

#include "formeditor.h"

static int fe_xml_attr_int(xmlNodePtr node, const char *name, int fallback) {
  xmlChar *v = node ? xmlGetProp(node, BAD_CAST name) : NULL;
  if (!v)
    return fallback;
  char *end = NULL;
  long n = strtol((const char *)v, &end, 0);
  bool ok = end && *end == '\0';
  xmlFree(v);
  return ok ? (int)n : fallback;
}

static bool fe_xml_elem(xmlNodePtr node, const char *name) {
  return node && node->type == XML_ELEMENT_NODE &&
         xmlStrcasecmp(node->name, BAD_CAST name) == 0;
}

static xmlNodePtr fe_find_runtime_node_by_id(xmlNodePtr parent,
                                             uint32_t target_id,
                                             uint32_t *next_id) {
  if (!parent || !next_id)
    return NULL;

  for (xmlNodePtr node = parent->children; node; node = node->next) {
    if (node->type != XML_ELEMENT_NODE)
      continue;

    xmlNodePtr found = fe_find_runtime_node_by_id(node, target_id, next_id);
    if (found)
      return found;

    uint32_t id = (uint32_t)fe_xml_attr_int(node, "id", 0);
    if (!id)
      id = (*next_id)++;
    if (id == target_id)
      return node;
  }

  return NULL;
}

typedef struct {
  window_t *column;
  uint32_t field_id;
  const char *title;
} fe_set_column_binding_command_t;

static int fe_report_column_index(window_t *column) {
  if (!column || !column->parent)
    return -1;
  int index = 0;
  for (window_t *child = column->parent->children; child; child = child->next) {
    if (child->proc != win_reportcolumn)
      continue;
    if (child == column)
      return index;
    index++;
  }
  return -1;
}

static bool fe_doc_apply_set_column_binding(window_t *doc,
                                            const fe_set_column_binding_command_t *cmd) {
  if (!doc || !cmd || !cmd->column || !cmd->column->parent || !cmd->field_id)
    return false;

  int column_index = fe_report_column_index(cmd->column);
  if (column_index < 0)
    return false;

  tableview_column_binding_t binding = {
    .field_id = cmd->field_id,
    .title = cmd->title,
  };
  if (!send_message(cmd->column->parent, tvSetColumnBinding, (uint32_t)column_index, &binding))
    return false;

  fe_doc_mark_modified(doc);
  if (g_app)
    g_app->project.modified = true;
  return true;
}

bool fe_doc_drop_create_component(int component_id,
                                  window_t *parent_target) {
  window_t *doc = parent_target ? get_root_window(parent_target) : NULL;
  const fe_component_desc_t *desc = fe_component_at(component_id);
  if (!doc || !desc || !desc->class_name || !desc->proc) {
    fprintf(stderr, "fe_doc_drop_create_component: invalid component_id %d\n", component_id);
    return false;
  }
  if ((desc->capabilities & FE_COMPONENT_PLACEABLE) == 0) {
    fprintf(stderr, "fe_doc_drop_create_component: component '%s' is not placeable\n", desc->class_name);
    return false;
  }
  int w = desc->default_layout_size.w > 0 ? desc->default_layout_size.w : 96;
  int h = desc->default_layout_size.h > 0 ? desc->default_layout_size.h : 24;
  window_t *child = create_window(
      desc->class_name,
      0,
      MAKERECT(0, 0, w, h),
      parent_target,
      desc->class_name,
      0,
      NULL);
  if (!child) {
    fprintf(stderr, "fe_doc_drop_create_component: failed to create component '%s'\n", desc->class_name);
    return false;
  }

  window_layout_sync(doc);
  invalidate_window(doc);

  fe_doc_mark_modified(doc);
  if (g_app)
    g_app->project.modified = true;
  fe_notify(FE_EVENT_ELEMENT_ADDED, doc);
  return true;
}

bool fe_doc_bind_database_field_to_column(window_t *doc,
                                          const ui_drag_item_payload_t *payload,
                                          window_t *target,
                                          char *error, size_t error_sz) {
  if (!doc || !payload || !target || target->proc != win_reportcolumn) {
    fe_error_set(error, error_sz, "Drop database fields onto a TableView column.");
    return false;
  }

  xmlNodePtr form_node = (xmlNodePtr)doc->userdata2;
  if (!fe_xml_elem(form_node, "form")) {
    fe_error_set(error, error_sz, "This document does not have editable form XML.");
    return false;
  }

  uint32_t next_id = CTRL_ID_BASE;
  xmlNodePtr column_node = fe_find_runtime_node_by_id(form_node, target->id, &next_id);
  if (!fe_xml_elem(column_node, "Column") || !fe_xml_elem(column_node->parent, "TableView")) {
    fe_error_set(error, error_sz, "Drop database fields onto a TableView column.");
    return false;
  }

  char field_expr[128];
  char title[128];
  uint32_t field_id = 0;
  if (!fe_resolve_table_column_database_field(column_node->parent, payload,
                                              field_expr, sizeof(field_expr),
                                              title, sizeof(title),
                                              &field_id,
                                              error, error_sz))
    return false;

  fe_set_column_binding_command_t cmd = {
    .column = target,
    .field_id = field_id,
    .title = title,
  };
  return fe_doc_apply_set_column_binding(doc, &cmd);
}

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
