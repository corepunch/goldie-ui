// Push / Pull / Fetch options dialog.
//
//  ┌─ Fetch ──────────────────────────────┐
//  │ Remote: [origin ▾]                  │
//  │ Branch: [main   ▾]                  │
//  │ [ ] Prune deleted branches          │
//  │                   [Cancel] [Fetch]  │
//  └──────────────────────────────────────┘
//
// The same form is re-used for Fetch / Pull / Push by passing
// a different git_op_t.  The command runs asynchronously via
// git_run_async() and posts evGitOpDone to the main window.

#include "gitclient.h"

#define CTL_REMOTE   1
#define CTL_BRANCH   2
#define CTL_PRUNE    3
#define CTL_FORCE    4
#define CTL_OK       5
#undef  CTL_CANCEL
#define CTL_CANCEL   6

static const form_ctrl_def_t kPPFRemoteRow[] = {
  { .class_name = "Label",   .text = "Remote:", .name = "lbl_remote",
    .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
  { .class_name = "ComboBox", .id = CTL_REMOTE, .name = "remote",
    .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
};

static const form_ctrl_def_t kPPFBranchRow[] = {
  { .class_name = "Label",   .text = "Branch:", .name = "lbl_branch",
    .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
  { .class_name = "ComboBox", .id = CTL_BRANCH, .name = "branch",
    .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
};

static const form_ctrl_def_t kPPFBtnRow[] = {
  { .class_name = "Button", .id = CTL_OK,     .flags = BUTTON_DEFAULT, .text = "OK",
    .name = "ok",     .h_align = LAYOUT_ALIGN_START },
  { .class_name = "Button", .id = CTL_CANCEL, .text = "Cancel",
    .name = "cancel", .h_align = LAYOUT_ALIGN_START },
};

static const form_ctrl_def_t kPPFCtrls[] = {
  {
    .class_name         = "StackView",
    .name               = "remote_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing     = 6,
    .h_align            = LAYOUT_ALIGN_STRETCH,
    .children           = kPPFRemoteRow,
    .child_count        = 2,
  },
  {
    .class_name         = "StackView",
    .name               = "branch_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing     = 6,
    .h_align            = LAYOUT_ALIGN_STRETCH,
    .children           = kPPFBranchRow,
    .child_count        = 2,
  },
  { .class_name = "CheckBox", .id = CTL_PRUNE, .text = "Prune deleted remote branches",
    .name = "prune", .h_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "CheckBox", .id = CTL_FORCE, .text = "Force",
    .name = "force", .h_align = LAYOUT_ALIGN_STRETCH },
  {
    .class_name         = "StackView",
    .name               = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing     = 6,
    .h_align            = LAYOUT_ALIGN_END,
    .children           = kPPFBtnRow,
    .child_count        = 2,
  },
};
static const form_def_t kPPFForm = {
  .name           = "Remote Operation",
  .flags          = WINDOW_AUTO_LAYOUT,
  .width          = 262,
  .height         = 104,
  .layout_spacing = 6,
  .padding        = {8, 8, 8, 8},
  .children       = kPPFCtrls,
  .child_count    = (int)(sizeof(kPPFCtrls)/sizeof(kPPFCtrls[0])),
  .ok_id          = CTL_OK,
  .cancel_id      = CTL_CANCEL,
};

typedef struct {
  git_op_t op;
} ppf_dlg_state_t;

static lresult_t ppf_dlg_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam) {
  ppf_dlg_state_t *st = (ppf_dlg_state_t *)win->userdata;

  switch (msg) {
    case evCreate: {
      win->userdata = lparam;
      st = (ppf_dlg_state_t *)lparam;
      gc_state_t *gc = g_gc;
      if (!gc || !gc->repo) return true;

      // Populate remotes.
      char remotes[8][256];
      int n = git_get_remotes(gc->repo, remotes, 8);
      window_t *remote_cb = get_window_item(win, CTL_REMOTE);
      for (int i = 0; i < n; i++)
        send_message(remote_cb, cbAddString, 0, remotes[i]);
      if (n > 0)
        set_window_item_text(win, CTL_REMOTE, "%s", remotes[0]);

      // Populate branches (local).
      window_t *branch_cb = get_window_item(win, CTL_BRANCH);
      for (int i = 0; i < gc->branch_count; i++) {
        if (!gc->branches[i].is_remote)
          send_message(branch_cb, cbAddString, 0, gc->branches[i].name);
      }
      char cur[256] = {0};
      git_current_branch(gc->repo, cur, sizeof(cur));
      if (cur[0]) set_window_item_text(win, CTL_BRANCH, "%s", cur);

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
        if (src->id == CTL_OK) {
          gc_state_t *gc = g_gc;
          if (!gc || !gc->repo) { end_dialog(win, 0); return true; }

          char remote[256] = {0};
          char branch[256] = {0};
          window_t *remote_w = get_window_item(win, CTL_REMOTE);
          window_t *branch_w = get_window_item(win, CTL_BRANCH);
          if (remote_w) strncpy(remote, remote_w->title, sizeof(remote) - 1);
          if (branch_w) strncpy(branch, branch_w->title, sizeof(branch) - 1);

          bool prune = send_message(get_window_item(win, CTL_PRUNE),
                                    btnGetCheck, 0, NULL) != 0;
          bool force = send_message(get_window_item(win, CTL_FORCE),
                                    btnGetCheck, 0, NULL) != 0;

          // Build arg list (static, valid for the lifetime of the async call
          // since they point to stack strings — safe because git_run_async
          // builds the shell command before the thread starts).
          const char *args[8];
          int ai = 0;
          args[ai++] = "git";

          git_op_t op = st ? st->op : GIT_OP_FETCH;
          switch (op) {
            case GIT_OP_FETCH:
              args[ai++] = "fetch";
              if (prune) args[ai++] = "--prune";
              if (remote[0]) args[ai++] = remote;
              break;
            case GIT_OP_PULL:
              args[ai++] = "pull";
              if (force) args[ai++] = "--force";
              if (remote[0]) args[ai++] = remote;
              if (branch[0]) args[ai++] = branch;
              break;
            case GIT_OP_PUSH:
              args[ai++] = "push";
              if (force) args[ai++] = "--force";
              if (remote[0]) args[ai++] = remote;
              if (branch[0]) args[ai++] = branch;
              break;
            default:
              break;
          }
          args[ai] = NULL;

          git_run_async(gc->repo, op, args, gc->main_win);
          end_dialog(win, 1);
          return true;
        }
      }
      return false;

    default:
      return default_winproc(win, msg, wparam, lparam);
  }
}

void gc_show_push_pull_dialog(window_t *parent, git_op_t op) {
  ppf_dlg_state_t st = { op };
  static const char *titles[] = {
    [GIT_OP_FETCH]   = "Fetch",
    [GIT_OP_PULL]    = "Pull",
    [GIT_OP_PUSH]    = "Push",
    [GIT_OP_CLONE]   = "Clone",
    [GIT_OP_GENERIC] = "Remote Operation",
  };
  const char *title = (op < 5) ? titles[op] : "Remote Operation";
  show_dialog_from_form(&kPPFForm, title, parent, ppf_dlg_proc, &st);
}

// ============================================================
// About dialog
// ============================================================

void gc_show_about_dialog(window_t *parent) {
  message_box(parent,
    "Git Client\n"
    "A SmartGit-style repository viewer\n"
    "built with the Orion UI framework.",
    "About", MB_OK);
}
