#ifndef VIBEOFFICE_TASKS_H
#define VIBEOFFICE_TASKS_H

#include <stdbool.h>
#include <stddef.h>

#define VIBE_TASK_INPUT_MAX 511
#define VIBE_TASK_MODEL_MAX 127
#define VIBE_TASK_OUTPUT_MAX 32767

typedef enum {
  VIBE_TASK_AVAILABLE,
  VIBE_TASK_PENDING,
  VIBE_TASK_BUSY,
  VIBE_TASK_DONE,
  VIBE_TASK_ERROR,
} vibe_task_status_t;

typedef struct {
  bool exists;
  int desk_id;
  vibe_task_status_t status;
  char model[VIBE_TASK_MODEL_MAX + 1];
  char input[VIBE_TASK_INPUT_MAX + 1];
  char output[VIBE_TASK_OUTPUT_MAX + 1];
} vibe_task_t;

typedef struct {
  int desk_id, pid, stdout_fd, stderr_fd;
  size_t stdout_len, stderr_len;
  char stdout_buf[VIBE_TASK_OUTPUT_MAX + 1];
  char stderr_buf[VIBE_TASK_OUTPUT_MAX + 1];
} vibe_process_t;

bool vibe_task_read(int desk_id, vibe_task_t *task);
void vibe_task_recover_stale(int desk_id);
bool vibe_task_submit(vibe_process_t *process, int desk_id, const char *model, const char *input,
                      char *error, size_t error_size);
bool vibe_task_poll(vibe_process_t *process);
void vibe_task_abort(vibe_process_t *process, const char *reason);
const char *vibe_task_status_name(vibe_task_status_t status);

#endif
