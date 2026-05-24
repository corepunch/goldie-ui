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
  window_t *target = canvas_find_component_drop_target(doc, -1, local_x, local_y);
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

static window_t *canvas_normalize_drop_target(window_t *doc, window_t *hit) {
  if (!doc || !hit)
    return NULL;
  for (window_t *p = hit; p; p = p->parent) {
    if (p == doc)
      return doc;
    if (p->flags & WINDOW_LAYOUT_CONTAINER)
      return p;
  }
  return NULL;
}

window_t *canvas_find_component_drop_target(window_t *doc, int type,
                                            int canvas_x, int canvas_y) {
  (void)type;
  if (!doc)
    return NULL;
  lresult_t hit_res = default_winproc(doc, evHitTest,
                                      MAKEDWORD((uint16_t)canvas_x, (uint16_t)canvas_y),
                                      NULL);
  return canvas_normalize_drop_target(doc, (window_t *)(intptr_t)hit_res);
}

bool canvas_drop_component_to_target(window_t *doc, int type, window_t *target,
                                     int screen_x, int screen_y) {
  if (!doc)
    return false;
  ui_drag_item_payload_t payload = {
    .tool_ident = type,
  };
  ipoint16_t client = window_client_origin_xy(doc);
  int local_x = screen_x - client.x + doc->hscroll.pos;
  int local_y = screen_y - client.y + doc->vscroll.pos;
  window_t *drop_target = target ? target
                                 : canvas_find_component_drop_target(doc, type, local_x, local_y);
  return fe_controller_drop_create(doc, &payload, drop_target);
}

lresult_t win_canvas_proc(window_t *win, uint32_t msg,
                          uint32_t wparam, void *lparam) {
  form_doc_state_t *doc = fe_doc_state(win);
  ui_drag_item_payload_t *payload = (ui_drag_item_payload_t *)lparam;
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
      if (!payload) {
        canvas_drag_overlay_clear(win);
        return true;
      } else if (!canvas_drag_overlay_update(win, LOWORD(wparam), HIWORD(wparam))) {
        canvas_drag_overlay_clear(win);
      }
      return true;
    case evMouseDragEnter:
      if (payload) {
        (void)canvas_drag_overlay_update(win, LOWORD(wparam), HIWORD(wparam));
      }
      return true;
    case evMouseDragLeave:
      canvas_drag_overlay_clear(win);
      return true;
    case evMouseDrop:
      if (payload) {
        window_t *drop_target = canvas_find_component_drop_target(
            win, payload->tool_ident, LOWORD(wparam), HIWORD(wparam));
        (void)fe_controller_drop_create(win, payload, drop_target);
      }
      canvas_drag_overlay_clear(win);
      return true;
    case evSetFocus:
      if (doc && window_has_state(win, WINDOW_STATE_VISIBLE))
        form_doc_activate(win);
      return default_winproc(win, msg, wparam, lparam);
    case evResize: {
      window_layout_sync(win);
      invalidate_window(win);
      fe_doc_mark_modified(win);
      if (g_app)
        g_app->project.modified = true;
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
