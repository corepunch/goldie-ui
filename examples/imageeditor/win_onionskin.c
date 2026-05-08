// Onion Skin settings dialog

#include "imageeditor.h"

typedef struct {
  bool enabled;
  int prev1, prev2, prev3, prev4;
  int next1, next2, next3, next4;
} onion_skin_state_t;

static int onion_clamp_percent(int v) {
  return CLAMP(v, 0, 100);
}

static int onion_count_nonzero(const uint8_t *vals, int n) {
  int count = 0;
  if (!vals || n <= 0) return 0;
  for (int i = 0; i < n; i++) {
    if (vals[i] > 0)
      count = i + 1;
  }
  return count;
}

static const ctrl_binding_t k_onion_bindings[] = {
  DDX_CHECK(ID_ONION_SKIN_ENABLED, onion_skin_state_t, enabled),
  DDX_TEXT(ID_ONION_SKIN_PREV1, onion_skin_state_t, prev1),
  DDX_TEXT(ID_ONION_SKIN_PREV2, onion_skin_state_t, prev2),
  DDX_TEXT(ID_ONION_SKIN_PREV3, onion_skin_state_t, prev3),
  DDX_TEXT(ID_ONION_SKIN_PREV4, onion_skin_state_t, prev4),
  DDX_TEXT(ID_ONION_SKIN_NEXT1, onion_skin_state_t, next1),
  DDX_TEXT(ID_ONION_SKIN_NEXT2, onion_skin_state_t, next2),
  DDX_TEXT(ID_ONION_SKIN_NEXT3, onion_skin_state_t, next3),
  DDX_TEXT(ID_ONION_SKIN_NEXT4, onion_skin_state_t, next4),
};

bool show_onion_skin_dialog(window_t *parent) {
  if (!g_app) return false;

  onion_skin_state_t st = {
    .enabled = g_app->anim_trace_enabled,
    .prev1 = g_app->anim_trace_prev_opacity[0],
    .prev2 = g_app->anim_trace_prev_opacity[1],
    .prev3 = g_app->anim_trace_prev_opacity[2],
    .prev4 = g_app->anim_trace_prev_opacity[3],
    .next1 = g_app->anim_trace_next_opacity[0],
    .next2 = g_app->anim_trace_next_opacity[1],
    .next3 = g_app->anim_trace_next_opacity[2],
    .next4 = g_app->anim_trace_next_opacity[3],
  };

  form_def_t form = imageeditor_onion_skin_form;
  form.bindings = k_onion_bindings;
  form.binding_count = ARRAY_LEN(k_onion_bindings);
  form.ok_id = ID_ONION_SKIN_OK;
  form.cancel_id = ID_ONION_SKIN_CANCEL;
  if (!show_ddx_dialog(&form, "Onion Skin", parent, &st))
    return false;

  g_app->anim_trace_enabled = st.enabled;
  g_app->anim_trace_prev_opacity[0] = (uint8_t)onion_clamp_percent(st.prev1);
  g_app->anim_trace_prev_opacity[1] = (uint8_t)onion_clamp_percent(st.prev2);
  g_app->anim_trace_prev_opacity[2] = (uint8_t)onion_clamp_percent(st.prev3);
  g_app->anim_trace_prev_opacity[3] = (uint8_t)onion_clamp_percent(st.prev4);
  g_app->anim_trace_next_opacity[0] = (uint8_t)onion_clamp_percent(st.next1);
  g_app->anim_trace_next_opacity[1] = (uint8_t)onion_clamp_percent(st.next2);
  g_app->anim_trace_next_opacity[2] = (uint8_t)onion_clamp_percent(st.next3);
  g_app->anim_trace_next_opacity[3] = (uint8_t)onion_clamp_percent(st.next4);
  g_app->anim_trace_frames = onion_count_nonzero(g_app->anim_trace_prev_opacity,
                                                  ONION_SKIN_MAX_STEPS);
  return true;
}
