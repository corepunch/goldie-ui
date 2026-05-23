// Runtime preview helpers for form windows loaded directly from XML nodes.

#include "formeditor.h"

static void canvas_restore_local_draw_space(window_t *win) {
  window_t *root = get_root_window(win);
  int t = titlebar_height(root);

  set_viewport(root->frame);
  set_projection(root->hscroll.pos,
                 -t + root->vscroll.pos,
                 root->frame.w + root->hscroll.pos,
                 root->frame.h - t + root->vscroll.pos);
}

static void canvas_drag_overlay_clear(window_t *doc) {
  form_doc_state_t *st = fe_doc_state(doc);
  if (!doc || !st || !st->drag_overlay_active)
    return;
  st->drag_overlay_active = false;
  st->drag_overlay_rect = (irect16_t){0};
  invalidate_window(doc);
}

// Return target's client rect in host's client coordinate space.
// host==target => {0,0,w,h}; nested children are translated to host-local.
static irect16_t client_rect_in_host(window_t *host, window_t *target) {
  irect16_t tr = get_client_rect(target);

  if (!host || !target)
    return (irect16_t){0, 0, 0, 0};
  if (host == target)
    return (irect16_t){0, 0, tr.w, tr.h};

  ipoint16_t host_client = window_client_origin_xy(host);
  ipoint16_t target_client = window_client_origin_xy(target);
  return (irect16_t){
    (int16_t)(target_client.x - host_client.x),
    (int16_t)(target_client.y - host_client.y),
    tr.w,
    tr.h,
  };
}

static bool canvas_drag_overlay_update(window_t *doc, int local_x, int local_y) {
  form_doc_state_t *st = fe_doc_state(doc);
  if (!doc || !st)
    return false;
  lresult_t hit_res = default_winproc(doc, evHitTest, MAKEDWORD(local_x, local_y), NULL);
  window_t *target = (window_t *)(intptr_t)hit_res;
  if (target) {
    st->drag_overlay_active = true;
    st->drag_overlay_rect = client_rect_in_host(doc, target);
    invalidate_window(doc);
    return true;
  } else {
    return false;
  }
}

void canvas_rebuild_live_controls(window_t *doc) {
  if (!doc)
    return;

  canvas_drag_overlay_clear(doc);

  while (doc->children)
    destroy_window(doc->children);

  fe_create_runtime_form_window(doc, doc, default_winproc);  
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
    case evHitTest:
      // Keep document canvas as the normal input target.
      // Drag preview targeting uses default_winproc(evHitTest) explicitly.
      return (lresult_t)(intptr_t)win;
    case evPaint:
      default_winproc(win, msg, wparam, lparam);
      if (doc && doc->drag_overlay_active) {
        canvas_restore_local_draw_space(win);
        draw_sel_rect(doc->drag_overlay_rect);
      }
      return true;
    case evMouseDrag:
      if (!canvas_drag_overlay_update(win, LOWORD(wparam), HIWORD(wparam)))
        canvas_drag_overlay_clear(win);
      return true;
    case evMouseDrop:
      canvas_drag_overlay_clear(win);
      return true;
    case evSetFocus:
      if (doc && window_has_state(win, WINDOW_STATE_VISIBLE))
        form_doc_activate(win);
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
