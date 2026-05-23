// Runtime preview helpers for form windows loaded directly from XML nodes.

#include "formeditor.h"

static lresult_t canvas_form_root_proc(window_t *win, uint32_t msg,
                                      uint32_t wparam, void *lparam) {
  switch (msg) {
    case evPaint: 
      return default_winproc(win, msg, wparam, lparam);

    // Keep the runtime canvas root as the mouse target.
    // Returning handled for hit-test blocks recursion into preview children.
    case evHitTest:
      return (lresult_t)(intptr_t)win;

    // Intercept pointer input at the canvas layer so preview controls
    // remain non-interactive while editing.
    case evLeftButtonDown:
    case evLeftButtonDoubleClick:
    case evLeftButtonUp:
    case evRightButtonDown:
    case evRightButtonUp:
    case evMouseMove:
    case evWheel:
      (void)wparam;
      (void)lparam;
      return true;

    // Intercept keyboard/text input at the canvas layer.
    case evKeyDown:
    case evKeyUp:
    case evTextInput:
      (void)wparam;
      (void)lparam;
      return true;

    default:
      return default_winproc(win, msg, wparam, lparam);
  }
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

lresult_t win_canvas_proc(window_t *win, uint32_t msg,
                          uint32_t wparam, void *lparam) {
  form_doc_state_t *doc = fe_doc_state(win);
  switch (msg) {
    case evCreate:
      return true;
    case evSetFocus:
      if (doc && window_has_state(win, WINDOW_STATE_VISIBLE))
        form_doc_activate(win);
      return default_winproc(win, msg, wparam, lparam);
    case evPaint:
      return default_winproc(win, msg, wparam, lparam);
    case evResize: {
      if (win && win->children) {
        irect16_t cr = get_client_rect(win);
        int new_w = MAX(1, cr.w);
        int new_h = MAX(1, cr.h);
        bool changed = (win->children->frame.w != new_w || win->children->frame.h != new_h);
        resize_window(win->children, cr.w, cr.h);
        if (changed) {
          fe_doc_mark_modified(win);
          if (g_app)
            g_app->project.modified = true;
        }
      }
      return default_winproc(win, msg, wparam, lparam);
    }
    case evClose:
      if (!doc)
        return false;
      show_window(win, false);
      forms_browser_refresh();
      return true;
    default:
      return default_winproc(win, msg, wparam, lparam);
  }
}
