// Runtime preview helpers for form windows loaded directly from XML nodes.

#include "formeditor.h"

static result_t canvas_form_root_proc(window_t *win, uint32_t msg,
                                      uint32_t wparam, void *lparam) {
  if (msg == evPaint) {
    irect16_t cr = get_client_rect(win);
    fill_rect(get_sys_color(brWindowBg), cr);
    return false;
  }
  return win_space(win, msg, wparam, lparam);
}

static window_t *canvas_create_runtime_form(window_t *doc) {
  return fe_create_runtime_form_window(doc,
                                       doc,
                                       canvas_form_root_proc);
}

void canvas_rebuild_live_controls(window_t *doc) {
  if (!doc)
    return;

  while (doc->children)
    destroy_window(doc->children);

  canvas_create_runtime_form(doc);
  invalidate_window(doc);
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

bool canvas_drop_component_to_target(window_t *doc, int type, window_t *target,
                                     int screen_x, int screen_y) {
  (void)doc;
  (void)type;
  (void)target;
  (void)screen_x;
  (void)screen_y;
  return false;
}
