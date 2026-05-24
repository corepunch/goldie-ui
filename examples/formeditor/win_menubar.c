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

// Document window procedure
// ============================================================

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
  { .class_name = "CheckBox", .id = GRID_ID_SHOW, .text = "Show grid", .name = "chk_show",
    .h_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "CheckBox", .id = GRID_ID_SNAP, .text = "Snap to grid", .name = "chk_snap",
    .h_align = LAYOUT_ALIGN_STRETCH },
  {
    .class_name = "StackView",
    .name = "size_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "Label", .text = "Grid size:", .name = "lbl_size", .size = {60, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "TextBox", .id = GRID_ID_SIZE, .text = "", .name = "edit_size", .size = {40, BUTTON_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
    },
    .child_count = 2,
  },
  {
    .class_name = "StackView",
    .name = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 4,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "Space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
      { .class_name = "Button", .id = GRID_ID_OK, .size = {50, BUTTON_HEIGHT},
        .flags = BUTTON_DEFAULT, .text = "OK", .name = "btn_ok", .h_align = LAYOUT_ALIGN_START },
      { .class_name = "Button", .id = GRID_ID_CANCEL, .size = {50, BUTTON_HEIGHT},
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
  window_t *doc;
  bool        accepted;
} grid_dlg_state_t;

static lresult_t grid_dlg_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam) {
  grid_dlg_state_t *gs = (grid_dlg_state_t *)win->userdata;
  switch (msg) {
    case evCreate: {
      gs = (grid_dlg_state_t *)lparam;
      win->userdata = gs;
      if (!g_app)
        return false;
      // Set checkbox states manually (no checkbox DDX helper yet).
      window_t *chk_show = get_window_item(win, GRID_ID_SHOW);
      window_t *chk_snap = get_window_item(win, GRID_ID_SNAP);
      if (chk_show)
        send_message(chk_show, btnSetCheck,
                     g_app->show_grid ? btnStateChecked : btnStateUnchecked, NULL);
      if (chk_snap)
        send_message(chk_snap, btnSetCheck,
                     g_app->snap_to_grid ? btnStateChecked : btnStateUnchecked, NULL);
      // Push grid_size via DDX.
      grid_size_data_t gsd = { g_app->grid_size };
      dialog_push(win, &gsd, STATIC_ARRAY(k_grid_bindings));
      return true;
    }
    case evCommand: {
      if (HIWORD(wparam) != btnClicked) return false;
      window_t *src = (window_t *)lparam;
      if (!src) return false;
      if (src->id == GRID_ID_OK) {
        if (!g_app)
          return false;
        // Pull grid_size via DDX.
        grid_size_data_t gsd = { g_app->grid_size };
        dialog_pull(win, &gsd, STATIC_ARRAY(k_grid_bindings));
        if (gsd.grid_size < GRID_SIZE_MIN) gsd.grid_size = GRID_SIZE_MIN;
        if (gsd.grid_size > GRID_SIZE_MAX) gsd.grid_size = GRID_SIZE_MAX;
        g_app->grid_size = gsd.grid_size;
        // Pull checkboxes manually.
        window_t *chk_show = get_window_item(win, GRID_ID_SHOW);
        window_t *chk_snap = get_window_item(win, GRID_ID_SNAP);
        if (chk_show)
          g_app->show_grid = (send_message(chk_show, btnGetCheck, 0, NULL) == btnStateChecked);
        if (chk_snap)
          g_app->snap_to_grid = (send_message(chk_snap, btnGetCheck, 0, NULL) == btnStateChecked);
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
      return default_winproc(win, msg, wparam, lparam);
  }
}

void show_grid_settings_dialog(window_t *parent, window_t *doc) {
  if (!g_app)
    return;
  grid_dlg_state_t gs = { doc, false };
  show_dialog_from_form(&kGridForm, "Grid Settings", parent, grid_dlg_proc, &gs);
  if (gs.accepted && doc && doc->children)
    invalidate_window(doc->children);
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
    .class_name = "GridView",
    .name = "fields",
    .flags = WINDOW_FLEXSPACE,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      {
        .class_name = "Column",
        .name = "labels",
        .size = {60, 0},
        .children = (const form_ctrl_def_t[]){
          { .class_name = "Label", .text = "Caption:", .name = "lbl_caption", .size = {60, CONTROL_HEIGHT},
            .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
          { .class_name = "Label", .text = "Name:", .name = "lbl_name", .size = {60, CONTROL_HEIGHT},
            .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
        },
        .child_count = 2,
      },
      {
        .class_name = "Column",
        .name = "inputs",
        .flags = WINDOW_FLEXSPACE,
        .children = (const form_ctrl_def_t[]){
          { .class_name = "TextBox", .id = PROPS_ID_CAPTION, .text = "", .name = "edit_caption",
            .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
          { .class_name = "TextBox", .id = PROPS_ID_NAME, .text = "", .name = "edit_name",
            .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
        },
        .child_count = 2,
      },
    },
    .child_count = 2,
  },
  {
    .class_name = "StackView",
    .name = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 4,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "Space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
      { .class_name = "Button", .id = PROPS_ID_OK, .size = {50, BUTTON_HEIGHT},
        .flags = BUTTON_DEFAULT, .text = "OK", .name = "btn_ok", .h_align = LAYOUT_ALIGN_START },
      { .class_name = "Button", .id = PROPS_ID_CANCEL, .size = {50, BUTTON_HEIGHT},
        .text = "Cancel", .name = "btn_cancel", .h_align = LAYOUT_ALIGN_START },
    },
    .child_count = 3,
  },
};

// Element-properties dialog is disabled in window-first migration mode.

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
  { .class_name = "CheckBox", .id = FORM_PROPS_ID_AUTO, .text = "Use auto layout", .name = "chk_auto",
    .h_align = LAYOUT_ALIGN_STRETCH },
  {
    .class_name = "StackView",
    .name = "kind_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "Label", .text = "Layout:", .name = "lbl_kind", .size = {72, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "ComboBox", .id = FORM_PROPS_ID_KIND, .text = "", .name = "combo_kind", .size = {124, BUTTON_HEIGHT + 2},
        .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER, .flags = WINDOW_FLEXSPACE },
    },
    .child_count = 2,
  },
  {
    .class_name = "StackView",
    .name = "orient_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "Label", .text = "Orientation:", .name = "lbl_orient", .size = {72, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "ComboBox", .id = FORM_PROPS_ID_ORIENT, .text = "", .name = "combo_orient", .size = {124, BUTTON_HEIGHT + 2},
        .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER, .flags = WINDOW_FLEXSPACE },
    },
    .child_count = 2,
  },
  {
    .class_name = "StackView",
    .name = "columns_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "Label", .text = "Columns:", .name = "lbl_columns", .size = {72, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "TextBox", .id = FORM_PROPS_ID_COLUMNS, .text = "", .name = "edit_columns", .size = {64, BUTTON_HEIGHT + 2},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
    },
    .child_count = 2,
  },
  {
    .class_name = "StackView",
    .name = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 4,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "Space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
      { .class_name = "Button", .id = FORM_PROPS_ID_OK, .size = {50, BUTTON_HEIGHT},
        .flags = BUTTON_DEFAULT, .text = "OK", .name = "btn_ok", .h_align = LAYOUT_ALIGN_START },
      { .class_name = "Button", .id = FORM_PROPS_ID_CANCEL, .size = {50, BUTTON_HEIGHT},
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

static lresult_t form_props_proc(window_t *win, uint32_t msg,
                                uint32_t wparam, void *lparam) {
  form_props_state_t *ps = (form_props_state_t *)win->userdata;
  switch (msg) {
    case evCreate:
      ps = (form_props_state_t *)lparam;
      win->userdata = ps;
      if (ps && g_app && g_app->active_form) {
        form_props_fill_layout_combos(win);
        dialog_push(win, ps, STATIC_ARRAY(k_form_props_bindings));
      }
      return true;
    case evCommand: {
      if (HIWORD(wparam) != btnClicked || !ps) return false;
      window_t *src = (window_t *)lparam;
      if (!src) return false;
      if (src->id == FORM_PROPS_ID_OK) {
        if (g_app && g_app->active_form) {
          window_t *doc = g_app->active_form;
          bool old_auto_layout = (doc->flags & WINDOW_AUTO_LAYOUT) != 0;
          flags_t old_orient = doc->flags & WINDOW_STACK_HORIZONTAL;
          dialog_pull(win, ps, STATIC_ARRAY(k_form_props_bindings));
          if (ps->auto_layout_enabled)
            doc->flags |= WINDOW_AUTO_LAYOUT;
          else
            doc->flags &= ~WINDOW_AUTO_LAYOUT;
          if (ps->layout_orientation & WINDOW_STACK_HORIZONTAL)
            doc->flags |= WINDOW_STACK_HORIZONTAL;
          else
            doc->flags &= ~WINDOW_STACK_HORIZONTAL;
          if (((doc->flags & WINDOW_AUTO_LAYOUT) != 0) != old_auto_layout ||
              (doc->flags & WINDOW_STACK_HORIZONTAL) != old_orient) {
            fe_doc_mark_modified(doc);
          }
          if (doc->flags & WINDOW_AUTO_LAYOUT) {
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
      return default_winproc(win, msg, wparam, lparam);
  }
}

bool show_form_props_dialog(window_t *parent, window_t *doc) {
  if (!doc) return false;
  form_props_state_t st = {
    .auto_layout_enabled = (doc->flags & WINDOW_AUTO_LAYOUT) != 0,
    .layout_mode = 0,
    .layout_orientation = (doc->flags & WINDOW_STACK_HORIZONTAL) ? WINDOW_STACK_HORIZONTAL : WINDOW_STACK_VERTICAL,
  };
  snprintf(st.layout_columns, sizeof(st.layout_columns), "0");
  show_dialog_from_form(&kFormPropsForm, "Form Properties", parent, form_props_proc, &st);
  return st.accepted;
}

bool show_props_dialog(window_t *parent, form_element_t *el) {
  (void)el;
  message_box(parent,
              "Element Properties",
              "Element properties are disabled in window-only migration mode.",
              MB_OK);
  return false;
}


// ============================================================
// Menu bar window procedure
// ============================================================

lresult_t editor_menubar_proc(window_t *win, uint32_t msg,
                              uint32_t wparam, void *lparam) {
  if (msg == evCommand) {
    uint16_t notif = HIWORD(wparam);
    if (notif == kMenuBarNotificationItemClick ||
        notif == kAcceleratorNotification      ||
        notif == btnClicked) {
      return true;
    }
  }
  return win_menubar(win, msg, wparam, lparam);
}
