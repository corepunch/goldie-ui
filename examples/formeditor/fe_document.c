// Document helper stubs during window-first migration.
// The runtime source of truth is the form XML node + live windows, not
// form_element_t arrays.

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

static char *fe_xml_attr_dup(xmlNodePtr node, const char *name) {
  xmlChar *v = node ? xmlGetProp(node, BAD_CAST name) : NULL;
  if (!v)
    return NULL;
  char *out = strdup((const char *)v);
  xmlFree(v);
  return out;
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

static const db_table_schema_t *fe_schema_table_by_id(const db_schema_def_t *schema,
                                                       uint32_t table_id) {
  if (!schema || !table_id)
    return NULL;
  for (int i = 0; i < schema->table_count; i++) {
    if (schema->tables[i].table_id == table_id)
      return &schema->tables[i];
  }
  return NULL;
}

static const db_table_schema_t *fe_schema_table_by_name(const db_schema_def_t *schema,
                                                         const char *name) {
  if (!schema || !name || !*name)
    return NULL;
  for (int i = 0; i < schema->table_count; i++) {
    if (schema->tables[i].name && strcmp(schema->tables[i].name, name) == 0)
      return &schema->tables[i];
  }
  return NULL;
}

static const db_field_schema_t *fe_table_field_by_id(const db_table_schema_t *table,
                                                      uint32_t field_id) {
  if (!table || !field_id)
    return NULL;
  for (int i = 0; i < table->field_count; i++) {
    if (table->fields[i].field_id == field_id)
      return &table->fields[i];
  }
  return NULL;
}

static void fe_table_name_from_source(const char *source, char *out, size_t out_sz) {
  if (!out || out_sz == 0)
    return;
  out[0] = '\0';
  if (!source || !*source)
    return;
  const char *table = strchr(source, '.');
  table = table ? table + 1 : source;
  snprintf(out, out_sz, "%s", table);
  char *next = strchr(out, '.');
  if (next)
    *next = '\0';
}

static const db_table_schema_t *fe_table_for_tableview_node(database_t *db,
                                                            xmlNodePtr table_node) {
  const db_schema_def_t *schema = db
      ? (const db_schema_def_t *)send_db_message(db, dbGetSchema, 0, NULL)
      : NULL;
  if (!schema || !table_node)
    return NULL;

  char *source = fe_xml_attr_dup(table_node, "source");
  if (!source)
    source = fe_xml_attr_dup(table_node, "database");
  char table_name[128];
  fe_table_name_from_source(source, table_name, sizeof(table_name));
  free(source);
  return fe_schema_table_by_name(schema, table_name);
}

static void fe_pretty_field_title(const char *expr, char *out, size_t out_sz) {
  if (!out || out_sz == 0)
    return;
  out[0] = '\0';
  bool cap_next = true;
  size_t n = 0;
  for (const char *p = expr ? expr : ""; *p && n + 1 < out_sz; p++) {
    if (*p == '.' || *p == '_' || *p == '-') {
      if (n > 0 && out[n - 1] != ' ')
        out[n++] = ' ';
      cap_next = true;
      continue;
    }
    char c = *p;
    if (cap_next && c >= 'a' && c <= 'z')
      c = (char)(c - 'a' + 'A');
    out[n++] = c;
    cap_next = false;
  }
  out[n] = '\0';
}

static bool fe_resolve_field_expr(database_t *db,
                                  const db_table_schema_t *target_table,
                                  const ui_drag_item_payload_t *payload,
                                  char *out, size_t out_sz) {
  if (!db || !target_table || !payload || !out || out_sz == 0)
    return false;
  out[0] = '\0';

  const db_schema_def_t *schema = (const db_schema_def_t *)send_db_message(db, dbGetSchema, 0, NULL);
  const db_table_schema_t *payload_table = fe_schema_table_by_id(schema, payload->item_class);
  const db_field_schema_t *payload_field = fe_table_field_by_id(payload_table, payload->item_id);
  if (!payload_table || !payload_field || !payload_field->name)
    return false;

  if (payload_table->table_id == target_table->table_id) {
    snprintf(out, out_sz, "%s", payload_field->name);
    return true;
  }

  const db_join_schema_t *match = NULL;
  int matches = 0;
  for (int i = 0; i < target_table->join_count; i++) {
    const db_join_schema_t *join = &target_table->joins[i];
    if (join->foreign_table_id == payload_table->table_id) {
      match = join;
      matches++;
    }
  }
  if (matches != 1 || !match || !match->name)
    return false;

  snprintf(out, out_sz, "%s.%s", match->name, payload_field->name);
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
                                          window_t *target) {
  if (!doc || !payload || !target || target->proc != win_reportcolumn)
    return false;

  xmlNodePtr form_node = (xmlNodePtr)doc->userdata2;
  if (!fe_xml_elem(form_node, "form"))
    return false;

  uint32_t next_id = CTRL_ID_BASE;
  xmlNodePtr column_node = fe_find_runtime_node_by_id(form_node, target->id, &next_id);
  if (!fe_xml_elem(column_node, "Column") || !fe_xml_elem(column_node->parent, "TableView"))
    return false;

  database_t *db = ui_get_database();
  if (!db)
    db = get_database_by_name("db");
  const db_table_schema_t *target_table = fe_table_for_tableview_node(db, column_node->parent);
  char field_expr[128];
  if (!fe_resolve_field_expr(db, target_table, payload, field_expr, sizeof(field_expr)))
    return false;

  char title[128];
  fe_pretty_field_title(field_expr, title, sizeof(title));
  xmlSetProp(column_node, BAD_CAST "field", BAD_CAST field_expr);
  if (title[0])
    xmlSetProp(column_node, BAD_CAST "title", BAD_CAST title);

  canvas_rebuild_live_controls(doc);
  fe_doc_mark_modified(doc);
  if (g_app)
    g_app->project.modified = true;
  return true;
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
