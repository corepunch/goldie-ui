// Project forms browser for the Orion Form Editor.
//
// Shows the forms loaded from a .orion project.

#include "formeditor.h"
#include "../../commctl/commctl.h"
#include "../../user/icons.h"

#define FORMS_ID_NEW     1
#define FORMS_ID_DELETE  2

typedef struct {
  window_t *list_win;
  int       subscription_id;
} forms_browser_state_t;

static const toolbar_item_t kFormsToolbar[] = {
  { TOOLBAR_ITEM_BUTTON, FORMS_ID_NEW,    sysicon_add,    0, 0, "New form" },
  { TOOLBAR_ITEM_BUTTON, FORMS_ID_DELETE, sysicon_delete, 0, 0, "Delete form" },
};

static int forms_doc_count(void) {
  return g_app ? g_app->form_count : 0;
}

static window_t *forms_doc_at(int idx) {
  if (!g_app || idx < 0 || idx >= g_app->form_count)
    return NULL;
  return g_app->forms[idx];
}

static const char *forms_doc_label(window_t *doc) {
  if (!doc || !doc->title[0])
    return "Untitled";
  return doc->title;
}

static void forms_browser_rebuild(forms_browser_state_t *st) {
  if (!st || !st->list_win) return;

  send_message(st->list_win, RVM_SETREDRAW, 0, NULL);
  send_message(st->list_win, RVM_CLEAR, 0, NULL);

  int idx = 0;
  int selected = -1;
  for (idx = 0; g_app && idx < g_app->form_count; idx++) {
    window_t *doc = g_app->forms[idx];
    if (!doc || !fe_doc_state(doc))
      continue;
    reportview_item_t item = {0};
    item.text = forms_doc_label(doc);
    item.color = get_sys_color(brTextNormal);
    item.userdata = (uint32_t)idx;
    send_message(st->list_win, RVM_ADDITEM, 0, &item);
    if (g_app && doc == g_app->active_form)
      selected = idx;
  }

  send_message(st->list_win, RVM_SETREDRAW, 1, NULL);
  if (selected >= 0)
    send_message(st->list_win, RVM_SETSELECTION, (uint32_t)selected, NULL);
}

void forms_browser_refresh(void) {
  if (!g_app || !g_app->windows[FE_WIN_FORMS]) return;
  forms_browser_state_t *st = (forms_browser_state_t *)g_app->windows[FE_WIN_FORMS]->userdata;
  forms_browser_rebuild(st);
}

static void forms_browser_observer(fe_event_type_t event, window_t *doc, void *ctx) {
  (void)doc;
  (void)ctx;
  switch (event) {
    case FE_EVENT_DOCUMENT_CREATED:
    case FE_EVENT_DOCUMENT_CLOSED:
    case FE_EVENT_DOCUMENT_ACTIVATED:
    case FE_EVENT_DOCUMENT_MODIFIED:
      forms_browser_refresh();
      break;
    default:
      break;
  }
}

window_t *forms_browser_create(hinstance_t hinstance) {
  window_t *win = create_window("Forms",
      WINDOW_NOTRAYBUTTON | WINDOW_NORESIZE | WINDOW_TOOLBAR,
      MAKERECT(FORMS_WIN_X, FORMS_WIN_Y, FORMS_WIN_W, FORMS_WIN_H),
      NULL, win_forms_browser_proc, hinstance, NULL);
  if (win) show_window(win, true);
  return win;
}

static void forms_add_new(void) {
  if (!g_app) return;
  window_t *doc = create_form_doc(FORM_DEFAULT_W, FORM_DEFAULT_H);
  int n = forms_doc_count();
  snprintf(doc->title, sizeof(doc->title), "Form %d", n);
  fe_doc_mark_modified(doc);
  g_app->project.modified = true;
  forms_browser_refresh();
}

static void forms_delete_active(void) {
  if (!g_app || !g_app->active_form) return;
  window_t *doc = g_app->active_form;
  close_form_doc(doc);
  g_app->project.modified = true;
  forms_browser_refresh();
}

result_t win_forms_browser_proc(window_t *win, uint32_t msg,
                                uint32_t wparam, void *lparam) {
  forms_browser_state_t *st = (forms_browser_state_t *)win->userdata;
  (void)lparam;
  switch (msg) {
    case evCreate: {
      st = allocate_window_data(win, sizeof(forms_browser_state_t));
      if (!st)
        return false;

      irect16_t cr = get_client_rect(win);
      st->list_win = create_window(
          "", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
          MAKERECT(0, 0, cr.w, cr.h),
          win, win_reportview, 0, NULL);
      if (!st->list_win)
        return false;

      send_message(st->list_win, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
      send_message(st->list_win, RVM_SETCOLUMNTITLESVISIBLE, 0, NULL);
      {
        reportview_column_t c0 = { "Form", 0 };
        send_message(st->list_win, RVM_ADDCOLUMN, 0, &c0);
      }

      send_message(win, tbSetItems, ARRAY_LEN(kFormsToolbar),
                   (void *)kFormsToolbar);
      st->subscription_id = fe_subscribe(forms_browser_observer, win);
      forms_browser_rebuild(st);
      return true;
    }

    case tbButtonClick:
      switch ((uint16_t)wparam) {
        case FORMS_ID_NEW:
          forms_add_new();
          return true;
        case FORMS_ID_DELETE:
          forms_delete_active();
          return true;
        default:
          return false;
      }

    case evResize:
      if (st && st->list_win) {
        irect16_t cr = get_client_rect(win);
        resize_window(st->list_win, cr.w, cr.h);
      }
      return false;

    case evCommand: {
      uint16_t notif = HIWORD(wparam);
      if (!st || lparam != st->list_win)
        return false;
      if (notif == RVN_SELCHANGE)
        return true;
      if (notif != RVN_DBLCLK)
        return false;

      window_t *doc = forms_doc_at((int)LOWORD(wparam));
      if (!doc)
        return false;
      form_doc_activate(doc);
      show_window(doc, true);
      forms_browser_refresh();
      return true;
    }

    case evDestroy:
      if (st)
        fe_unsubscribe(st->subscription_id);
      if (g_app && g_app->windows[FE_WIN_FORMS] == win)
        g_app->windows[FE_WIN_FORMS] = NULL;
      return false;

    default:
      return false;
  }
}
