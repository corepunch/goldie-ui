#include "formeditor.h"

void fe_error_set(char *error, size_t error_sz, const char *message) {
  if (error && error_sz) snprintf(error, error_sz, "%s", message ? message : "");
}

static void xml_attr_copy(xmlNodePtr node, const char *name, char *out, size_t out_sz) {
  if (!out || !out_sz) return;
  out[0] = '\0';
  xmlChar *value = node ? xmlGetProp(node, BAD_CAST name) : NULL;
  if (value) {
    snprintf(out, out_sz, "%s", (const char *)value);
    xmlFree(value);
  }
}

static database_t *project_database(const char *name) {
  if (!g_app || !name || !*name) return NULL;
  for (int i = 0; i < g_app->project.database_count; i++) {
    database_t *db = g_app->project.databases[i];
    if (db && db->name && strcmp(db->name, name) == 0) return db;
  }
  return get_database_by_name(name);
}

static const db_table_schema_t *schema_table(const db_schema_def_t *schema, const char *name) {
  if (!schema || !name || !*name) return NULL;
  for (int i = 0; i < schema->table_count; i++)
    if (schema->tables[i].name && strcmp(schema->tables[i].name, name) == 0)
      return &schema->tables[i];
  return NULL;
}

static bool table_has_field(const db_table_schema_t *table, const char *name) {
  if (!table || !name) return false;
  for (int i = 0; i < table->field_count; i++)
    if (table->fields[i].name && strcmp(table->fields[i].name, name) == 0) return true;
  return false;
}

static void pretty_title(const char *field, char *out, size_t out_sz) {
  if (!out || !out_sz) return;
  size_t n = 0;
  bool cap = true;
  for (const char *p = field ? field : ""; *p && n + 1 < out_sz; p++) {
    if (*p == '.' || *p == '_' || *p == '-') {
      if (n && out[n - 1] != ' ') out[n++] = ' ';
      cap = true;
    } else {
      char c = *p;
      if (cap && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
      out[n++] = c;
      cap = false;
    }
  }
  out[n] = '\0';
}

bool fe_resolve_table_column_database_field(xmlNodePtr table_node,
    const fe_database_field_ref_t *field, char *field_expr, size_t field_expr_sz,
    char *title, size_t title_sz, char *error, size_t error_sz) {
  if (!table_node || !field || !field_expr || !field_expr_sz) {
    fe_error_set(error, error_sz, "Drop database fields onto a TableView column.");
    return false;
  }

  char source[192];
  xml_attr_copy(table_node, "source", source, sizeof(source));
  if (!source[0]) xml_attr_copy(table_node, "db_source", source, sizeof(source));

  char database_name[64] = {0}, table_name[64] = {0};
  const char *dot = strchr(source, '.');
  if (dot) {
    snprintf(database_name, sizeof(database_name), "%.*s", (int)(dot - source), source);
    snprintf(table_name, sizeof(table_name), "%s", dot + 1);
    char *next = strchr(table_name, '.');
    if (next) *next = '\0';
  } else {
    snprintf(database_name, sizeof(database_name), "%s", field->database);
    snprintf(table_name, sizeof(table_name), "%s", source);
  }

  if (database_name[0] && strcmp(database_name, field->database) != 0) {
    fe_error_set(error, error_sz, "That field belongs to a different database than this TableView.");
    return false;
  }
  database_t *db = project_database(field->database);
  const db_schema_def_t *schema = db
      ? (const db_schema_def_t *)send_db_message(db, dbGetSchema, 0, NULL) : NULL;
  const db_table_schema_t *base = schema_table(schema, table_name);
  const db_table_schema_t *from = schema_table(schema, field->table);
  if (!base || !from || !table_has_field(from, field->field)) {
    fe_error_set(error, error_sz, "That database field is not part of the TableView schema.");
    return false;
  }

  if (base == from) {
    snprintf(field_expr, field_expr_sz, "%s", field->field);
  } else {
    const char *alias = NULL;
    for (int i = 0; i < base->join_count; i++) {
      const db_join_schema_t *join = &base->joins[i];
      if (join->foreign_table && strcmp(join->foreign_table, from->name) == 0) {
        if (alias) {
          fe_error_set(error, error_sz, "This table has multiple joins to that field's table.");
          return false;
        }
        alias = join->name;
      }
    }
    if (!alias || !*alias) {
      fe_error_set(error, error_sz,
                   "That field is not on the TableView source table or one of its joined tables.");
      return false;
    }
    snprintf(field_expr, field_expr_sz, "%s.%s", alias, field->field);
  }
  pretty_title(field->field, title, title_sz);
  return true;
}
