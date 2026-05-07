// VIEW: About dialog (form-based, DDX-driven).

#include "taskmanager.h"

// ============================================================
// Form definition — auto-layout vertical stack
// ============================================================

static const form_ctrl_def_t kAboutChildren[] = {
  { .class_name = "label",  .text = "Orion Task Manager",           .name = "lbl_title",
    .h_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "label",  .text = "Version 1.0",                  .name = "lbl_version",
    .h_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "label",  .text = "CRUD demo using Orion framework.", .name = "lbl_desc",
    .h_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "button", .id = ID_OK, .flags = BUTTON_DEFAULT,   .text = "OK",
    .name = "btn_ok", .h_align = LAYOUT_ALIGN_CENTER },
};

static const form_def_t kAboutForm = {
  .name           = "About",
  .width          = 220,
  .height         = 96,
  .flags          = 0,
  .auto_layout    = true,
  .layout_kind    = "stack",
  .layout_spacing = 6,
  .padding        = {8, 8, 8, 8},
  .children       = kAboutChildren,
  .child_count    = (int)(sizeof(kAboutChildren)/sizeof(kAboutChildren[0])),
  .ok_id          = ID_OK,
};

// ============================================================
// Public entry point
// ============================================================

void show_about_dialog(window_t *parent) {
  show_ddx_dialog(&kAboutForm, "About Task Manager", parent, NULL);
}
