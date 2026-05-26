#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "user.h"
#include "database.h"

// ── Database class registry (analogous to window class registry) ────────────

#define MAX_DATABASE_CLASSES 16

static db_class_desc_t g_database_classes[MAX_DATABASE_CLASSES];
static int g_database_class_count = 0;

static bool db_streq(const char *a, const char *b) {
  return a && b && strcmp(a, b) == 0;
}

bool register_database_class(const db_class_desc_t *desc) {
  if (!desc || !desc->class_name || !*desc->class_name || !desc->proc)
    return false;
  
  // Check if already registered (idempotent)
  for (int i = 0; i < g_database_class_count; i++) {
    if (db_streq(g_database_classes[i].class_name, desc->class_name))
      return true;
  }
  
  if (g_database_class_count >= MAX_DATABASE_CLASSES)
    return false;
  
  g_database_classes[g_database_class_count++] = *desc;
  return true;
}

dbproc_t find_database_class_proc(const char *class_name) {
  if (!class_name || !*class_name) return NULL;
  for (int i = 0; i < g_database_class_count; i++) {
    if (db_streq(g_database_classes[i].class_name, class_name))
      return g_database_classes[i].proc;
  }
  return NULL;
}

// ── Database instance registry ──────────────────────────────────────────────

#define MAX_DATABASE_INSTANCES 16

static struct {
  const char *name;
  database_t *db;
} g_database_instances[MAX_DATABASE_INSTANCES];
static int g_database_instance_count = 0;

bool register_database(const char *name, database_t *db) {
  if (!name || !*name || !db) return false;
  
  // Check if already registered (allow overwrite)
  for (int i = 0; i < g_database_instance_count; i++) {
    if (db_streq(g_database_instances[i].name, name)) {
      g_database_instances[i].db = db;
      return true;
    }
  }
  
  if (g_database_instance_count >= MAX_DATABASE_INSTANCES) {
    fprintf(stderr, "Too many database instances registered (max %d)\n",
            MAX_DATABASE_INSTANCES);
    return false;
  }
  
  g_database_instances[g_database_instance_count].name = name;
  g_database_instances[g_database_instance_count].db = db;
  g_database_instance_count++;
  return true;
}

database_t *get_database_by_name(const char *name) {
  if (!name || !*name) return NULL;
  for (int i = 0; i < g_database_instance_count; i++) {
    if (db_streq(g_database_instances[i].name, name))
      return g_database_instances[i].db;
  }
  fprintf(stderr, "Database '%s' not found in registry\n", name);
  return NULL;
}

database_t *create_database(const char *name, const char *class_name, const char *source_path) {
  if (!name || !class_name) return NULL;
  
  dbproc_t proc = find_database_class_proc(class_name);
  if (!proc) {
    fprintf(stderr, "Database class '%s' not registered\n", class_name);
    return NULL;
  }
  
  database_t *db = calloc(1, sizeof(database_t));
  if (!db) return NULL;
  
  db->name = name;
  db->class_name = class_name;
  db->proc = proc;
  db->dirty = false;
  
  if (source_path)
    strncpy(db->source_path, source_path, sizeof(db->source_path) - 1);
  
  // Send dbCreate message (proc can allocate userdata, parse source_path, etc.)
  send_db_message(db, dbCreate, 0, (void *)source_path);
  
  // Send dbLoad message to load initial data
  send_db_message(db, dbLoad, 0, NULL);
  
  return db;
}

void destroy_database(database_t *db) {
  if (!db) return;
  
  // Save if dirty
  if (db->dirty)
    send_db_message(db, dbSave, 0, NULL);
  
  // Send destroy message (proc can free userdata)
  send_db_message(db, dbDestroy, 0, NULL);
  
  free(db);
}

lresult_t send_db_message(database_t *db, uint32_t msg, uint32_t wparam, void *lparam) {
  if (!db || !db->proc) return 0;
  return db->proc(db, msg, wparam, lparam);
}

// ── Declarative database API helpers (from .orion metadata) ─────────────────

const db_source_def_t *db_api_find_source(const db_api_def_t *api, uint32_t source_id) {
  if (!api || !api->sources || api->source_count <= 0 || source_id == 0) return NULL;
  for (int i = 0; i < api->source_count; i++) {
    if (api->sources[i].source_id == source_id)
      return &api->sources[i];
  }
  return NULL;
}

const db_view_binding_t *db_api_find_binding(const db_api_def_t *api, uint32_t binding_id) {
  if (!api || !api->bindings || api->binding_count <= 0 || binding_id == 0) return NULL;
  for (int i = 0; i < api->binding_count; i++) {
    if (api->bindings[i].binding_id == binding_id)
      return &api->bindings[i];
  }
  return NULL;
}

const db_view_binding_t *db_api_find_binding_for_view(const db_api_def_t *api, uint32_t view_id) {
  if (!api || !api->bindings || api->binding_count <= 0 || view_id == 0) return NULL;
  for (int i = 0; i < api->binding_count; i++) {
    if (api->bindings[i].view_id == view_id)
      return &api->bindings[i];
  }
  return NULL;
}

const db_action_def_t *db_api_find_action(const db_api_def_t *api, uint32_t action_id) {
  if (!api || !api->actions || api->action_count <= 0 || action_id == 0) return NULL;
  for (int i = 0; i < api->action_count; i++) {
    if (api->actions[i].action_id == action_id)
      return &api->actions[i];
  }
  return NULL;
}

const db_outlet_def_t *db_api_find_outlet(const db_api_def_t *api, uint32_t outlet_id) {
  if (!api || !api->outlets || api->outlet_count <= 0 || outlet_id == 0) return NULL;
  for (int i = 0; i < api->outlet_count; i++) {
    if (api->outlets[i].outlet_id == outlet_id)
      return &api->outlets[i];
  }
  return NULL;
}

const db_table_schema_t *db_schema_find_table_by_id(const db_schema_def_t *schema, uint32_t table_id) {
  if (!schema || !schema->tables || schema->table_count <= 0 || table_id == 0)
    return NULL;
  for (int i = 0; i < schema->table_count; i++) {
    if (schema->tables[i].table_id == table_id)
      return &schema->tables[i];
  }
  return NULL;
}

const db_table_schema_t *db_schema_find_table_by_name(const db_schema_def_t *schema, const char *name) {
  if (!schema || !schema->tables || schema->table_count <= 0 || !name || !*name)
    return NULL;
  for (int i = 0; i < schema->table_count; i++) {
    if (schema->tables[i].name && strcmp(schema->tables[i].name, name) == 0)
      return &schema->tables[i];
  }
  return NULL;
}

const db_field_schema_t *db_table_find_field_by_id(const db_table_schema_t *table, uint32_t field_id) {
  if (!table || !table->fields || table->field_count <= 0 || field_id == 0)
    return NULL;
  for (int i = 0; i < table->field_count; i++) {
    if (table->fields[i].field_id == field_id)
      return &table->fields[i];
  }
  return NULL;
}

const db_field_schema_t *db_table_find_field_by_name(const db_table_schema_t *table, const char *name) {
  if (!table || !table->fields || table->field_count <= 0 || !name || !*name)
    return NULL;
  for (int i = 0; i < table->field_count; i++) {
    if (table->fields[i].name && strcmp(table->fields[i].name, name) == 0)
      return &table->fields[i];
  }
  return NULL;
}

bool db_schema_resolve_many_relationship(const db_schema_def_t *schema,
                                         const db_table_schema_t *master_table,
                                         const char *relation_name,
                                         const db_table_schema_t **detail_table_out,
                                         const db_field_schema_t **filter_field_out) {
  if (detail_table_out)
    *detail_table_out = NULL;
  if (filter_field_out)
    *filter_field_out = NULL;
  if (!schema || !master_table || !relation_name || !*relation_name)
    return false;

  const db_field_schema_t *relation = db_table_find_field_by_name(master_table, relation_name);
  if (!relation || !relation->relation_many || relation->relation_table_id == 0)
    return false;

  const db_table_schema_t *detail_table =
    db_schema_find_table_by_id(schema, relation->relation_table_id);
  if (!detail_table)
    return false;

  const db_field_schema_t *match = NULL;
  int matches = 0;
  for (int i = 0; i < detail_table->field_count; i++) {
    const db_field_schema_t *field = &detail_table->fields[i];
    if (field->relation_many)
      continue;
    if (field->relation_table_id == master_table->table_id) {
      match = field;
      matches++;
    }
  }
  if (matches != 1 || !match)
    return false;

  if (detail_table_out)
    *detail_table_out = detail_table;
  if (filter_field_out)
    *filter_field_out = match;
  return true;
}

static const db_field_meta_t *db_find_field_meta(const db_field_meta_t *fields, int field_count, uint32_t field_id) {
  if (!fields || field_count <= 0 || field_id == 0)
    return NULL;
  for (int i = 0; i < field_count; i++) {
    if (fields[i].field_id == field_id)
      return &fields[i];
  }
  return NULL;
}

static bool db_resolve_joined_field(database_t *db, const db_table_schema_t *table,
                                    uint32_t field_id, const db_join_schema_t **join_out,
                                    const db_field_meta_t **foreign_field_out) {
  if (!db || !table || !table->joins || table->join_count <= 0 || field_id == 0 ||
      !join_out || !foreign_field_out)
    return false;

  const db_join_schema_t *match = NULL;
  const db_field_meta_t *match_field = NULL;
  for (int i = 0; i < table->join_count; i++) {
    int foreign_field_count = 0;
    const db_field_meta_t *foreign_fields = (const db_field_meta_t *)send_db_message(
      db, dbGetFieldMeta, table->joins[i].foreign_table_id, &foreign_field_count);
    const db_field_meta_t *foreign_field = db_find_field_meta(foreign_fields, foreign_field_count, field_id);
    if (!foreign_field)
      continue;
    if (match)
      return false;
    match = &table->joins[i];
    match_field = foreign_field;
  }

  if (!match || !match_field)
    return false;
  *join_out = match;
  *foreign_field_out = match_field;
  return true;
}

static bool db_format_field_value(const db_field_meta_t *field, const void *record, char *buf, size_t buf_sz) {
  if (!field || !record || !buf || buf_sz == 0)
    return false;
  const void *field_ptr = (const char *)record + field->offset;
  switch (field->type) {
    case DB_TYPE_INT:
      snprintf(buf, buf_sz, "%d", *(const int *)field_ptr);
      return true;
    case DB_TYPE_STRING:
      snprintf(buf, buf_sz, "%s", (const char *)field_ptr);
      return true;
    case DB_TYPE_BOOL:
      snprintf(buf, buf_sz, "%s", *(const bool *)field_ptr ? "true" : "false");
      return true;
    case DB_TYPE_FLOAT:
      snprintf(buf, buf_sz, "%g", *(const float *)field_ptr);
      return true;
    case DB_TYPE_DOUBLE:
      snprintf(buf, buf_sz, "%g", *(const double *)field_ptr);
      return true;
    default:
      return false;
  }
}

bool db_get_schema_field_text(database_t *db, uint32_t table_id, const void *record,
                              uint32_t field_id, char *buf, size_t buf_sz) {
  if (!db || table_id == 0 || !record || field_id == 0 || !buf || buf_sz == 0)
    return false;

  const db_schema_def_t *schema = (const db_schema_def_t *)send_db_message(db, dbGetSchema, 0, NULL);
  const db_table_schema_t *table = db_schema_find_table_by_id(schema, table_id);
  int field_count = 0;
  const db_field_meta_t *fields = (const db_field_meta_t *)send_db_message(db, dbGetFieldMeta, table_id, &field_count);
  if (!table || !fields || field_count <= 0)
    return false;

  const db_field_meta_t *field = db_find_field_meta(fields, field_count, field_id);
  if (field)
    return db_format_field_value(field, record, buf, buf_sz);

  const db_join_schema_t *join = NULL;
  const db_field_meta_t *foreign_field = NULL;
  if (!db_resolve_joined_field(db, table, field_id, &join, &foreign_field))
    return false;

  const db_field_meta_t *local_field = db_find_field_meta(fields, field_count, join->local_field_id);
  if (!local_field || local_field->type != DB_TYPE_INT)
    return false;

  const db_field_schema_t *local_schema = db_table_find_field_by_id(table, join->local_field_id);

  int relation_id = *(const int *)((const char *)record + local_field->offset);
  if (relation_id == 0)
    return false;

  void *foreign_record = (void *)send_db_message(db, dbFind,
    MAKEDWORD(join->foreign_table_id,
              local_schema && local_schema->relation_field_id ? local_schema->relation_field_id : 0),
    (void *)(intptr_t)relation_id);
  if (!foreign_record)
    return false;
  if (!foreign_field)
    return false;

  return db_format_field_value(foreign_field, foreign_record, buf, buf_sz);
}

// ── Result list helpers ──────────────────────────────────────────────────────

void free_result_list(void *head) {
  result_node_t *node = (result_node_t *)head;
  while (node) {
    result_node_t *next = (result_node_t *)node->next;
    free(node);
    node = next;
  }
}

int count_result_list(void *head) {
  int count = 0;
  for (result_node_t *node = (result_node_t *)head; node; node = (result_node_t *)node->next)
    count++;
  return count;
}

// ── Reflection-based XML loading (uses generated field metadata) ────────────

#include <libxml/parser.h>
#include <libxml/tree.h>

bool db_load_field_from_xml(xmlNodePtr node, void *record_base,
                             const db_field_meta_t *field) {
  if (!node || !record_base || !field) return false;
  
  xmlChar *value = NULL;
  
  // Try attribute first
  value = xmlGetProp(node, (const xmlChar *)field->name);
  
  // If not found as attribute, try child element
  if (!value) {
    for (xmlNode *child = node->children; child; child = child->next) {
      if (child->type == XML_ELEMENT_NODE &&
          xmlStrcmp(child->name, (const xmlChar *)field->name) == 0) {
        value = xmlNodeGetContent(child);
        break;
      }
    }
  }
  
  // Special case: if field is "body" or "text" and we haven't found it yet,
  // use the node's direct text content
  if (!value && (strcmp(field->name, "body") == 0 || strcmp(field->name, "text") == 0)) {
    value = xmlNodeGetContent(node);
  }
  
  if (!value) return false;
  
  void *field_ptr = (char *)record_base + field->offset;
  
  switch (field->type) {
    case DB_TYPE_INT:
      *(int *)field_ptr = atoi((const char *)value);
      break;
      
    case DB_TYPE_STRING:
      {
        const char *src = (const char *)value;
        const char *start = src;
        const char *end = src + strlen(src);
        while (*start && isspace((unsigned char)*start))
          start++;
        while (end > start && isspace((unsigned char)*(end - 1)))
          end--;
        size_t len = (size_t)(end - start);
        size_t cap = (size_t)field->length;
        if (cap == 0) break;
        if (len >= cap) len = cap - 1;
        memcpy((char *)field_ptr, start, len);
        ((char *)field_ptr)[len] = '\0';
      }
      break;
      
    case DB_TYPE_BOOL:
      *(bool *)field_ptr = (value[0] == '1' || value[0] == 't' || value[0] == 'Y');
      break;
      
    case DB_TYPE_FLOAT:
      *(float *)field_ptr = (float)atof((const char *)value);
      break;
      
    case DB_TYPE_DOUBLE:
      *(double *)field_ptr = atof((const char *)value);
      break;
      
    default:
      xmlFree(value);
      return false;
  }
  
  xmlFree(value);
  return true;
}

bool db_load_record_from_xml(xmlNodePtr node, void *record,
                              const db_field_meta_t *fields, int field_count) {
  if (!node || !record || !fields || field_count <= 0) return false;
  
  // Zero the record first
  memset(record, 0, fields[field_count - 1].offset +
         (fields[field_count - 1].type == DB_TYPE_STRING ?
          fields[field_count - 1].length : sizeof(int)));
  
  // Load each field
  for (int i = 0; i < field_count; i++) {
    db_load_field_from_xml(node, record, &fields[i]);
  }
  
  return true;
}

// ── Reflection-based XML saving (uses generated field metadata) ─────────────

bool db_save_field_to_xml(xmlNodePtr node, const void *record_base,
                           const db_field_meta_t *field) {
  if (!node || !record_base || !field) return false;
  
  const void *field_ptr = (const char *)record_base + field->offset;
  char buf[2048];
  
  switch (field->type) {
    case DB_TYPE_INT:
      snprintf(buf, sizeof(buf), "%d", *(const int *)field_ptr);
      break;
      
    case DB_TYPE_STRING: {
      const char *str = (const char *)field_ptr;
      // For body/text fields with potentially large content, use element content
      if (strcmp(field->name, "body") == 0 || strcmp(field->name, "text") == 0) {
        if (str[0] != '\0') {
          xmlNodeSetContent(node, (const xmlChar *)str);
        }
        return true;
      }
      snprintf(buf, sizeof(buf), "%s", str);
      break;
    }
      
    case DB_TYPE_BOOL:
      snprintf(buf, sizeof(buf), "%d", *(const bool *)field_ptr ? 1 : 0);
      break;
      
    case DB_TYPE_FLOAT:
      snprintf(buf, sizeof(buf), "%g", *(const float *)field_ptr);
      break;
      
    case DB_TYPE_DOUBLE:
      snprintf(buf, sizeof(buf), "%g", *(const double *)field_ptr);
      break;
      
    default:
      return false;
  }
  
  xmlSetProp(node, (const xmlChar *)field->name, (const xmlChar *)buf);
  return true;
}

xmlNodePtr db_save_record_to_xml(xmlNodePtr parent, const char *element_name,
                                   const void *record, const db_field_meta_t *fields,
                                   int field_count) {
  if (!parent || !element_name || !record || !fields || field_count <= 0)
    return NULL;
  
  xmlNodePtr node = xmlNewChild(parent, NULL, (const xmlChar *)element_name, NULL);
  if (!node) return NULL;
  
  // Save each field
  for (int i = 0; i < field_count; i++) {
    db_save_field_to_xml(node, record, &fields[i]);
  }
  
  return node;
}
