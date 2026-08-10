// New branch dialog — uses generated form from gitclient.orion.

#include "gitclient.h"

typedef struct {
  char name[256];
  char from[256];
  bool checkout;
  bool result;
} new_branch_state_t;

static const ctrl_binding_t branch_bindings[] = {
  DDX_TEXT (ID_NEW_BRANCH_DIALOG_NAME,     new_branch_state_t, name),
  DDX_TEXT (ID_NEW_BRANCH_DIALOG_FROM,     new_branch_state_t, from),
  DDX_CHECK(ID_NEW_BRANCH_DIALOG_CHECKOUT, new_branch_state_t, checkout),
};

static result_t new_branch_proc(window_t *win, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  new_branch_state_t *st = (new_branch_state_t *)win->userdata;

  switch (msg) {
    case evCreate:
      win->userdata = lparam; st = (new_branch_state_t *)lparam;
      if (!st->from[0] && g_gc && g_gc->repo)
        git_current_branch(g_gc->repo, st->from, sizeof(st->from));
      dialog_push(win, st, branch_bindings, ARRAY_LEN(branch_bindings));
      return true;

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (!src) return false;
        if (src->id == ID_NEW_BRANCH_DIALOG_CANCEL) { end_dialog(win, 0); return true; }
        if (src->id == ID_NEW_BRANCH_DIALOG_OK) {
          dialog_pull(win, st, branch_bindings, ARRAY_LEN(branch_bindings));
          if (!st->name[0]) {
            message_box(win, "Please enter a branch name.", "New Branch", MB_OK);
            return true;
          }
          if (!gc_create_branch(st->name, st->from[0] ? st->from : NULL, st->checkout)) {
            message_box(win, "Branch creation failed.", "New Branch", MB_OK);
            return true;
          }
          st->result = true;
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
  new_branch_state_t st = {0};
  show_dialog_from_form(&gc_new_branch_dialog_form, "New Branch", parent,
                         new_branch_proc, &st);
  if (st.result)
    gc_refresh_all();
  return st.result;
}
