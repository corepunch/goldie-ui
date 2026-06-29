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

void show_grid_settings_dialog(window_t *parent, form_doc_t *doc) {
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

bool show_form_props_dialog(window_t *parent, form_doc_t *doc) {
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
                 "Unknown", ps->el->id,
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
// Menu bar window procedure
// ============================================================

result_t editor_menubar_proc(window_t *win, uint32_t msg,
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
