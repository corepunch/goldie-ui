#ifndef VIBEOFFICE_MODELS_H
#define VIBEOFFICE_MODELS_H

#include <orion/ui.h>

typedef struct {
  int id;
  const char *name;
  const char *opencode_id;
} vibe_model_info_t;

database_t *vibe_models_create(void);
const vibe_model_info_t *vibe_model_at(int index);
const vibe_model_info_t *vibe_model_by_id(int id);
const vibe_model_info_t *vibe_model_by_opencode_id(const char *opencode_id);
int vibe_model_count(void);

#endif
