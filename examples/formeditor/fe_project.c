// Project-level document management for Form Editor.

#include "formeditor.h"

// ============================================================
// Document title
// ============================================================

void form_doc_update_title(window_t *doc) {
  form_doc_state_t *st = fe_doc_state(doc);
  if (!doc || !st) return;
  const char *name = doc->title[0] ? doc->title : "Untitled";
  const char *slash = strrchr(name, '/');
  if (slash) name = slash + 1;
  snprintf(doc->title, sizeof(doc->title), "%s%s",
           name, st->modified ? " *" : "");
  invalidate_window(doc);
}

void form_doc_activate(window_t *doc) {
  if (!g_app || !doc) return;
  if (g_app->active_form == doc) return;
  window_t *prev = g_app->active_form;
  g_app->active_form = doc;
  if (prev)
    invalidate_window(prev);
  invalidate_window(doc);
  fe_notify(FE_EVENT_DOCUMENT_ACTIVATED, doc);
}

void form_doc_show_only(window_t *doc) {
  if (!g_app || !doc) return;
  for (int i = 0; i < g_app->form_count; i++) {
    window_t *w = g_app->forms[i];
    if (w && w != doc && is_window(w))
      show_window(w, false);
  }
  form_doc_activate(doc);
  if (is_window(doc))
    show_window(doc, true);
}

// ============================================================
// Document window procedure
// ============================================================

result_t doc_win_proc(window_t *win, uint32_t msg,
                              uint32_t wparam, void *lparam) {
  form_doc_state_t *doc = fe_doc_state(win);
  switch (msg) {
    case evCreate:
      return true;
    case evSetFocus:
      if (doc && window_has_state(win, WINDOW_STATE_VISIBLE)) form_doc_activate(win);
      return false;
    case evPaint:
      fill_rect(get_sys_color(brWorkspaceBg), R(0, 0, win->frame.w, win->frame.h));
      return false;
    case evHScroll:
      // Forward the built-in hscroll notification to the canvas child.
      if (win && win->children)
        send_message(win->children, evHScroll, wparam, lparam);
      return true;
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
      return false;
    }
    case evClose: {
      if (!doc) return false;
      show_window(win, false);
      forms_browser_refresh();
      return true;
    }
    default:
      return false;
  }
}

// ============================================================
// create_form_doc / close_form_doc
// ============================================================

irect16_t form_doc_frame_for_size(int form_w, int form_h, uint32_t form_flags) {
  int max_w = SCREEN_W - 4;
  int max_h = SCREEN_H - MENUBAR_HEIGHT - 4;
  bool has_status = (form_flags & WINDOW_STATUSBAR) != 0;
  bool has_toolbar = (form_flags & WINDOW_TOOLBAR) != 0;
  int status_h = has_status ? STATUSBAR_HEIGHT : 0;
  int toolbar_h = has_toolbar ? (TB_SPACING + 2 * (TOOLBAR_PADDING + TOOLBAR_BEVEL_WIDTH)) : 0;
  bool needs_hscroll = form_w > max_w;
  int hstrip = (needs_hscroll && !has_status) ? SCROLLBAR_WIDTH : 0;
  int max_canvas_h = max_h - TITLEBAR_HEIGHT - toolbar_h - status_h - hstrip;
  bool needs_vscroll;
  int frame_w;
  int frame_h;

  if (max_w < 1) max_w = 1;
  if (max_canvas_h < 1) max_canvas_h = 1;

  needs_vscroll = form_h > max_canvas_h;
  frame_w = form_w + (needs_vscroll ? SCROLLBAR_WIDTH : 0);
  if (frame_w > max_w) frame_w = max_w;

  frame_h = TITLEBAR_HEIGHT + toolbar_h + status_h + hstrip + form_h;
  if (frame_h > max_h) frame_h = max_h;

  return (irect16_t){CW_USEDEFAULT, CW_USEDEFAULT, frame_w, frame_h};
}



window_t *create_form_doc(int w, int h) {
  if (!g_app) return NULL;
  if (w <= 0 || h <= 0 || w > INT16_MAX || h > INT16_MAX) return NULL;
  window_t *prev_doc = g_app->active_form;
  if (g_app->form_count >= MAX_ELEMENTS)
    return NULL;

  form_doc_state_t *doc = (form_doc_state_t *)calloc(1, sizeof(form_doc_state_t));
  if (!doc)
    return NULL;

  doc->modified  = false;

  // Document window
  uint32_t doc_flags = fe_default_auto_layout_enabled() ? WINDOW_AUTO_LAYOUT : 0;
  doc_flags &= ~WINDOW_STACK_HORIZONTAL;
  irect16_t doc_frame = form_doc_frame_for_size(w, h, doc_flags);
  set_default_window_position(DOC_START_X, DOC_START_Y);
  window_t *dwin = create_window(
      "Untitled",
      WINDOW_HSCROLL | (doc_flags & (WINDOW_TOOLBAR | WINDOW_STATUSBAR)),
      &doc_frame,
      NULL, doc_win_proc, g_app->hinstance, NULL);
  dwin->userdata = doc;
  dwin->flags = (dwin->flags & ~(WINDOW_AUTO_LAYOUT | WINDOW_STACK_HORIZONTAL)) |
                (doc_flags & (WINDOW_AUTO_LAYOUT | WINDOW_STACK_HORIZONTAL));
  dwin->layout.layout_spacing = 4;
  dwin->layout.layout_padding = (irect16_t){0, 0, 0, 0};
  dwin->layout.layout_margin = (irect16_t){0, 0, 0, 0};

  // Canvas child window (owns the VSCROLL) — sized to the document window's client area
  irect16_t cr = get_client_rect(dwin);
  window_t *cwin = create_window(
      "", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
      MAKERECT(0, 0, cr.w, cr.h),
      dwin, win_canvas_proc, 0, dwin);
  cwin->flags &= ~WINDOW_NOTABSTOP;
  cr = get_client_rect(dwin);
  resize_window(cwin, cr.w, cr.h);

  g_app->forms[g_app->form_count++] = dwin;
  g_app->active_form = dwin;

  show_window(dwin, true);
  if (prev_doc)
    invalidate_window(prev_doc);
  form_doc_update_title(dwin);
  send_message(dwin, evStatusBar, 0, (void *)"New form");
  fe_notify(FE_EVENT_DOCUMENT_CREATED, dwin);
  return dwin;
}

void close_form_doc(window_t *doc) {
  if (!doc) return;
  form_doc_state_t *st = fe_doc_state(doc);
  if (g_app) {
    window_t *doc_win = doc;
    int found = -1;
    for (int i = 0; i < g_app->form_count; i++) {
      if (g_app->forms[i] == doc_win) {
        found = i;
        break;
      }
    }
    if (found >= 0) {
      for (int i = found; i + 1 < g_app->form_count; i++)
        g_app->forms[i] = g_app->forms[i + 1];
      g_app->forms[g_app->form_count - 1] = NULL;
      g_app->form_count--;
    }
    if (g_app->active_form == doc)
      g_app->active_form = (g_app->form_count > 0 && g_app->forms[0]) ? g_app->forms[0] : NULL;
  }
  if (is_window(doc)) {
    doc->userdata = NULL;
    destroy_window(doc);
  }
  fe_notify(FE_EVENT_DOCUMENT_CLOSED, NULL);
  free(st);
}
