// Branch rename dialog — renames the current branch.

#include "gitclient.h"

typedef struct {
  char  cur_name[256];
  bool  result;
} branch_rename_state_t;

static result_t rename_branch_proc(window_t *win, uint32_t msg,
                                    uint32_t wparam, void *lparam) {
  branch_rename_state_t *st = (branch_rename_state_t *)win->userdata;

  switch (msg) {
    case evCreate:
      win->userdata = lparam;
      st = (branch_rename_state_t *)lparam;
      if (st && st->cur_name[0])
        set_window_item_text(win, ID_BRANCH_RENAME_DIALOG_NAME, "%s", st->cur_name);
      return true;

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (!src) return false;
        if (src->id == ID_BRANCH_RENAME_DIALOG_CANCEL) {
          end_dialog(win, 0);
          return true;
        }
        if (src->id == ID_BRANCH_RENAME_DIALOG_OK) {
          char new_name[256] = {0};
          send_message(get_window_item(win, ID_BRANCH_RENAME_DIALOG_NAME),
                       edGetText, sizeof(new_name), new_name);
          if (!new_name[0]) {
            message_box(win, "Please enter a branch name.", "Rename Branch", MB_OK);
            return true;
          }
          if (!gc_rename_branch(st->cur_name, new_name)) {
            message_box(win, "Branch rename failed.", "Rename Branch", MB_OK);
            return true;
          }
          st = (branch_rename_state_t *)win->userdata;
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

bool gc_show_rename_branch_dialog(window_t *parent, const char *cur_name) {
  branch_rename_state_t st = { .result = false };
  strncpy(st.cur_name, cur_name ? cur_name : "HEAD", sizeof(st.cur_name) - 1);
  show_dialog_from_form(&gc_branch_rename_dialog_form, "Rename Branch", parent,
                         rename_branch_proc, &st);
  if (st.result)
    gc_refresh_all();
  return st.result;
}
