#include "formeditor.h"

void fe_error_set(char *error, size_t error_sz, const char *message) {
  if (!error || error_sz == 0)
    return;
  snprintf(error, error_sz, "%s", message ? message : "");
}

static char *fe_xml_attr_dup(xmlNodePtr node, const char *name) {
  xmlChar *v = node ? xmlGetProp(node, BAD_CAST name) : NULL;
  if (!v)
    return NULL;
  char *out = strdup((const char *)v);
  xmlFree(v);
  return out;
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

static void fe_db_name_from_source(const char *source, char *out, size_t out_sz) {
  if (!out || out_sz == 0)
    return;
  out[0] = '\0';
  if (!source || !*source)
    return;
  const char *dot = strchr(source, '.');
  if (!dot)
    return;
  snprintf(out, out_sz, "%.*s", (int)(dot - source), source);
}

static database_t *fe_project_database_by_name(const char *name) {
  if (!g_app || !name || !*name)
    return NULL;
  for (int i = 0; i < g_app->project.database_count; i++) {
    database_t *db = g_app->project.databases[i];
    if (db && db->name && strcmp(db->name, name) == 0)
      return db;
  }
  return NULL;
}

static database_t *fe_database_for_tableview_node(xmlNodePtr table_node) {
  char *source = fe_xml_attr_dup(table_node, "source");
  if (!source)
    source = fe_xml_attr_dup(table_node, "database");

  char db_name[128];
  fe_db_name_from_source(source, db_name, sizeof(db_name));
  free(source);

  database_t *db = fe_project_database_by_name(db_name);
  if (!db && db_name[0])
    db = get_database_by_name(db_name);
  if (!db)
    db = ui_get_database();
  if (!db)
    db = get_database_by_name("db");
  return db;
}

static database_t *fe_payload_database(const ui_drag_item_payload_t *payload) {
  if (payload && payload->source_name[0])
    return fe_project_database_by_name(payload->source_name);
  database_t *db = ui_get_database();
  if (!db)
    db = get_database_by_name("db");
  return db;
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
                                  char *out, size_t out_sz,
                                  uint32_t *field_id,
                                  char *error, size_t error_sz) {
  if (!db || !target_table || !payload || !out || out_sz == 0)
    return false;
  out[0] = '\0';
  if (field_id)
    *field_id = 0;

  const db_schema_def_t *schema = (const db_schema_def_t *)send_db_message(db, dbGetSchema, 0, NULL);
  const db_table_schema_t *payload_table = fe_schema_table_by_id(schema, payload->item_class);
  const db_field_schema_t *payload_field = fe_table_field_by_id(payload_table, payload->item_id);
  if (!payload_table || !payload_field || !payload_field->name) {
    fe_error_set(error, error_sz, "That database field is not part of this database schema.");
    return false;
  }

  if (payload_table->table_id == target_table->table_id) {
    snprintf(out, out_sz, "%s", payload_field->name);
    if (field_id)
      *field_id = payload_field->field_id;
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
  if (matches != 1 || !match || !match->name) {
    fe_error_set(error, error_sz,
                 "That field is not on the TableView source table or one of its joined tables.");
    return false;
  }

  snprintf(out, out_sz, "%s.%s", match->name, payload_field->name);
  if (field_id)
    *field_id = (match->foreign_field_id == payload_field->field_id && match->local_field_id)
                  ? match->local_field_id
                  : payload_field->field_id;
  return true;
}

bool fe_resolve_table_column_database_field(xmlNodePtr table_node,
                                            const ui_drag_item_payload_t *payload,
                                            char *field_expr, size_t field_expr_sz,
                                            char *title, size_t title_sz,
                                            uint32_t *field_id,
                                            char *error, size_t error_sz) {
  if (!table_node || !payload || !field_expr || field_expr_sz == 0) {
    fe_error_set(error, error_sz, "Drop database fields onto a TableView column.");
    return false;
  }

  field_expr[0] = '\0';
  if (title && title_sz > 0)
    title[0] = '\0';

  database_t *db = fe_database_for_tableview_node(table_node);
  database_t *payload_db = fe_payload_database(payload);
  if (payload->source_name[0] && !payload_db) {
    fe_error_set(error, error_sz, "That database field comes from an unavailable database.");
    return false;
  }
  if (payload_db && db && payload_db != db) {
    fe_error_set(error, error_sz,
                 "That field belongs to a different database than this TableView.");
    return false;
  }
  if (!db)
    db = payload_db;

  const db_table_schema_t *target_table = fe_table_for_tableview_node(db, table_node);
  if (!target_table) {
    fe_error_set(error, error_sz, "The TableView source table could not be found.");
    return false;
  }

  if (!fe_resolve_field_expr(db, target_table, payload,
                             field_expr, field_expr_sz, field_id, error, error_sz))
    return false;

  fe_pretty_field_title(field_expr, title, title_sz);
  return true;
}
