#ifndef AGENT_ICON_H
#define AGENT_ICON_H

#include <orion/ui.h>

#define AGENT_ICON_MAX_ARTIFACTS 6
#define AGENT_ICON_STRIP_W 16
#define AGENT_ICON_ARTIFACT_SIZE 32
#define AGENT_ICON_ARTIFACT_BADGE_SIZE 13
#define AGENT_ICON_DRAG_THRESHOLD 3

typedef struct {
  int id, count;
  uint32_t texture;
  int image_w, image_h;
  const char *label;
  void *item_data;
  uint32_t count_badge_texture;
  int count_badge_w, count_badge_h;
} agent_artifact_t;

typedef struct {
  int id, count;
  uint32_t texture;
  int image_w, image_h;
  char label[32];
  void *item_data;
  uint32_t count_badge_texture;
  int count_badge_w, count_badge_h;
} agent_artifact_state_t;

typedef struct {
  // Icon state
  uint32_t image_texture;
  int image_w, image_h;
  uint32_t status_texture;
  int status_w, status_h;
  char badge_text[16];
  uint32_t badge_bg, badge_fg;
  int badge_anchor;
  void *item_data;
  bool draggable, drag_pending, dragging;
  int drag_x, drag_y;

  // Artifact state (input = left, output = right)
  agent_artifact_state_t input[AGENT_ICON_MAX_ARTIFACTS];
  int input_count;
  agent_artifact_state_t output[AGENT_ICON_MAX_ARTIFACTS];
  int output_count;

  // Artifact drag state
  int artifact_pending_col; // -1 = none, 0 = input, 1 = output
  int artifact_pending_idx;
  bool artifact_dragging;
  int artifact_grab_x, artifact_grab_y;
  window_t *ghost;
} agent_icon_state_t;

// Message: set input artifacts (left column). lparam = agent_artifact_t[]
#define aimSetInputArtifacts (evUser + 400)
// Message: set output artifacts (right column). lparam = agent_artifact_t[]
#define aimSetOutputArtifacts (evUser + 401)

// Notification: artifact dragged to sibling. lparam = agent_artifact_drop_t*
#define aimnArtifactDrop (evUser + 410)

typedef struct {
  window_t *source, *target;
  int artifact_id;
  void *item_data;
  bool input; // true = dropped on left (input) side
} agent_artifact_drop_t;

result_t win_agent_icon(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

#endif
