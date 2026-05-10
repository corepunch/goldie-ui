// New-branch dialog (form_def_t).
//
//  ┌─ New Branch ──────────────────────────────┐
//  │ Name: [_____________________________]     │
//  │ From: [master__________________▾]         │
//  │ [ ] Checkout after creation               │
//  │                        [Cancel] [Create]  │
//  └───────────────────────────────────────────┘

#include "gitclient.h"

#define CTL_NAME     1
#define CTL_FROM     2
#define CTL_CHECKOUT 3
#define CTL_CREATE   4
#define CTL_CANCEL   5

// Branch name row: label + textedit
static const form_ctrl_def_t kBranchNameRow[] = {
  { .class_name = "label",   .text = "Name:", .name = "lbl_name",
    .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
  { .class_name = "textedit", .id = CTL_NAME, .name = "name",
    .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
};

// Branch from row: label + combobox
static const form_ctrl_def_t kBranchFromRow[] = {
  { .class_name = "label",   .text = "From:", .name = "lbl_from",
    .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
  { .class_name = "combobox", .id = CTL_FROM, .name = "from",
    .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
};

// Button row
static const form_ctrl_def_t kBranchBtnRow[] = {
  { .class_name = "button", .id = CTL_CREATE, .flags = BUTTON_DEFAULT, .text = "Create",
    .name = "ok",     .h_align = LAYOUT_ALIGN_START },
  { .class_name = "button", .id = CTL_CANCEL, .text = "Cancel",
    .name = "cancel", .h_align = LAYOUT_ALIGN_START },
};

static const form_ctrl_def_t kNewBranchCtrls[] = {
  {
    .class_name         = "stack",
    .name               = "name_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing     = 6,
    .h_align            = LAYOUT_ALIGN_STRETCH,
    .v_align            = LAYOUT_ALIGN_START,
    .children           = kBranchNameRow,
    .child_count        = 2,
  },
  {
    .class_name         = "stack",
    .name               = "from_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing     = 6,
    .h_align            = LAYOUT_ALIGN_STRETCH,
    .v_align            = LAYOUT_ALIGN_START,
    .children           = kBranchFromRow,
    .child_count        = 2,
  },
  { .class_name = "checkbox", .id = CTL_CHECKOUT, .text = "Checkout after creation",
    .name = "co", .h_align = LAYOUT_ALIGN_STRETCH },
  {
    .class_name         = "stack",
    .name               = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing     = 6,
    .h_align            = LAYOUT_ALIGN_END,
    .v_align            = LAYOUT_ALIGN_START,
    .children           = kBranchBtnRow,
    .child_count        = 2,
  },
};
static const form_def_t kNewBranchForm = {
  .name           = "New Branch",
  .width          = 276,
  .height         = 86,
  .layout_spacing = 6,
  .padding        = {8, 8, 8, 8},
  .children       = kNewBranchCtrls,
  .child_count    = (int)(sizeof(kNewBranchCtrls)/sizeof(kNewBranchCtrls[0])),
  .ok_id          = CTL_CREATE,
  .cancel_id      = CTL_CANCEL,
};

typedef struct {
  bool result;
} new_branch_state_t;

static result_t new_branch_proc(window_t *win, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate: {
      win->userdata = lparam;
      gc_state_t *gc = g_gc;
      window_t *from_cb = get_window_item(win, CTL_FROM);
      if (gc) {
        // Populate the "from" combobox with all branch names.
        for (int i = 0; i < gc->branch_count; i++) {
          if (!gc->branches[i].is_remote)
            send_message(from_cb, cbAddString, 0, gc->branches[i].name);
        }
        // Set current branch as default.
        char cur[256] = {0};
        git_current_branch(gc->repo, cur, sizeof(cur));
        set_window_item_text(win, CTL_FROM, "%s", cur);
      }
      return true;
    }

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (!src) return false;
        if (src->id == CTL_CANCEL) {
          end_dialog(win, 0);
          return true;
        }
        if (src->id == CTL_CREATE) {
          gc_state_t *gc = g_gc;
          if (!gc || !gc->repo) { end_dialog(win, 0); return true; }

          char name[256] = {0};
          window_t *name_edit = get_window_item(win, CTL_NAME);
          if (name_edit)
            strncpy(name, name_edit->title, sizeof(name) - 1);
          if (!name[0]) {
            message_box(win, "Please enter a branch name.", "New Branch", MB_OK);
            return true;
          }

          char from[256] = {0};
          window_t *from_edit = get_window_item(win, CTL_FROM);
          if (from_edit)
            strncpy(from, from_edit->title, sizeof(from) - 1);

          char buf[1024] = {0};
          const char *args_create[] = {
            "git", "branch", name, from[0] ? from : NULL, NULL
          };
          bool ok = git_run_sync(gc->repo, args_create, buf, sizeof(buf));
          if (!ok) {
            message_box(win, buf, "Branch failed", MB_OK);
            return true;
          }

          bool checkout = send_message(get_window_item(win, CTL_CHECKOUT),
                                       btnGetCheck, 0, NULL) != 0;
          if (checkout) {
            const char *args_co[] = { "git", "checkout", name, NULL };
            git_run_sync(gc->repo, args_co, buf, sizeof(buf));
          }

          new_branch_state_t *st = (new_branch_state_t *)win->userdata;
          if (st) st->result = true;
          end_dialog(win, 1);
          return true;
        }
      }
      return false;

    default:
      return false;
  }
}

bool gc_show_new_branch_dialog(window_t *parent) {
  new_branch_state_t st = { false };
  show_dialog_from_form(&kNewBranchForm, "New Branch", parent,
                         new_branch_proc, &st);
  if (st.result)
    gc_refresh_all();
  return st.result;
}
