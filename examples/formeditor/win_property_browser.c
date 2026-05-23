// VB-style property browser for the form editor.
//
// Reuses win_reportview as a two-column property grid.  Value cells edit in
// place by overlaying a win_textedit on top of the clicked report cell.

#include "formeditor.h"
#include "../../commctl/commctl.h"
#include "../../user/enum_parse.h"

enum {
  PROP_ROW_NONE = 0,
  PROP_ROW_NAME,
  PROP_ROW_CAPTION,
  PROP_ROW_TYPE,
  PROP_ROW_ID,
  PROP_ROW_LEFT,
  PROP_ROW_TOP,
  PROP_ROW_WIDTH,
  PROP_ROW_HEIGHT,
  PROP_ROW_H_ALIGN,
  PROP_ROW_V_ALIGN,
  PROP_ROW_FONT,
  // Database binding properties (NeXTSTEP DBKit style)
  PROP_ROW_DB_FIELD,
  PROP_ROW_DB_SOURCE,
  PROP_ROW_DB_DISPLAY,
  PROP_ROW_DB_VALUE,
};

typedef struct {
  window_t *list_win;
  window_t *edit_win;
  uint32_t  edit_prop_id;
  int       edit_row;
  int       subscription_id;
} prop_browser_state_t;

#define PROP_VALUE_X 72
#define PROP_HEADER_H 0

static result_t prop_edit_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam);

static const char *prop_ctrl_type_name(int type) {
  const fe_component_desc_t *c = fe_component_by_id(type);
  return c ? c->class_name : "Control";
}

static form_element_t *prop_selected_element(form_doc_t *doc) {
  if (!doc || !doc->canvas_win) return NULL;
  canvas_state_t *cs = (canvas_state_t *)doc->canvas_win->userdata;
  if (!cs || cs->selected_idx < 0 || cs->selected_idx >= doc->element_count)
    return NULL;
  return &doc->elements[cs->selected_idx];
}

static void prop_add_row(window_t *list, const char *name, const char *value,
                         uint32_t prop_id) {
  const char *subs[1] = { value ? value : "" };
  reportview_item_t item = {
    .text = name ? name : "",
    .color = get_sys_color(brTextNormal),
    .userdata = prop_id,
    .subitems = { subs[0] },
    .subitem_count = 1,
  };
  send_message(list, RVM_ADDITEM, 0, &item);
}

static bool prop_row_editable(uint32_t prop_id) {
  switch (prop_id) {
    case PROP_ROW_NAME:
    case PROP_ROW_CAPTION:
    case PROP_ROW_LEFT:
    case PROP_ROW_TOP:
    case PROP_ROW_WIDTH:
    case PROP_ROW_HEIGHT:
    case PROP_ROW_H_ALIGN:
    case PROP_ROW_V_ALIGN:
    case PROP_ROW_FONT:
    case PROP_ROW_DB_FIELD:
    case PROP_ROW_DB_SOURCE:
    case PROP_ROW_DB_DISPLAY:
    case PROP_ROW_DB_VALUE:
      return true;
    default:
      return false;
  }
}

static int prop_parse_int(const char *s, int fallback) {
  if (!s || !*s) return fallback;
  char *end = NULL;
  long v = strtol(s, &end, 10);
  if (end == s) return fallback;
  if (v < INT16_MIN) v = INT16_MIN;
  if (v > INT16_MAX) v = INT16_MAX;
  return (int)v;
}

static const enum_token_t kPropHAlignTokens[] = {
  {"stretch", LAYOUT_ALIGN_STRETCH},
  {"left",    LAYOUT_ALIGN_START},
  {"start",   LAYOUT_ALIGN_START},
  {"center",  LAYOUT_ALIGN_CENTER},
  {"right",   LAYOUT_ALIGN_END},
  {"end",     LAYOUT_ALIGN_END},
};

static const enum_token_t kPropVAlignTokens[] = {
  {"stretch", LAYOUT_ALIGN_STRETCH},
  {"top",     LAYOUT_ALIGN_START},
  {"start",   LAYOUT_ALIGN_START},
  {"center",  LAYOUT_ALIGN_CENTER},
  {"bottom",  LAYOUT_ALIGN_END},
  {"end",     LAYOUT_ALIGN_END},
};

static const enum_token_t kPropFontTokens[] = {
  {"system", FONT_SYSTEM},
  {"small",  FONT_SMALL},
  {"icon",   FONT_ICON},
};

static uint8_t prop_parse_h_align(const char *s, uint8_t fallback) {
  return (uint8_t)enum_parse_token(s, kPropHAlignTokens, ARRAY_LEN(kPropHAlignTokens), fallback);
}

static uint8_t prop_parse_v_align(const char *s, uint8_t fallback) {
  return (uint8_t)enum_parse_token(s, kPropVAlignTokens, ARRAY_LEN(kPropVAlignTokens), fallback);
}

static const char *prop_h_align_name(uint8_t align) {
  return enum_token_name(align, kPropHAlignTokens, ARRAY_LEN(kPropHAlignTokens), "stretch");
}

static const char *prop_v_align_name(uint8_t align) {
  return enum_token_name(align, kPropVAlignTokens, ARRAY_LEN(kPropVAlignTokens), "stretch");
}

static uint8_t prop_parse_font(const char *s, uint8_t fallback) {
  return (uint8_t)enum_parse_token(s, kPropFontTokens, ARRAY_LEN(kPropFontTokens), fallback);
}

static const char *prop_font_name(uint8_t font) {
  return enum_token_name(font, kPropFontTokens, ARRAY_LEN(kPropFontTokens), "small");
}

static bool prop_is_label(form_element_t *el) {
  const fe_component_desc_t *c = el ? fe_component_by_id(el->type) : NULL;
  return c && c->class_name && strcmp(c->class_name, "label") == 0;
}

static void prop_end_edit(prop_browser_state_t *pbs, bool commit) {
  if (!pbs || !pbs->edit_win)
    return;

  window_t *edit = pbs->edit_win;
  form_doc_t *doc = g_app ? g_app->doc : NULL;
  form_element_t *el = prop_selected_element(doc);
  uint32_t prop_id = pbs->edit_prop_id;
  char value[sizeof(edit->title)];
  snprintf(value, sizeof(value), "%s", edit->title);

  pbs->edit_win = NULL;
  pbs->edit_prop_id = PROP_ROW_NONE;
  pbs->edit_row = -1;
  destroy_window(edit);

  if (!commit || !doc || !el)
    return;

  switch (prop_id) {
    case PROP_ROW_NAME:
      fe_doc_set_element_name(doc, el->id, value);
      break;
    case PROP_ROW_CAPTION:
      fe_doc_set_element_text(doc, el->id, value);
      break;
    case PROP_ROW_H_ALIGN:
      fe_doc_set_element_align(doc, el->id, prop_parse_h_align(value, el->h_align), el->v_align);
      break;
    case PROP_ROW_V_ALIGN:
      fe_doc_set_element_align(doc, el->id, el->h_align, prop_parse_v_align(value, el->v_align));
      break;
    case PROP_ROW_FONT:
      fe_doc_set_element_font(doc, el->id, prop_parse_font(value, el->font));
      break;
    case PROP_ROW_LEFT:
      {
        int x = prop_parse_int(value, el->frame.x);
        if (x < 0) x = 0;
        if (x + el->frame.w > doc->form_size.w)
          x = doc->form_size.w - el->frame.w;
        if (x < 0) x = 0;
        fe_doc_set_element_frame(doc, el->id, (irect16_t){x, el->frame.y, el->frame.w, el->frame.h});
      }
      break;
    case PROP_ROW_TOP:
      {
        int y = prop_parse_int(value, el->frame.y);
        if (y < 0) y = 0;
        if (y + el->frame.h > doc->form_size.h)
          y = doc->form_size.h - el->frame.h;
        if (y < 0) y = 0;
        fe_doc_set_element_frame(doc, el->id, (irect16_t){el->frame.x, y, el->frame.w, el->frame.h});
      }
      break;
    case PROP_ROW_WIDTH:
      {
        int w = prop_parse_int(value, el->frame.w);
        if (w < 10) w = 10;
        if (el->frame.x + w > doc->form_size.w)
          w = doc->form_size.w - el->frame.x;
        if (w < 1) w = 1;
        fe_doc_set_element_frame(doc, el->id, (irect16_t){el->frame.x, el->frame.y, w, el->frame.h});
      }
      break;
    case PROP_ROW_HEIGHT:
      {
        int h = prop_parse_int(value, el->frame.h);
        if (h < 8) h = 8;
        if (el->frame.y + h > doc->form_size.h)
          h = doc->form_size.h - el->frame.y;
        if (h < 1) h = 1;
        fe_doc_set_element_frame(doc, el->id, (irect16_t){el->frame.x, el->frame.y, el->frame.w, h});
      }
      break;
    case PROP_ROW_DB_FIELD:
      fe_doc_set_element_db_field(doc, el->id, value);
      break;
    case PROP_ROW_DB_SOURCE:
      fe_doc_set_element_db_source(doc, el->id, value);
      break;
    case PROP_ROW_DB_DISPLAY:
      fe_doc_set_element_db_display(doc, el->id, value);
      break;
    case PROP_ROW_DB_VALUE:
      fe_doc_set_element_db_value(doc, el->id, value);
      break;
    default:
      return;
  }

  fe_doc_mark_modified(doc);
  canvas_sync_live_controls(doc);
  property_browser_refresh(doc);
}

static void prop_begin_edit(prop_browser_state_t *pbs, int row) {
  if (!pbs || !pbs->list_win || row < 0)
    return;

  reportview_item_t item = {0};
  if (!send_message(pbs->list_win, RVM_GETITEMDATA, (uint32_t)row, &item))
    return;
  if (!prop_row_editable(item.userdata))
    return;

  prop_end_edit(pbs, false);

  int value_x = PROP_VALUE_X;
  int y = PROP_HEADER_H
        + row * COLUMNVIEW_ENTRY_HEIGHT
        - (int)pbs->list_win->vscroll.pos;
  int value_w = pbs->list_win->frame.w - value_x
              - (pbs->list_win->vscroll.visible ? SCROLLBAR_WIDTH : 0);
  if (value_w < 20) value_w = 20;

  pbs->edit_prop_id = item.userdata;
  pbs->edit_row = row;
  if (item.userdata == PROP_ROW_FONT) {
    static const char *kFontItems[] = { "system", "small", "icon" };
    pbs->edit_win = create_window(
        item.subitem_count > 0 && item.subitems[0] ? item.subitems[0] : "",
        WINDOW_NOTITLE,
        MAKERECT(value_x, y, value_w, COLUMNVIEW_ENTRY_HEIGHT),
        pbs->list_win, win_combobox, 0, NULL);
    if (!pbs->edit_win)
      return;
    pbs->edit_win->id = 1;
    pbs->edit_win->userdata = pbs;
    send_message(pbs->edit_win, cbClear, 0, NULL);
    for (size_t i = 0; i < ARRAY_LEN(kFontItems); i++)
      send_message(pbs->edit_win, cbAddString, 0, (void *)kFontItems[i]);
    send_message(pbs->edit_win, cbSetCurrentSelection,
                 (uint32_t)prop_parse_font(item.subitems[0], FONT_SMALL), NULL);
    resize_window(pbs->edit_win, value_w, COLUMNVIEW_ENTRY_HEIGHT);
    set_focus(pbs->edit_win);
    return;
  }

  pbs->edit_win = create_window(
      item.subitem_count > 0 && item.subitems[0] ? item.subitems[0] : "",
      WINDOW_NOTITLE,
      MAKERECT(value_x, y, value_w, COLUMNVIEW_ENTRY_HEIGHT),
      pbs->list_win, prop_edit_proc, 0, NULL);
  if (!pbs->edit_win)
    return;

  pbs->edit_win->id = 1;
  pbs->edit_win->userdata = pbs;
  resize_window(pbs->edit_win, value_w, COLUMNVIEW_ENTRY_HEIGHT);
  window_set_state(pbs->edit_win, WINDOW_STATE_EDITING, true);
  pbs->edit_win->cursor_pos = (int)strlen(pbs->edit_win->title);
  set_focus(pbs->edit_win);
}

static void prop_fill_for_element(window_t *list, form_element_t *el) {
  char buf[32];

  prop_add_row(list, "(Name)", el->name, PROP_ROW_NAME);
  prop_add_row(list, "Caption", el->text, PROP_ROW_CAPTION);
  prop_add_row(list, "Type", prop_ctrl_type_name(el->type), PROP_ROW_TYPE);
  if (prop_is_label(el))
    prop_add_row(list, "Font", prop_font_name(el->font), PROP_ROW_FONT);

  snprintf(buf, sizeof(buf), "%d", el->id);
  prop_add_row(list, "ID", buf, PROP_ROW_ID);
  
  // Database binding properties
  prop_add_row(list, "Database Field", el->db_field, PROP_ROW_DB_FIELD);
  prop_add_row(list, "Database Source", el->db_source, PROP_ROW_DB_SOURCE);
  prop_add_row(list, "Database Display", el->db_display, PROP_ROW_DB_DISPLAY);
  prop_add_row(list, "Database Value", el->db_value, PROP_ROW_DB_VALUE);
  
  if (g_app && g_app->doc && (g_app->doc->flags & WINDOW_AUTO_LAYOUT)) {
    prop_add_row(list, "Horizontal alignment", prop_h_align_name(el->h_align), PROP_ROW_H_ALIGN);
    prop_add_row(list, "Vertical alignment", prop_v_align_name(el->v_align), PROP_ROW_V_ALIGN);
  } else {
    snprintf(buf, sizeof(buf), "%d", el->frame.x);
    prop_add_row(list, "Left", buf, PROP_ROW_LEFT);
    snprintf(buf, sizeof(buf), "%d", el->frame.y);
    prop_add_row(list, "Top", buf, PROP_ROW_TOP);
    snprintf(buf, sizeof(buf), "%d", el->frame.w);
    prop_add_row(list, "Width", buf, PROP_ROW_WIDTH);
    snprintf(buf, sizeof(buf), "%d", el->frame.h);
    prop_add_row(list, "Height", buf, PROP_ROW_HEIGHT);
  }
}

void property_browser_refresh(form_doc_t *doc) {
  if (!g_app || !g_app->prop_win || !is_window(g_app->prop_win))
    return;
  prop_browser_state_t *pbs = (prop_browser_state_t *)g_app->prop_win->userdata;
  if (!pbs || !pbs->list_win)
    return;

  prop_end_edit(pbs, false);

  window_t *list = pbs->list_win;
  send_message(list, RVM_SETREDRAW, 0, NULL);
  send_message(list, RVM_CLEAR, 0, NULL);

  form_element_t *el = prop_selected_element(doc);
  if (el) {
    prop_fill_for_element(list, el);
  } else {
    prop_add_row(list, "Selection", "(None)", PROP_ROW_NONE);
  }

  send_message(list, RVM_SETREDRAW, 1, NULL);
}

static void property_browser_observer(fe_event_type_t event, form_doc_t *doc, void *ctx) {
  (void)ctx;
  switch (event) {
    case FE_EVENT_DOCUMENT_ACTIVATED:
    case FE_EVENT_DOCUMENT_MODIFIED:
    case FE_EVENT_SELECTION_CHANGED:
    case FE_EVENT_ELEMENT_MODIFIED:
      property_browser_refresh(doc);
      break;
    default:
      break;
  }
}

window_t *property_browser_create(hinstance_t hinstance) {
  window_t *win = create_window(
      "Properties",
      WINDOW_NOTRAYBUTTON | WINDOW_NORESIZE,
      MAKERECT(PROPBROWSER_WIN_X, PROPBROWSER_WIN_Y,
               PROPBROWSER_WIN_W, PROPBROWSER_WIN_H),
      NULL, win_property_browser_proc, hinstance, NULL);
  if (win)
    show_window(win, true);
  return win;
}

result_t win_property_browser_proc(window_t *win, uint32_t msg,
                                    uint32_t wparam, void *lparam) {
  prop_browser_state_t *pbs = (prop_browser_state_t *)win->userdata;

  switch (msg) {
    case evCreate: {
      pbs = allocate_window_data(win, sizeof(prop_browser_state_t));
      irect16_t cr = get_client_rect(win);
      pbs->list_win = create_window(
          "", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
          MAKERECT(0, 0, cr.w, cr.h),
          win, win_reportview, 0, NULL);
      if (!pbs->list_win)
        return false;

      send_message(pbs->list_win, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
      send_message(pbs->list_win, RVM_SETCOLUMNTITLESVISIBLE, 0, NULL);
      reportview_column_t c0 = { "Property", 72 };
      reportview_column_t c1 = { "Value", 0 };
      send_message(pbs->list_win, RVM_ADDCOLUMN, 0, &c0);
      send_message(pbs->list_win, RVM_ADDCOLUMN, 0, &c1);
      pbs->subscription_id = fe_subscribe(property_browser_observer, win);
      property_browser_refresh(g_app ? g_app->doc : NULL);
      return true;
    }

    case evDestroy:
      if (pbs)
        fe_unsubscribe(pbs->subscription_id);
      if (g_app && g_app->prop_win == win)
        g_app->prop_win = NULL;
      return false;

    case evResize:
      if (pbs && pbs->list_win) {
        irect16_t cr = get_client_rect(win);
        resize_window(pbs->list_win, cr.w, cr.h);
      }
      return false;

    case evCommand: {
      uint16_t notif = HIWORD(wparam);
      if (!pbs)
        return false;

      if (lparam == pbs->edit_win && (notif == edUpdate || notif == cbSelectionChange)) {
        prop_end_edit(pbs, true);
        return true;
      }

      if (lparam != pbs->list_win || notif != RVN_DBLCLK)
        return false;

      reportview_item_t item = {0};
      if (!send_message(pbs->list_win, RVM_GETITEMDATA, LOWORD(wparam), &item))
        return false;
      if (prop_row_editable(item.userdata))
        prop_begin_edit(pbs, (int)LOWORD(wparam));
      return true;
    }

    case evParentNotify: {
      if (!pbs || !lparam) return false;
      parent_notify_t *pn = (parent_notify_t *)lparam;
      if (pn->child != pbs->list_win)
        return false;

      if (pn->child_msg == evLeftButtonDown ||
          pn->child_msg == evLeftButtonDoubleClick) {
        int lx = (int16_t)LOWORD(pn->child_wparam);
        int ly = (int16_t)HIWORD(pn->child_wparam);
        if (lx < PROP_VALUE_X || ly < PROP_HEADER_H)
          return false;
        int row = (ly - PROP_HEADER_H) / COLUMNVIEW_ENTRY_HEIGHT;
        prop_begin_edit(pbs, row);
        return true;
      }
      return false;
    }

    default:
      return false;
  }
}

static result_t prop_edit_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam) {
  prop_browser_state_t *pbs = (prop_browser_state_t *)win->userdata;

  switch (msg) {
    case evDestroy:
      if (pbs && pbs->edit_win == win) {
        pbs->edit_win = NULL;
        pbs->edit_prop_id = PROP_ROW_NONE;
        pbs->edit_row = -1;
      }
      return win_textedit(win, msg, wparam, lparam);

    case evKillFocus:
      prop_end_edit(pbs, true);
      return true;

    case evKeyDown:
      if (wparam == AX_KEY_ESCAPE) {
        prop_end_edit(pbs, false);
        return true;
      }
      if (wparam == AX_KEY_ENTER) {
        // Handle Enter directly: do NOT delegate to win_textedit, which would
        // write to win->editing and call invalidate_window(win) AFTER
        // prop_end_edit -> destroy_window frees the window (use-after-free).
        prop_end_edit(pbs, true);
        return true;
      }
      return win_textedit(win, msg, wparam, lparam);

    default:
      return win_textedit(win, msg, wparam, lparam);
  }
}
