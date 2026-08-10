#include "scener.h"
#include <orion/commctl/commctl.h>

typedef struct {
  window_t *list_win;
  int selected_obj;
} prop_browser_state_t;

static void prop_add_row(window_t *list, const char *name, const char *value) {
  const char *subs[1] = {value ? value : ""};
  reportview_item_t item = {
    .text = name ? name : "",
    .color = get_sys_color(brTextNormal),
    .userdata = 0,
    .subitems = {subs[0]},
    .subitem_count = 1,
  };
  send_message(list, RVM_ADDITEM, 0, &item);
}

void property_browser_refresh(window_t *win) {
  if (!win) return;
  prop_browser_state_t *st = (prop_browser_state_t *)win->userdata;
  if (!st || !st->list_win) return;

  send_message(st->list_win, RVM_CLEAR, 0, NULL);

  scene_doc_t *doc = g_app ? g_app->active_doc : NULL;
  if (!doc) {
    prop_add_row(st->list_win, "Status", "No active document");
    return;
  }

  int idx = doc->scene.selectedObj;
  if (idx < 0 || idx >= doc->scene.nobjs) {
    prop_add_row(st->list_win, "Status", "No object selected");
    return;
  }

  SceneObj *obj = &doc->scene.objs[idx];
  char value[128];

  prop_add_row(st->list_win, "Object", "");
  
  snprintf(value, sizeof(value), "%d", idx);
  prop_add_row(st->list_win, "Index", value);

  prop_add_row(st->list_win, "", "");
  prop_add_row(st->list_win, "Transform", "");

  snprintf(value, sizeof(value), "%.2f", obj->pos.x);
  prop_add_row(st->list_win, "Pos X", value);
  snprintf(value, sizeof(value), "%.2f", obj->pos.y);
  prop_add_row(st->list_win, "Pos Y", value);
  snprintf(value, sizeof(value), "%.2f", obj->pos.z);
  prop_add_row(st->list_win, "Pos Z", value);

  snprintf(value, sizeof(value), "%.1f", obj->rot.x);
  prop_add_row(st->list_win, "Rot X", value);
  snprintf(value, sizeof(value), "%.1f", obj->rot.y);
  prop_add_row(st->list_win, "Rot Y", value);
  snprintf(value, sizeof(value), "%.1f", obj->rot.z);
  prop_add_row(st->list_win, "Rot Z", value);

  snprintf(value, sizeof(value), "%.2f", obj->scale.x);
  prop_add_row(st->list_win, "Scale X", value);
  snprintf(value, sizeof(value), "%.2f", obj->scale.y);
  prop_add_row(st->list_win, "Scale Y", value);
  snprintf(value, sizeof(value), "%.2f", obj->scale.z);
  prop_add_row(st->list_win, "Scale Z", value);

  prop_add_row(st->list_win, "", "");
  prop_add_row(st->list_win, "Material", "");

  snprintf(value, sizeof(value), "%.2f", obj->color.x);
  prop_add_row(st->list_win, "Color R", value);
  snprintf(value, sizeof(value), "%.2f", obj->color.y);
  prop_add_row(st->list_win, "Color G", value);
  snprintf(value, sizeof(value), "%.2f", obj->color.z);
  prop_add_row(st->list_win, "Color B", value);

  snprintf(value, sizeof(value), "%.1f", obj->shininess);
  prop_add_row(st->list_win, "Shininess", value);

  prop_add_row(st->list_win, "", "");
  prop_add_row(st->list_win, "Flags", "");

  prop_add_row(st->list_win, "Cast Shadow", obj->castsShadow ? "Yes" : "No");
  prop_add_row(st->list_win, "Renderable", obj->renderable ? "Yes" : "No");
  prop_add_row(st->list_win, "Unlit", obj->unlit ? "Yes" : "No");
}

window_t *create_property_browser_window(void) {
  if (!g_app) return NULL;
  int sw = ui_get_system_metrics(kSystemMetricScreenWidth);
  int sh = ui_get_system_metrics(kSystemMetricScreenHeight);
  int win_h = sh - MENUBAR_HEIGHT - TOOLBAR_BAND_HEIGHT - 40;
  int win_w = 200;
  
  window_t *win = create_window("Properties",
      WINDOW_ALWAYSONTOP | WINDOW_NOTRAYBUTTON | WINDOW_NORESIZE,
      MAKERECT(sw - win_w - 180, MENUBAR_HEIGHT + TOOLBAR_BAND_HEIGHT, win_w, win_h),
      NULL, win_property_browser, g_app->hinstance, NULL);
  if (win) show_window(win, true);
  return win;
}

result_t win_property_browser(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  (void)wparam;
  (void)lparam;

  prop_browser_state_t *st = (prop_browser_state_t *)win->userdata;
  switch (msg) {
    case evCreate: {
      st = allocate_window_data(win, sizeof(prop_browser_state_t));
      st->selected_obj = -1;
      st->list_win = create_window(
          "", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
          MAKERECT(0, 0, win->frame.w, win->frame.h),
          win, win_reportview, 0, NULL);
      if (st->list_win) {
        send_message(st->list_win, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
        send_message(st->list_win, RVM_SETCOLUMNTITLESVISIBLE, 0, NULL);
        reportview_column_t c0 = {"Property", 80};
        reportview_column_t c1 = {"Value", 0};
        send_message(st->list_win, RVM_ADDCOLUMN, 0, &c0);
        send_message(st->list_win, RVM_ADDCOLUMN, 0, &c1);
      }
      return true;
    }

    case evResize:
      if (st && st->list_win)
        resize_window(st->list_win, win->frame.w, win->frame.h);
      return false;

    default:
      return false;
  }
}
