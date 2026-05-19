// Project-level document management for Form Editor.

#include "formeditor.h"
#include "fe_project.h"

// ============================================================
// Document title
// ============================================================

void form_doc_update_title(form_doc_t *doc) {
  if (!doc || !doc->doc_win) return;
  const char *name = doc->form_title[0] ? doc->form_title :
                     (doc->form_id[0] ? doc->form_id : "Untitled");
  const char *slash = strrchr(name, '/');
  if (slash) name = slash + 1;
  snprintf(doc->doc_win->title, sizeof(doc->doc_win->title), "%s%s",
           name, doc->modified ? " *" : "");
  invalidate_window(doc->doc_win);
}

void form_doc_activate(form_doc_t *doc) {
  if (!g_app || !doc) return;
  if (g_app->doc == doc) return;
  form_doc_t *prev = g_app->doc;
  g_app->doc = doc;
  if (prev && prev->doc_win)
    invalidate_window(prev->doc_win);
  if (doc->doc_win)
    invalidate_window(doc->doc_win);
  fe_notify(FE_EVENT_DOCUMENT_ACTIVATED, doc);
}

void form_doc_show_only(form_doc_t *doc) {
  if (!g_app || !doc) return;
  for (form_doc_t *it = g_app->docs; it; it = it->next) {
    if (it != doc && it->doc_win && is_window(it->doc_win))
      show_window(it->doc_win, false);
  }
  form_doc_activate(doc);
  if (doc->doc_win && is_window(doc->doc_win))
    show_window(doc->doc_win, true);
}

// ============================================================
// Document window procedure
// ============================================================

result_t doc_win_proc(window_t *win, uint32_t msg,
                              uint32_t wparam, void *lparam) {
  form_doc_t *doc = (form_doc_t *)win->userdata;
  switch (msg) {
    case evCreate:
      return true;
    case evSetFocus:
      if (doc && window_has_state(win, WINDOW_STATE_VISIBLE)) form_doc_activate(doc);
      return false;
    case evPaint:
      fill_rect(get_sys_color(brWorkspaceBg), R(0, 0, win->frame.w, win->frame.h));
      return false;
    case evHScroll:
      // Forward the built-in hscroll notification to the canvas child.
      if (doc && doc->canvas_win)
        send_message(doc->canvas_win, evHScroll, wparam, lparam);
      return true;
    case evResize: {
      if (doc && doc->canvas_win) {
        irect16_t cr = get_client_rect(win);
        int new_w = MAX(1, cr.w);
        int new_h = MAX(1, cr.h);
        bool changed = (doc->form_size.w != new_w || doc->form_size.h != new_h);
        doc->form_size.w = new_w;
        doc->form_size.h = new_h;
        resize_window(doc->canvas_win, cr.w, cr.h);
        if (changed) {
          fe_doc_mark_modified(doc);
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
  int status_h = has_status ? STATUSBAR_HEIGHT : 0;
  bool needs_hscroll = form_w > max_w;
  int hstrip = (needs_hscroll && !has_status) ? SCROLLBAR_WIDTH : 0;
  int max_canvas_h = max_h - TITLEBAR_HEIGHT - status_h - hstrip;
  bool needs_vscroll;
  int frame_w;
  int frame_h;

  if (max_w < 1) max_w = 1;
  if (max_canvas_h < 1) max_canvas_h = 1;

  needs_vscroll = form_h > max_canvas_h;
  frame_w = form_w + (needs_vscroll ? SCROLLBAR_WIDTH : 0);
  if (frame_w > max_w) frame_w = max_w;

  frame_h = TITLEBAR_HEIGHT + status_h + hstrip + form_h;
  if (frame_h > max_h) frame_h = max_h;

  return (irect16_t){CW_USEDEFAULT, CW_USEDEFAULT, frame_w, frame_h};
}



// ============================================================
// Layout reflow wrapper
// ============================================================

void form_doc_auto_layout_reflow(form_doc_t *doc) {
  fe_layout_reflow(doc);
  if (doc) canvas_sync_live_controls(doc);
}

form_doc_t *create_form_doc(int w, int h) {
  if (!g_app) return NULL;
  if (w <= 0 || h <= 0 || w > INT16_MAX || h > INT16_MAX) return NULL;
  form_doc_t *prev_doc = g_app->doc;

  form_doc_t *doc = (form_doc_t *)calloc(1, sizeof(form_doc_t));
  if (!doc) return NULL;

  doc->form_size.w    = w;
  doc->form_size.h    = h;
  doc->flags     = 0;
  doc->modified  = false;
  if (fe_default_auto_layout_enabled())
    doc->flags |= WINDOW_AUTO_LAYOUT;
  doc->layout_mode = (doc->flags & WINDOW_AUTO_LAYOUT) ? 1 : 0;
  doc->flags &= ~WINDOW_STACK_HORIZONTAL;
  doc->layout_columns = 0;
  doc->layout_spacing = 4;
  doc->padding = (irect16_t){0, 0, 0, 0};
  doc->margin = (irect16_t){0, 0, 0, 0};
  doc->next_id   = CTRL_ID_BASE;
  doc->grid_size    = 8;
  doc->show_grid    = true;
  doc->snap_to_grid = true;

  // Document window
  irect16_t doc_frame = form_doc_frame_for_size(w, h, doc->flags);
  set_default_window_position(DOC_START_X, DOC_START_Y);
  window_t *dwin = create_window(
      "Untitled",
      WINDOW_HSCROLL | (doc->flags & WINDOW_STATUSBAR),
      &doc_frame,
      NULL, doc_win_proc, g_app->hinstance, NULL);
  dwin->userdata = doc;
  doc->doc_win   = dwin;

  // Canvas child window (owns the VSCROLL) — sized to the document window's client area
  irect16_t cr = get_client_rect(dwin);
  window_t *cwin = create_window(
      "", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
      MAKERECT(0, 0, cr.w, cr.h),
      dwin, win_canvas_proc, 0, doc);
  cwin->flags &= ~WINDOW_NOTABSTOP;
  doc->canvas_win = cwin;
  cr = get_client_rect(dwin);
  resize_window(cwin, cr.w, cr.h);

  doc->next = NULL;
  if (!g_app->docs) {
    g_app->docs = doc;
  } else {
    form_doc_t *tail = g_app->docs;
    while (tail->next)
      tail = tail->next;
    tail->next = doc;
  }
  g_app->doc = doc;

  show_window(dwin, true);
  if (prev_doc && prev_doc->doc_win)
    invalidate_window(prev_doc->doc_win);
  form_doc_update_title(doc);
  send_message(dwin, evStatusBar, 0, (void *)"New form");
  fe_notify(FE_EVENT_DOCUMENT_CREATED, doc);
  return doc;
}

void close_form_doc(form_doc_t *doc) {
  if (!doc) return;
  if (g_app) {
    form_doc_t **link = &g_app->docs;
    while (*link && *link != doc)
      link = &(*link)->next;
    if (*link == doc)
      *link = doc->next;
    if (g_app->doc == doc)
      g_app->doc = g_app->docs;
  }
  if (doc->doc_win && is_window(doc->doc_win))
    destroy_window(doc->doc_win);
  fe_notify(FE_EVENT_DOCUMENT_CLOSED, NULL);
  free(doc);
}
