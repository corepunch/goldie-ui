#include <string.h>

#include "user.h"

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
  for (int i = 0; i < binding_count; i++) {
    if (!bindings[i].field || strcmp(bindings[i].field, field) != 0)
      continue;
    buf[0] = '\0';
    return proc(object, bindings[i].msg, (uint32_t)buf_sz, buf) ? true : false;
  }
  return false;
}
