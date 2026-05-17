#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "user.h"
#include "database.h"

// ── Database class registry (analogous to window class registry) ────────────

#define MAX_DATABASE_CLASSES 16

static db_class_desc_t g_database_classes[MAX_DATABASE_CLASSES];
static int g_database_class_count = 0;

static bool streq(const char *a, const char *b) {
  return a && b && strcmp(a, b) == 0;
}

bool register_database_class(const db_class_desc_t *desc) {
  if (!desc || !desc->class_name || !*desc->class_name || !desc->proc)
    return false;
  
  // Check if already registered (idempotent)
  for (int i = 0; i < g_database_class_count; i++) {
    if (streq(g_database_classes[i].class_name, desc->class_name))
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
    if (streq(g_database_classes[i].class_name, class_name))
      return g_database_classes[i].proc;
  }
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

const db_source_def_t *db_api_find_source(const db_api_def_t *api, const char *name) {
  if (!api || !name || !api->sources || api->source_count <= 0) return NULL;
  for (int i = 0; i < api->source_count; i++) {
    if (api->sources[i].name && strcmp(api->sources[i].name, name) == 0)
      return &api->sources[i];
  }
  return NULL;
}

const db_view_binding_t *db_api_find_binding(const db_api_def_t *api, const char *name) {
  if (!api || !name || !api->bindings || api->binding_count <= 0) return NULL;
  for (int i = 0; i < api->binding_count; i++) {
    if (api->bindings[i].name && strcmp(api->bindings[i].name, name) == 0)
      return &api->bindings[i];
  }
  return NULL;
}

const db_view_binding_t *db_api_find_binding_for_view(const db_api_def_t *api, const char *view) {
  if (!api || !view || !api->bindings || api->binding_count <= 0) return NULL;
  for (int i = 0; i < api->binding_count; i++) {
    if (api->bindings[i].view && strcmp(api->bindings[i].view, view) == 0)
      return &api->bindings[i];
  }
  return NULL;
}

const db_action_def_t *db_api_find_action(const db_api_def_t *api, const char *name) {
  if (!api || !name || !api->actions || api->action_count <= 0) return NULL;
  for (int i = 0; i < api->action_count; i++) {
    if (api->actions[i].name && strcmp(api->actions[i].name, name) == 0)
      return &api->actions[i];
  }
  return NULL;
}

bool db_object_get_field_text(const db_field_msg_binding_t *bindings, int binding_count,
                              db_object_proc_t proc, const void *object,
                              const char *field, char *buf, size_t buf_sz) {
  if (!bindings || binding_count <= 0 || !proc || !object || !field || !buf || buf_sz == 0)
    return false;
  // HIWORD(wparam) is 16-bit in the Action-Message DDX contract.
  if (buf_sz > 0xffffu)
    return false;
  for (int i = 0; i < binding_count; i++) {
    if (!bindings[i].field || strcmp(bindings[i].field, field) != 0)
      continue;
    buf[0] = '\0';
    uint32_t packed = MAKEDWORD(bindings[i].column_id, (uint16_t)buf_sz);
    return proc(object, dbObjGetFieldText, packed, buf) ? true : false;
  }
  return false;
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
