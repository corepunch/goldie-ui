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

static bool canvas_payload_is_control(const ui_drag_item_payload_t *payload) {
  return payload && payload->item_type == UI_DRAG_ITEM_CONTROL_CLASS;
}

static bool canvas_payload_is_database_field(const ui_drag_item_payload_t *payload) {
  return payload && payload->item_type == UI_DRAG_ITEM_DATABASE_FIELD;
}

static void canvas_select_runtime_window(window_t *doc, window_t *target) {
  form_doc_state_t *st = fe_doc_state(doc);
  if (!doc || !st)
    return;
  if (target && get_root_window(target) != doc)
    target = NULL;
  if (!target)
    target = doc->children ? doc->children : doc;
  if (st->selected_window == target)
    return;
  st->selected_window = target;
  invalidate_window(doc);
  property_browser_refresh(doc);
  fe_notify(FE_EVENT_SELECTION_CHANGED, doc);
}

static window_t *canvas_hit_child(window_t *doc, int canvas_x, int canvas_y) {
  if (!doc)
    return NULL;

  lresult_t hit_res = default_winproc(doc, evHitTest,
                                      MAKEDWORD((uint16_t)canvas_x, (uint16_t)canvas_y),
                                      NULL);
  window_t *hit = (window_t *)(intptr_t)hit_res;
  if (!hit) {
    irect16_t cr = get_client_rect(doc);
    if (canvas_x < 0 || canvas_y < 0 || canvas_x >= cr.w || canvas_y >= cr.h)
      return NULL;
    hit = doc;
  }
  return hit;
}

static bool canvas_drag_overlay_update(window_t *doc,
                                       const ui_drag_item_payload_t *payload,
                                       int local_x, int local_y) {
  form_doc_state_t *st = fe_doc_state(doc);
  if (!doc || !st || !payload)
    return false;

  window_t *target = NULL;
  if (canvas_payload_is_control(payload)) {
    int component_id = (int)payload->item_class;
    target = canvas_find_component_drop_target(doc, component_id, local_x, local_y);
  } else if (canvas_payload_is_database_field(payload)) {
    target = g_ui_runtime.drag_item_target;
  }

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
  form_doc_state_t *st = fe_doc_state(doc);

  canvas_drag_overlay_clear(doc);
  if (st)
    st->selected_window = NULL;

  while (doc->children)
    destroy_window(doc->children);

  fe_create_runtime_form_window(doc, doc, default_winproc);  
  if (st)
    st->selected_window = doc->children;
  invalidate_window(doc);
}

void canvas_set_component_drag_hover(window_t *doc, bool active, window_t *target) {
  (void)doc;
  (void)active;
  (void)target;
}

window_t *canvas_find_component_drop_target(window_t *doc, int type,
                                            int canvas_x, int canvas_y) {
  if (!doc)
    return NULL;
  ui_drag_item_payload_t payload = {
    .item_type = UI_DRAG_ITEM_CONTROL_CLASS,
    .item_class = type >= 0 ? (uint32_t)type : 0,
    .item_id = type >= 0 ? (uint32_t)type : 0,
  };
  for (window_t *p = canvas_hit_child(doc, canvas_x, canvas_y); p; p = p->parent) {
    if (send_message(p, evAcceptsDrop,
                     MAKEDWORD(UI_DRAG_ITEM_CONTROL_CLASS, (uint16_t)payload.item_class),
                     &payload)) {
      return p;
    }
  }
  return NULL;
}

bool canvas_drop_component_to_target(window_t *doc, int type, window_t *target,
                                     int screen_x, int screen_y) {
  if (!doc)
    return false;
  ui_drag_item_payload_t payload = {
    .item_type = UI_DRAG_ITEM_CONTROL_CLASS,
    .item_class = type >= 0 ? (uint32_t)type : 0,
    .item_id = type >= 0 ? (uint32_t)type : 0,
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
    case evAcceptsDrop:
      return default_winproc(win, msg, wparam, lparam);
    case evPaint:
      default_winproc(win, msg, wparam, lparam);
      if (doc && doc->selected_window && doc->selected_window != win) {
        canvas_restore_local_draw_space(win);
        draw_sel_rect(client_rect_in_host(win, doc->selected_window));
      }
      if (doc && doc->drag_overlay_active) {
        canvas_restore_local_draw_space(win);
        draw_sel_rect(doc->drag_overlay_rect);
      }
      return true;
    case evLeftButtonDown: {
      window_t *target = canvas_hit_child(win, LOWORD(wparam), HIWORD(wparam));
      canvas_select_runtime_window(win, target);
      return true;
    }
    case evMouseDrag:
      if (!canvas_payload_is_control(payload) &&
          !canvas_payload_is_database_field(payload)) {
        canvas_drag_overlay_clear(win);
        return true;
      } else if (!canvas_drag_overlay_update(win, payload, LOWORD(wparam), HIWORD(wparam))) {
        canvas_drag_overlay_clear(win);
      }
      return true;
    case evMouseDragEnter:
      if (canvas_payload_is_control(payload) ||
          canvas_payload_is_database_field(payload)) {
        (void)canvas_drag_overlay_update(win, payload, LOWORD(wparam), HIWORD(wparam));
      }
      return true;
    case evMouseDragLeave:
      canvas_drag_overlay_clear(win);
      return true;
    case evMouseDrop:
      if (canvas_payload_is_control(payload)) {
        int component_id = (int)payload->item_class;
        window_t *drop_target = canvas_find_component_drop_target(
            win, component_id, LOWORD(wparam), HIWORD(wparam));
        (void)fe_controller_drop_create(win, payload, drop_target);
      } else if (canvas_payload_is_database_field(payload)) {
        (void)fe_controller_drop_create(win, payload, g_ui_runtime.drag_item_target);
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
