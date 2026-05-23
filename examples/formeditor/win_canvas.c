// Minimal canvas host for runtime form windows loaded directly from XML nodes.

#include "formeditor.h"

static result_t canvas_form_root_proc(window_t *win, uint32_t msg,
                                      uint32_t wparam, void *lparam) {
  return win_space(win, msg, wparam, lparam);
}

static window_t *canvas_create_runtime_form(window_t *doc) {
  window_t *canvas = doc ? doc->children : NULL;
  return fe_create_runtime_form_window(doc,
                                       canvas,
                                       canvas_form_root_proc);
}

void canvas_rebuild_live_controls(window_t *doc) {
  window_t *canvas = doc ? doc->children : NULL;
  if (!doc || !canvas)
    return;

  canvas_state_t *s = (canvas_state_t *)canvas->userdata;
  if (!s)
    return;

  while (canvas->children)
    destroy_window(canvas->children);

  s->form_root_win = canvas_create_runtime_form(doc);
  invalidate_window(canvas);
}

void canvas_sync_live_controls(window_t *doc) {
  canvas_rebuild_live_controls(doc);
}

void canvas_set_component_drag_hover(window_t *doc, bool active, window_t *target) {
  (void)doc;
  (void)active;
  (void)target;
}

window_t *canvas_find_component_drop_target(window_t *doc, int type,
                                            int canvas_x, int canvas_y) {
  (void)doc;
  (void)type;
  (void)canvas_x;
  (void)canvas_y;
  return NULL;
}

bool canvas_drop_component(window_t *doc, int type, int canvas_x, int canvas_y) {
  (void)doc;
  (void)type;
  (void)canvas_x;
  (void)canvas_y;
  return false;
}

bool canvas_drop_component_to_target(window_t *doc, int type, window_t *target,
                                     int screen_x, int screen_y) {
  (void)doc;
  (void)type;
  (void)target;
  (void)screen_x;
  (void)screen_y;
  return false;
}

result_t win_canvas_proc(window_t *win, uint32_t msg,
                         uint32_t wparam, void *lparam) {
  (void)wparam;
  canvas_state_t *s = (canvas_state_t *)win->userdata;
  window_t *doc = s ? s->doc : NULL;

  switch (msg) {
    case evCreate: {
      canvas_state_t *st = allocate_window_data(win, sizeof(canvas_state_t));
      st->doc = (window_t *)lparam;
      st->selected_idx = -1;
      st->preview_type = -1;
      st->hover_layout_idx = -1;
      st->hover_layout_rc = (irect16_t){0, 0, 0, 0};
      st->pan = (ipoint16_t){0, 0};
      st->external_component_drag = false;
      st->drag = (drag_state_t){.mode = DRAG_NONE};
      canvas_rebuild_live_controls(st->doc);
      return true;
    }

    case evResize:
      if (doc) {
        canvas_sync_live_controls(doc);
      }
      return false;

    case evPaint:
      // fill_rect(get_sys_color(brWorkspaceBg), R(0, 0, win->frame.w, win->frame.h));
      return false;

    default:
      return false;
  }
}
