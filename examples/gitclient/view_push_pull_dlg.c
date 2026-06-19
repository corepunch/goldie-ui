// Push/Pull/Fetch dialog — uses generated form from gitclient.orion.

#include "gitclient.h"

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
      if (!gc || !gc->repo || !gc->db) return true;

      char remotes[8][256];
      int n = git_get_remotes(gc->repo, remotes, 8);
      window_t *remote_cb = get_window_item(win, ID_PUSH_PULL_DIALOG_REMOTE);
      for (int i = 0; i < n; i++)
        send_message(remote_cb, cbAddString, 0, remotes[i]);
      if (n > 0)
        set_window_item_text(win, ID_PUSH_PULL_DIALOG_REMOTE, "%s", remotes[0]);

      window_t *branch_cb = get_window_item(win, ID_PUSH_PULL_DIALOG_BRANCH);
      result_node_t *branches = (result_node_t *)send_db_message(
        gc->db, dbFetch, MAKEDWORD(ID_DB_BRANCHES, 0), (void *)(intptr_t)0);
      for (result_node_t *node = branches; node; node = node->next) {
        db_branche_t *b = *(db_branche_t **)node->data;
        if (!b->is_remote)
          send_message(branch_cb, cbAddString, 0, b->name);
      }
      free_result_list(branches);

      char cur[256] = {0};
      git_current_branch(gc->repo, cur, sizeof(cur));
      if (cur[0]) set_window_item_text(win, ID_PUSH_PULL_DIALOG_BRANCH, "%s", cur);
      return true;
    }

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (!src) return false;
        if (src->id == ID_PUSH_PULL_DIALOG_CANCEL) {
          end_dialog(win, 0);
          return true;
        }
        if (src->id == ID_PUSH_PULL_DIALOG_OK) {
          gc_state_t *gc = g_gc;
          if (!gc || !gc->repo) { end_dialog(win, 0); return true; }

          char remote[256] = {0};
          char branch[256] = {0};
          window_t *remote_w = get_window_item(win, ID_PUSH_PULL_DIALOG_REMOTE);
          window_t *branch_w = get_window_item(win, ID_PUSH_PULL_DIALOG_BRANCH);
          if (remote_w) strncpy(remote, remote_w->title, sizeof(remote) - 1);
          if (branch_w) strncpy(branch, branch_w->title, sizeof(branch) - 1);

          bool prune = send_message(
            get_window_item(win, ID_PUSH_PULL_DIALOG_PRUNE),
            btnGetCheck, 0, NULL) != 0;
          bool force = send_message(
            get_window_item(win, ID_PUSH_PULL_DIALOG_FORCE),
            btnGetCheck, 0, NULL) != 0;

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
  show_dialog_from_form(&gc_push_pull_dialog_form, title, parent,
                         ppf_dlg_proc, &st);
}

void gc_show_about_dialog(window_t *parent) {
  message_box(parent,
    "Git Client\n"
    "A SmartGit-style repository viewer\n"
    "built with the Orion UI framework.",
    "About", MB_OK);
}
