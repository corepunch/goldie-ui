#include "formeditor.h"
#include "../../commctl/commctl.h"

typedef struct {
  window_t *list_win;
} prop_browser_state_t;

static window_t *prop_runtime_target(window_t *doc) {
  if (!doc || !doc->children)
    return NULL;
  // One-window mode: doc->children is the runtime preview root.
  return doc->children->children ? doc->children->children : doc->children;
}

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

void property_browser_refresh(window_t *doc) {
  if (!g_app || !g_app->windows[FE_WIN_PROP])
    return;
  prop_browser_state_t *pbs = (prop_browser_state_t *)g_app->windows[FE_WIN_PROP]->userdata;
  if (!pbs || !pbs->list_win)
    return;

  send_message(pbs->list_win, RVM_CLEAR, 0, NULL);

  window_t *target = prop_runtime_target(doc);
  if (!target) {
    prop_add_row(pbs->list_win, "Status", "No runtime window selected");
    return;
  }

  ui_property_entry_t props[64];
  memset(props, 0, sizeof(props));
  int count = (int)send_message(target, edQueryProperties,
                                (uint32_t)(sizeof(props) / sizeof(props[0])), props);
  if (count <= 0) {
    prop_add_row(pbs->list_win, "Status", "Target does not expose properties");
    return;
  }

  char value_buf[128];
  for (int i = 0; i < count; i++) {
    const ui_property_entry_t *e = &props[i];
    if (!e->name[0])
      continue;

    snprintf(value_buf, sizeof(value_buf), "%s", e->value);

    prop_add_row(pbs->list_win, e->name, value_buf);
  }
}

window_t *property_browser_create(hinstance_t hinstance) {
  window_t *win = create_window("Properties",
      WINDOW_NOTRAYBUTTON | WINDOW_NORESIZE,
      MAKERECT(PROPBROWSER_WIN_X, PROPBROWSER_WIN_Y,
               PROPBROWSER_WIN_W, PROPBROWSER_WIN_H),
      NULL, win_property_browser_proc, hinstance, NULL);
  return win;
}

lresult_t win_property_browser_proc(window_t *win, uint32_t msg,
                                   uint32_t wparam, void *lparam) {
  (void)wparam;
  (void)lparam;

  prop_browser_state_t *st = (prop_browser_state_t *)win->userdata;
  switch (msg) {
    case evCreate: {
      st = allocate_window_data(win, sizeof(prop_browser_state_t));
      st->list_win = create_window(
          "", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
          MAKERECT(0, 0, win->frame.w, win->frame.h),
          win, win_reportview, 0, NULL);
      if (st->list_win) {
        send_message(st->list_win, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
        send_message(st->list_win, RVM_SETCOLUMNTITLESVISIBLE, 1, NULL);
        reportview_column_t c0 = {"Property", 88};
        reportview_column_t c1 = {"Value", 80};
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
      return default_winproc(win, msg, wparam, lparam);
  }
}
