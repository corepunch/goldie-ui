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

const db_action_def_t *db_api_find_action(const db_api_def_t *api, const char *name) {
  if (!api || !name || !api->actions || api->action_count <= 0) return NULL;
  for (int i = 0; i < api->action_count; i++) {
    if (api->actions[i].name && strcmp(api->actions[i].name, name) == 0)
      return &api->actions[i];
  }
  return NULL;
}
