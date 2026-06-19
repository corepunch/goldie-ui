// New branch dialog — uses generated form from gitclient.orion.

#include "gitclient.h"

typedef struct {
  bool result;
} new_branch_state_t;

static lresult_t new_branch_proc(window_t *win, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate: {
      win->userdata = lparam;
      gc_state_t *gc = g_gc;
      if (!gc || !gc->db) return true;

      window_t *from_cb = get_window_item(win, ID_NEW_BRANCH_DIALOG_FROM);
      result_node_t *branches = (result_node_t *)send_db_message(
        gc->db, dbFetch, MAKEDWORD(ID_DB_BRANCHES, 0), (void *)(intptr_t)0);
      for (result_node_t *n = branches; n; n = n->next) {
        db_branche_t *b = *(db_branche_t **)n->data;
        if (!b->is_remote)
          send_message(from_cb, cbAddString, 0, b->name);
      }
      free_result_list(branches);

      char cur[256] = {0};
      git_current_branch(gc->repo, cur, sizeof(cur));
      if (cur[0]) set_window_item_text(win, ID_NEW_BRANCH_DIALOG_FROM, "%s", cur);
      return true;
    }

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (!src) return false;
        if (src->id == ID_NEW_BRANCH_DIALOG_CANCEL) {
          end_dialog(win, 0);
          return true;
        }
        if (src->id == ID_NEW_BRANCH_DIALOG_OK) {
          char name[256] = {0};
          window_t *name_edit = get_window_item(win, ID_NEW_BRANCH_DIALOG_NAME);
          if (name_edit)
            strncpy(name, name_edit->title, sizeof(name) - 1);
          if (!name[0]) {
            message_box(win, "Please enter a branch name.", "New Branch", MB_OK);
            return true;
          }

          char from[256] = {0};
          window_t *from_edit = get_window_item(win, ID_NEW_BRANCH_DIALOG_FROM);
          if (from_edit)
            strncpy(from, from_edit->title, sizeof(from) - 1);

          bool checkout = send_message(
            get_window_item(win, ID_NEW_BRANCH_DIALOG_CHECKOUT),
            btnGetCheck, 0, NULL) != 0;

          if (!gc_create_branch(name, from[0] ? from : NULL, checkout)) {
            message_box(win, "Branch creation failed.", "New Branch", MB_OK);
            return true;
          }

          new_branch_state_t *st = (new_branch_state_t *)win->userdata;
          if (st) st->result = true;
          end_dialog(win, 1);
          return true;
        }
      }
      return false;

    default:
      return default_winproc(win, msg, wparam, lparam);
  }
}

bool gc_show_new_branch_dialog(window_t *parent) {
  new_branch_state_t st = { false };
  show_dialog_from_form(&gc_new_branch_dialog_form, "New Branch", parent,
                         new_branch_proc, &st);
  if (st.result)
    gc_refresh_all();
  return st.result;
}
