// Menu bar, document management, file I/O, and dialog entry points
// for the Orion Form Editor.

#include "formeditor.h"
#include "../../commctl/commctl.h"
#include "../../user/enum_parse.h"
#include <ctype.h>
#include <inttypes.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

// ============================================================
// Menu definitions
// ============================================================

static const menu_item_t kFileItems[] = {
  {"New",        ID_FILE_NEW},
  {"Open...",    ID_FILE_OPEN},
  {NULL,         0},
  {"Save",       ID_FILE_SAVE},
  {"Save As...", ID_FILE_SAVEAS},
  {NULL,         0},
  {"Quit",       ID_FILE_QUIT},
};

static const menu_item_t kEditItems[] = {
  {"Delete",            ID_EDIT_DELETE},
  {NULL,                0},
  {"Properties...",     ID_EDIT_PROPS},
};

static const menu_item_t kViewItems[] = {
  {"Grid Settings...", ID_VIEW_GRID},
};

static const menu_item_t kHelpItems[] = {
  {"About...", ID_HELP_ABOUT},
};

menu_def_t kMenus[] = {
  {"File", kFileItems, (int)(sizeof(kFileItems)/sizeof(kFileItems[0]))},
  {"Edit", kEditItems, (int)(sizeof(kEditItems)/sizeof(kEditItems[0]))},
  {"View", kViewItems, (int)(sizeof(kViewItems)/sizeof(kViewItems[0]))},
  {"Help", kHelpItems, (int)(sizeof(kHelpItems)/sizeof(kHelpItems[0]))},
};
const int kNumMenus = (int)(sizeof(kMenus)/sizeof(kMenus[0]));

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
  property_browser_refresh(doc);
  forms_browser_refresh();
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
  forms_browser_refresh();
}

// ============================================================
// Document window procedure
// ============================================================

static result_t doc_win_proc(window_t *win, uint32_t msg,
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
  property_browser_refresh(doc);
  forms_browser_refresh();
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
  property_browser_refresh(g_app ? g_app->doc : NULL);
  forms_browser_refresh();
  free(doc);
}

// ============================================================
// About dialog
// ============================================================

#define ABOUT_W 220
#define ABOUT_H  80

static result_t about_proc(window_t *win, uint32_t msg,
                            uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      win->userdata = lparam;
      return true;
    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        end_dialog(win, 1);
        return true;
      }
      return false;
    default:
      return false;
  }
}

enum {
  ABOUT_ID_OK = 1,
};

static const form_ctrl_def_t kAboutChildren[] = {
  { .class_name = "label", .text = "Orion Form Editor", .name = "title",
    .h_align = LAYOUT_ALIGN_STRETCH, .font = FONT_SYSTEM, .font_set = true },
  { .class_name = "label", .text = "Version 1.0", .name = "version",
    .h_align = LAYOUT_ALIGN_STRETCH, .color = brTextDisabled, .color_set = true },
  { .class_name = "label", .text = "VB3-inspired form designer", .name = "desc",
    .h_align = LAYOUT_ALIGN_STRETCH, .color = brTextDisabled, .color_set = true },
  {
    .class_name = "stack",
    .name = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
      { .class_name = "button", .id = ABOUT_ID_OK, .size = {50, BUTTON_HEIGHT},
        .flags = BUTTON_DEFAULT, .text = "OK", .name = "ok", .h_align = LAYOUT_ALIGN_START },
    },
    .child_count = 2,
  },
};

static const form_def_t kAboutForm = {
  .name = "About Orion Form Editor",
  .flags = WINDOW_AUTO_LAYOUT,
  .width = ABOUT_W,
  .height = ABOUT_H,
  .layout_spacing = 6,
  .padding = {8, 8, 8, 8},
  .children = kAboutChildren,
  .child_count = ARRAY_LEN(kAboutChildren),
  .ok_id = ABOUT_ID_OK,
};

void show_about_dialog(window_t *parent) {
  show_dialog_from_form(&kAboutForm, "About Orion Form Editor",
                        parent, about_proc, NULL);
}

// ============================================================
// Grid Settings dialog
// ============================================================

#define GRID_W   180
#define GRID_H   108

#define GRID_ROW1_Y   6
#define GRID_ROW2_Y   (GRID_ROW1_Y + BUTTON_HEIGHT + 4)
#define GRID_ROW3_Y   (GRID_ROW2_Y + BUTTON_HEIGHT + 4)
#define GRID_BTN_Y    (GRID_H - BUTTON_HEIGHT - 6)

#define GRID_ID_SHOW   1
#define GRID_ID_SNAP   2
#define GRID_ID_SIZE   3
#define GRID_ID_OK     4
#define GRID_ID_CANCEL 5

#define GRID_SIZE_MIN  1
#define GRID_SIZE_MAX  64

// grid_size is bound via DDX_TEXT; checkboxes are handled manually.
typedef struct {
  int  grid_size;
} grid_size_data_t;

static const form_ctrl_def_t kGridChildren[] = {
  { .class_name = "checkbox", .id = GRID_ID_SHOW, .text = "Show grid", .name = "chk_show",
    .h_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "checkbox", .id = GRID_ID_SNAP, .text = "Snap to grid", .name = "chk_snap",
    .h_align = LAYOUT_ALIGN_STRETCH },
  {
    .class_name = "stack",
    .name = "size_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "label", .text = "Grid size:", .name = "lbl_size", .size = {60, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "textedit", .id = GRID_ID_SIZE, .text = "", .name = "edit_size", .size = {40, BUTTON_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
    },
    .child_count = 2,
  },
  {
    .class_name = "stack",
    .name = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 4,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
      { .class_name = "button", .id = GRID_ID_OK, .size = {50, BUTTON_HEIGHT},
        .flags = BUTTON_DEFAULT, .text = "OK", .name = "btn_ok", .h_align = LAYOUT_ALIGN_START },
      { .class_name = "button", .id = GRID_ID_CANCEL, .size = {50, BUTTON_HEIGHT},
        .text = "Cancel", .name = "btn_cancel", .h_align = LAYOUT_ALIGN_START },
    },
    .child_count = 3,
  },
};

static const ctrl_binding_t k_grid_bindings[] = {
  DDX_TEXT(GRID_ID_SIZE, grid_size_data_t, grid_size),
};

static const form_def_t kGridForm = {
  .name          = "Grid Settings",
  .width         = GRID_W,
  .height        = GRID_H,
  .flags = (0) | WINDOW_AUTO_LAYOUT,
  .layout_spacing = 6,
  .padding       = {8, 8, 8, 8},
  .children      = kGridChildren,
  .child_count   = ARRAY_LEN(kGridChildren),
  .bindings      = k_grid_bindings,
  .binding_count = ARRAY_LEN(k_grid_bindings),
  .ok_id         = GRID_ID_OK,
  .cancel_id     = GRID_ID_CANCEL,
};

typedef struct {
  form_doc_t *doc;
  bool        accepted;
} grid_dlg_state_t;

static result_t grid_dlg_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam) {
  grid_dlg_state_t *gs = (grid_dlg_state_t *)win->userdata;
  switch (msg) {
    case evCreate: {
      gs = (grid_dlg_state_t *)lparam;
      win->userdata = gs;
      form_doc_t *doc = gs->doc;
      // Set checkbox states manually (no checkbox DDX helper yet).
      window_t *chk_show = get_window_item(win, GRID_ID_SHOW);
      window_t *chk_snap = get_window_item(win, GRID_ID_SNAP);
      if (chk_show)
        send_message(chk_show, btnSetCheck,
                     doc->show_grid ? btnStateChecked : btnStateUnchecked, NULL);
      if (chk_snap)
        send_message(chk_snap, btnSetCheck,
                     doc->snap_to_grid ? btnStateChecked : btnStateUnchecked, NULL);
      // Push grid_size via DDX.
      grid_size_data_t gsd = { doc->grid_size };
      dialog_push(win, &gsd, k_grid_bindings, ARRAY_LEN(k_grid_bindings));
      return true;
    }
    case evCommand: {
      if (HIWORD(wparam) != btnClicked) return false;
      window_t *src = (window_t *)lparam;
      if (!src) return false;
      if (src->id == GRID_ID_OK) {
        form_doc_t *doc = gs->doc;
        // Pull grid_size via DDX.
        grid_size_data_t gsd = { doc->grid_size };
        dialog_pull(win, &gsd, k_grid_bindings, ARRAY_LEN(k_grid_bindings));
        if (gsd.grid_size < GRID_SIZE_MIN) gsd.grid_size = GRID_SIZE_MIN;
        if (gsd.grid_size > GRID_SIZE_MAX) gsd.grid_size = GRID_SIZE_MAX;
        doc->grid_size = gsd.grid_size;
        // Pull checkboxes manually.
        window_t *chk_show = get_window_item(win, GRID_ID_SHOW);
        window_t *chk_snap = get_window_item(win, GRID_ID_SNAP);
        if (chk_show)
          doc->show_grid = (send_message(chk_show, btnGetCheck, 0, NULL) == btnStateChecked);
        if (chk_snap)
          doc->snap_to_grid = (send_message(chk_snap, btnGetCheck, 0, NULL) == btnStateChecked);
        gs->accepted = true;
        end_dialog(win, 1);
        return true;
      }
      if (src->id == GRID_ID_CANCEL) {
        end_dialog(win, 0);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

static void show_grid_settings_dialog(window_t *parent, form_doc_t *doc) {
  grid_dlg_state_t gs = { doc, false };
  show_dialog_from_form(&kGridForm, "Grid Settings", parent, grid_dlg_proc, &gs);
  if (gs.accepted && doc->canvas_win)
    invalidate_window(doc->canvas_win);
}

// ============================================================
// Properties dialog
// ============================================================

#define PROPS_W  260
#define PROPS_H  110

// Child IDs
#define PROPS_ID_CAPTION   1
#define PROPS_ID_NAME      2
#define PROPS_ID_OK        3
#define PROPS_ID_CANCEL    4

// Computed row positions (mirrors the form below)
#define PROPS_ROW1_Y       4
#define PROPS_ROW2_Y       (PROPS_ROW1_Y + BUTTON_HEIGHT + 6)   // 23
#define PROPS_INFO_Y       (PROPS_ROW2_Y + BUTTON_HEIGHT + 6)   // 42
#define PROPS_BTN_Y        (PROPS_H - BUTTON_HEIGHT - 6)        // 86

static const form_ctrl_def_t kPropsChildren[] = {
  {
    .class_name = "grid",
    .name = "fields",
    .flags = WINDOW_FLEXSPACE,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      {
        .class_name = "column",
        .name = "labels",
        .size = {60, 0},
        .children = (const form_ctrl_def_t[]){
          { .class_name = "label", .text = "Caption:", .name = "lbl_caption", .size = {60, CONTROL_HEIGHT},
            .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
          { .class_name = "label", .text = "Name:", .name = "lbl_name", .size = {60, CONTROL_HEIGHT},
            .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
        },
        .child_count = 2,
      },
      {
        .class_name = "column",
        .name = "inputs",
        .flags = WINDOW_FLEXSPACE,
        .children = (const form_ctrl_def_t[]){
          { .class_name = "textedit", .id = PROPS_ID_CAPTION, .text = "", .name = "edit_caption",
            .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
          { .class_name = "textedit", .id = PROPS_ID_NAME, .text = "", .name = "edit_name",
            .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
        },
        .child_count = 2,
      },
    },
    .child_count = 2,
  },
  {
    .class_name = "stack",
    .name = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 4,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
      { .class_name = "button", .id = PROPS_ID_OK, .size = {50, BUTTON_HEIGHT},
        .flags = BUTTON_DEFAULT, .text = "OK", .name = "btn_ok", .h_align = LAYOUT_ALIGN_START },
      { .class_name = "button", .id = PROPS_ID_CANCEL, .size = {50, BUTTON_HEIGHT},
        .text = "Cancel", .name = "btn_cancel", .h_align = LAYOUT_ALIGN_START },
    },
    .child_count = 3,
  },
};

// DDX bindings: caption and name edits ↔ form_element_t.text / .name
static const ctrl_binding_t k_props_bindings[] = {
  DDX_TEXT(PROPS_ID_CAPTION, form_element_t, text),
  DDX_TEXT(PROPS_ID_NAME, form_element_t, name),
};

static const form_def_t kPropsForm = {
  .name          = "Element Properties",
  .width         = PROPS_W,
  .height        = PROPS_H,
  .flags = (0) | WINDOW_AUTO_LAYOUT,
  .layout_spacing = 6,
  .padding       = {8, 8, 8, 8},
  .children      = kPropsChildren,
  .child_count   = ARRAY_LEN(kPropsChildren),
  .bindings      = k_props_bindings,
  .binding_count = ARRAY_LEN(k_props_bindings),
  .ok_id         = PROPS_ID_OK,
  .cancel_id     = PROPS_ID_CANCEL,
};

// ============================================================
// Form Properties dialog
// ============================================================

#define FORM_PROPS_W  220
#define FORM_PROPS_H   158

#define FORM_PROPS_ID_AUTO   1
#define FORM_PROPS_ID_KIND    2
#define FORM_PROPS_ID_ORIENT  3
#define FORM_PROPS_ID_COLUMNS 4
#define FORM_PROPS_ID_OK      5
#define FORM_PROPS_ID_CANCEL  6

#define FORM_PROPS_ROW1_Y      10
#define FORM_PROPS_ROW2_Y      34
#define FORM_PROPS_ROW3_Y      58
#define FORM_PROPS_ROW4_Y      82
#define FORM_PROPS_BTN_Y       (FORM_PROPS_H - BUTTON_HEIGHT - 6)

typedef struct {
  bool auto_layout_enabled;
  int  layout_mode;
  int  layout_orientation;
  char layout_columns[8];
  bool accepted;
} form_props_state_t;

static void form_props_fill_layout_combos(window_t *win) {
  static const char *const kKindItems[] = { "None", "Stack", "Grid" };
  static const char *const kOrientItems[] = { "Vertical", "Horizontal" };
  window_t *kind = get_window_item(win, FORM_PROPS_ID_KIND);
  window_t *orient = get_window_item(win, FORM_PROPS_ID_ORIENT);
  if (kind) {
    send_message(kind, cbClear, 0, NULL);
    for (size_t i = 0; i < ARRAY_LEN(kKindItems); i++)
      send_message(kind, cbAddString, 0, (void *)kKindItems[i]);
  }
  if (orient) {
    send_message(orient, cbClear, 0, NULL);
    for (size_t i = 0; i < ARRAY_LEN(kOrientItems); i++)
      send_message(orient, cbAddString, 0, (void *)kOrientItems[i]);
  }
}

static const ctrl_binding_t k_form_props_bindings[] = {
  DDX_CHECK(FORM_PROPS_ID_AUTO, form_props_state_t, auto_layout_enabled),
  DDX_COMBO(FORM_PROPS_ID_KIND, form_props_state_t, layout_mode, 0),
  DDX_COMBO(FORM_PROPS_ID_ORIENT, form_props_state_t, layout_orientation, WINDOW_STACK_VERTICAL),
  DDX_TEXT(FORM_PROPS_ID_COLUMNS, form_props_state_t, layout_columns),
};

static const form_ctrl_def_t kFormPropsChildren[] = {
  { .class_name = "checkbox", .id = FORM_PROPS_ID_AUTO, .text = "Use auto layout", .name = "chk_auto",
    .h_align = LAYOUT_ALIGN_STRETCH },
  {
    .class_name = "stack",
    .name = "kind_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "label", .text = "Layout:", .name = "lbl_kind", .size = {72, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "combobox", .id = FORM_PROPS_ID_KIND, .text = "", .name = "combo_kind", .size = {124, BUTTON_HEIGHT + 2},
        .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER, .flags = WINDOW_FLEXSPACE },
    },
    .child_count = 2,
  },
  {
    .class_name = "stack",
    .name = "orient_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "label", .text = "Orientation:", .name = "lbl_orient", .size = {72, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "combobox", .id = FORM_PROPS_ID_ORIENT, .text = "", .name = "combo_orient", .size = {124, BUTTON_HEIGHT + 2},
        .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER, .flags = WINDOW_FLEXSPACE },
    },
    .child_count = 2,
  },
  {
    .class_name = "stack",
    .name = "columns_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "label", .text = "Columns:", .name = "lbl_columns", .size = {72, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "textedit", .id = FORM_PROPS_ID_COLUMNS, .text = "", .name = "edit_columns", .size = {64, BUTTON_HEIGHT + 2},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
    },
    .child_count = 2,
  },
  {
    .class_name = "stack",
    .name = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 4,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
      { .class_name = "button", .id = FORM_PROPS_ID_OK, .size = {50, BUTTON_HEIGHT},
        .flags = BUTTON_DEFAULT, .text = "OK", .name = "btn_ok", .h_align = LAYOUT_ALIGN_START },
      { .class_name = "button", .id = FORM_PROPS_ID_CANCEL, .size = {50, BUTTON_HEIGHT},
        .text = "Cancel", .name = "btn_cancel", .h_align = LAYOUT_ALIGN_START },
    },
    .child_count = 3,
  },
};

static const form_def_t kFormPropsForm = {
  .name          = "Form Properties",
  .width         = FORM_PROPS_W,
  .height        = FORM_PROPS_H,
  .flags = (0) | WINDOW_AUTO_LAYOUT,
  .layout_spacing = 6,
  .padding       = {8, 8, 8, 8},
  .children      = kFormPropsChildren,
  .child_count   = ARRAY_LEN(kFormPropsChildren),
  .bindings      = k_form_props_bindings,
  .binding_count = ARRAY_LEN(k_form_props_bindings),
  .ok_id         = FORM_PROPS_ID_OK,
  .cancel_id     = FORM_PROPS_ID_CANCEL,
};

static result_t form_props_proc(window_t *win, uint32_t msg,
                                uint32_t wparam, void *lparam) {
  form_props_state_t *ps = (form_props_state_t *)win->userdata;
  switch (msg) {
    case evCreate:
      ps = (form_props_state_t *)lparam;
      win->userdata = ps;
      if (ps && g_app && g_app->doc) {
        form_props_fill_layout_combos(win);
        dialog_push(win, ps, k_form_props_bindings, ARRAY_LEN(k_form_props_bindings));
      }
      return true;
    case evCommand: {
      if (HIWORD(wparam) != btnClicked || !ps) return false;
      window_t *src = (window_t *)lparam;
      if (!src) return false;
      if (src->id == FORM_PROPS_ID_OK) {
        if (g_app && g_app->doc) {
          form_doc_t *doc = g_app->doc;
          bool old_auto_layout = (doc->flags & WINDOW_AUTO_LAYOUT) != 0;
          uint8_t old_kind = doc->layout_mode;
          flags_t old_orient = doc->flags & WINDOW_STACK_HORIZONTAL;
          uint8_t old_columns = doc->layout_columns;
          dialog_pull(win, ps, k_form_props_bindings, ARRAY_LEN(k_form_props_bindings));
          if (ps->auto_layout_enabled)
            doc->flags |= WINDOW_AUTO_LAYOUT;
          else
            doc->flags &= ~WINDOW_AUTO_LAYOUT;
          doc->layout_mode = (uint8_t)ps->layout_mode;
          if (ps->layout_orientation & WINDOW_STACK_HORIZONTAL)
            doc->flags |= WINDOW_STACK_HORIZONTAL;
          else
            doc->flags &= ~WINDOW_STACK_HORIZONTAL;
          {
            int cols = atoi(ps->layout_columns);
            if (cols < 0) cols = 0;
            if (cols > 255) cols = 255;
            doc->layout_columns = (uint8_t)cols;
          }
          if (((doc->flags & WINDOW_AUTO_LAYOUT) != 0) != old_auto_layout ||
              doc->layout_mode != old_kind ||
              (doc->flags & WINDOW_STACK_HORIZONTAL) != old_orient ||
              doc->layout_columns != old_columns) {
            fe_doc_mark_modified(doc);
          }
          if (doc->flags & WINDOW_AUTO_LAYOUT) {
            form_doc_auto_layout_reflow(doc);
            canvas_rebuild_live_controls(doc);
          }
        }
        ps->accepted = true;
        end_dialog(win, 1);
        return true;
      }
      if (src->id == FORM_PROPS_ID_CANCEL) {
        end_dialog(win, 0);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

static bool show_form_props_dialog(window_t *parent, form_doc_t *doc) {
  if (!doc) return false;
  form_props_state_t st = {
    .auto_layout_enabled = (doc->flags & WINDOW_AUTO_LAYOUT) != 0,
    .layout_mode = doc->layout_mode,
    .layout_orientation = (doc->flags & WINDOW_STACK_HORIZONTAL) ? WINDOW_STACK_HORIZONTAL : WINDOW_STACK_VERTICAL,
  };
  snprintf(st.layout_columns, sizeof(st.layout_columns), "%u",
           (unsigned)doc->layout_columns);
  show_dialog_from_form(&kFormPropsForm, "Form Properties", parent, form_props_proc, &st);
  return st.accepted;
}

typedef struct {
  form_element_t *el;
  bool            accepted;
} props_state_t;

static result_t props_proc(window_t *win, uint32_t msg,
                            uint32_t wparam, void *lparam) {
  props_state_t *ps = (props_state_t *)win->userdata;
  switch (msg) {
    case evCreate: {
      ps = (props_state_t *)lparam;
      win->userdata = ps;

      // Dynamic type-info label (content is computed at runtime).
      char info[64];
      snprintf(info, sizeof(info), "Type: %s  ID: %d  (%d, %d)  %d x %d",
               ctrl_type_token(ps->el->type), ps->el->id,
               ps->el->frame.x, ps->el->frame.y, ps->el->frame.w, ps->el->frame.h);
      create_window(info, WINDOW_NOTITLE | WINDOW_NOFILL,
          MAKERECT(4, PROPS_INFO_Y, PROPS_W - 8, CONTROL_HEIGHT),
          win, "label", 0, (void *)(uintptr_t)brTextDisabled);

      // Pre-populate caption/name edits from the element.
      dialog_push(win, ps->el, k_props_bindings, ARRAY_LEN(k_props_bindings));
      return true;
    }

    case evCommand: {
      if (HIWORD(wparam) != btnClicked) return false;
      window_t *src = (window_t *)lparam;
      if (!src) return false;

      if (src->id == PROPS_ID_OK) {
        dialog_pull(win, ps->el, k_props_bindings, ARRAY_LEN(k_props_bindings));
        ps->accepted = true;
        end_dialog(win, 1);
        return true;
      }
      if (src->id == PROPS_ID_CANCEL) {
        end_dialog(win, 0);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

bool show_props_dialog(window_t *parent, form_element_t *el) {
  props_state_t ps = {0};
  ps.el       = el;
  ps.accepted = false;
  show_dialog_from_form(&kPropsForm, "Element Properties", parent, props_proc, &ps);
  return ps.accepted;
}

// ============================================================
// File-picker wrapper (analogous to imageeditor/filepicker.c)
// ============================================================

static bool show_form_file_picker(window_t *parent, bool save_mode,
                                   char *out_path, size_t out_sz) {
  openfilename_t ofn = {0};
  ofn.lStructSize  = sizeof(ofn);
  ofn.hwndOwner    = parent;
  ofn.lpstrFile    = out_path;
  ofn.nMaxFile     = (uint32_t)out_sz;
  ofn.lpstrFilter  = "Orion Projects\0*.orion\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags        = save_mode ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST;
  return save_mode ? get_save_filename(&ofn) : get_open_filename(&ofn);
}

// ============================================================
// Menu command handler
// ============================================================

void handle_menu_command(uint16_t id) {
  if (!g_app) return;
  form_doc_t *doc = g_app->doc;

  switch (id) {
    case ID_FILE_NEW:
      create_form_doc(FORM_DEFAULT_W, FORM_DEFAULT_H);
      break;

    case ID_FILE_OPEN: {
      char path[512] = {0};
      window_t *owner = doc ? doc->doc_win : (g_app->menubar_win);
      if (show_form_file_picker(owner, false, path, sizeof(path))) {
        if (!fe_project_load(path) && owner)
          message_box(owner, "Failed to load Orion project.", "Open", MB_OK);
      }
      break;
    }

    case ID_FILE_SAVE:
      if (g_app->project.loaded && g_app->project.filename[0]) {
        if (fe_project_save(g_app->project.filename)) {
          if (doc && doc->doc_win)
            send_message(doc->doc_win, evStatusBar, 0, (void *)"Project saved");
        } else if (doc && doc->doc_win) {
          send_message(doc->doc_win, evStatusBar, 0, (void *)"Project save failed");
        }
      } else {
        goto do_save_as;
      }
      break;

    do_save_as:
    case ID_FILE_SAVEAS: {
      if (!doc && !g_app->docs) break;
      char path[512] = {0};
      window_t *owner = doc ? doc->doc_win : g_app->menubar_win;
      if (show_form_file_picker(owner, true, path, sizeof(path))) {
        if (fe_project_save(path)) {
          if (doc && doc->doc_win)
            send_message(doc->doc_win, evStatusBar, 0, path);
        } else {
          if (doc && doc->doc_win)
            send_message(doc->doc_win, evStatusBar, 0, (void *)"Project save failed");
        }
      }
      break;
    }

    case ID_FILE_QUIT:
#ifdef BUILD_AS_GEM
      if (g_app) {
        while (g_app->docs)
          close_form_doc(g_app->docs);
        if (g_app->tool_win)    destroy_window(g_app->tool_win);
        if (g_app->menubar_win) destroy_window(g_app->menubar_win);
      }
#else
      ui_request_quit();
#endif
      break;

    case ID_EDIT_DELETE: {
      if (!doc) break;
      window_t *cwin = doc->canvas_win;
      if (!cwin) break;
      canvas_state_t *cs = (canvas_state_t *)cwin->userdata;
      if (!cs || cs->selected_idx < 0) break;
      int idx = cs->selected_idx;
      if (doc->elements[idx].live_win)
        destroy_window(doc->elements[idx].live_win);
      // Remove element by shifting the array
      for (int i = idx; i < doc->element_count - 1; i++)
        doc->elements[i] = doc->elements[i + 1];
      doc->element_count--;
      cs->selected_idx = -1;
      fe_doc_mark_modified(doc);
      canvas_rebuild_live_controls(doc);
      break;
    }

    case ID_EDIT_PROPS: {
      if (!doc) break;
      window_t *cwin = doc->canvas_win;
      if (!cwin) break;
      canvas_state_t *cs = (canvas_state_t *)cwin->userdata;
      window_t *owner = g_app->menubar_win ? g_app->menubar_win : doc->doc_win;
      if (!cs || cs->selected_idx < 0) {
        show_form_props_dialog(owner, doc);
      } else {
        form_element_t *el = &doc->elements[cs->selected_idx];
        if (show_props_dialog(owner, el)) {
          fe_doc_mark_modified(doc);
          property_browser_refresh(doc);
        }
      }
      break;
    }

    case ID_VIEW_GRID: {
      if (!doc) break;
      window_t *owner = g_app->menubar_win ? g_app->menubar_win : doc->doc_win;
      show_grid_settings_dialog(owner, doc);
      break;
    }

    case ID_HELP_ABOUT: {
      window_t *owner = g_app->menubar_win ? g_app->menubar_win : (doc ? doc->doc_win : NULL);
      show_about_dialog(owner);
      break;
    }

    default:
      if (id != ID_TOOL_SELECT && fe_component_by_tool_ident(id))
        break;
      break;
  }
}

// ============================================================
// Menu bar window procedure
// ============================================================

result_t editor_menubar_proc(window_t *win, uint32_t msg,
                              uint32_t wparam, void *lparam) {
  if (msg == evCommand) {
    uint16_t notif = HIWORD(wparam);
    if (notif == kMenuBarNotificationItemClick ||
        notif == kAcceleratorNotification      ||
        notif == btnClicked) {
      handle_menu_command(LOWORD(wparam));
      return true;
    }
  }
  return win_menubar(win, msg, wparam, lparam);
}
