// Commit dialog — uses generated form from gitclient.orion.

#include "gitclient.h"

typedef struct {
  bool  amend_requested;
  bool  result;
} commit_dlg_state_t;

static lresult_t commit_dlg_proc(window_t *win, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  commit_dlg_state_t *st = (commit_dlg_state_t *)win->userdata;

  switch (msg) {
    case evCreate:
      win->userdata = lparam;
      st = (commit_dlg_state_t *)lparam;
      if (st && st->amend_requested) {
        send_message(get_window_item(win, ID_COMMIT_DIALOG_AMEND),
                     btnSetCheck, 1, NULL);
        gc_state_t *gc = g_gc;
        if (gc && gc->repo) {
          char prev_msg[1024] = {0};
          const char *args[] = { "git", "log", "-1", "--format=%B", NULL };
          git_run_sync(gc->repo, args, prev_msg, sizeof(prev_msg));
          char *nl = strrchr(prev_msg, '\n');
          if (nl && *(nl+1) == '\0') *nl = '\0';
          send_message(get_window_item(win, ID_COMMIT_DIALOG_MSG),
                       edSetText, 0, prev_msg);
        }
      }
      return true;

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (!src) return false;
        if (src->id == ID_COMMIT_DIALOG_CANCEL) {
          end_dialog(win, 0);
          return true;
        }
        if (src->id == ID_COMMIT_DIALOG_OK) {
          char msg_text[2048] = {0};
          send_message(get_window_item(win, ID_COMMIT_DIALOG_MSG),
                       edGetText, sizeof(msg_text), msg_text);
          if (!msg_text[0]) {
            message_box(win, "Please enter a commit message.",
                        "Commit", MB_OK);
            return true;
          }
          bool amend = send_message(get_window_item(win, ID_COMMIT_DIALOG_AMEND),
                                    btnGetCheck, 0, NULL) != 0;
          if (!gc_commit(msg_text, amend)) {
            message_box(win, "Commit failed.", "Commit", MB_OK);
            return true;
          }
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

bool gc_show_commit_dialog(window_t *parent, bool amend) {
  commit_dlg_state_t st = { .amend_requested = amend, .result = false };
  show_dialog_from_form(&gc_commit_dialog_form, "Commit", parent,
                         commit_dlg_proc, &st);
  if (st.result)
    gc_refresh_all();
  return st.result;
}
