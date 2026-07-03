// Menu bar and command dispatch — uses generated menus from gitclient.orion.

#include "gitclient.h"
#include "../../gem_magic.h"

// ============================================================
// Accelerator table
// ============================================================

static const accel_t kAccelEntries[] = {
  { FVIRTKEY | FCONTROL, AX_KEY_K,  ID_COMMIT_COMMIT },
  { FVIRTKEY,            AX_KEY_F5, ID_REPO_REFRESH  },
  { FVIRTKEY | FCONTROL, AX_KEY_F,  ID_REMOTE_FETCH  },
  { FVIRTKEY | FCONTROL, AX_KEY_N,  ID_BRANCH_NEW    },
  { FVIRTKEY | FCONTROL, AX_KEY_D,  ID_BRANCH_DELETE },
  { FVIRTKEY | FCONTROL, AX_KEY_M,  ID_BRANCH_MERGE  },
};

// ============================================================
// Helper: get selected branch name from branches list
// ============================================================

static bool gc_get_selected_branch(char *buf, int buf_sz, bool *is_current) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->branches_win || !gc->db) return false;
  int sel = (int)send_message(gc->branches_win, RVM_GETSELECTION, 0, NULL);
  if (sel < 0) return false;
  result_node_t *rows = (result_node_t *)send_db_message(
    gc->db, dbFetch, MAKEDWORD(ID_DB_BRANCHES, 0), (void *)(intptr_t)0);
  int row = 0;
  bool found = false;
  for (result_node_t *n = rows; n; n = n->next, row++) {
    if (row == sel) {
      db_branche_t *b = *(db_branche_t **)n->data;
      strncpy(buf, b->name, (size_t)buf_sz - 1);
      if (is_current) *is_current = b->is_current;
      found = true;
      break;
    }
  }
  free_result_list(rows);
  return found;
}

// ============================================================
// Menubar window procedure
// ============================================================

result_t gc_menubar_proc(window_t *win, uint32_t msg,
                         uint32_t wparam, void *lparam) {
  if (msg == evCommand &&
      (HIWORD(wparam) == kMenuBarNotificationItemClick ||
       HIWORD(wparam) == kAcceleratorNotification)) {
    gc_handle_command(LOWORD(wparam));
    return true;
  }
  return win_menubar(win, msg, wparam, lparam);
}

void gc_create_menubar(void) {
  gc_state_t *gc = g_gc;
  if (!gc) return;

  // kGCMenus and kGCMenuCount are generated from gitclient.orion.
  gc->menubar_win = set_app_menu(gc_menubar_proc,
                                 kGCMenus, kGCMenuCount,
                                 gc_handle_command,
                                 gc->hinstance);

  int accel_count = (int)(sizeof(kAccelEntries) / sizeof(kAccelEntries[0]));
  gc->accel = load_accelerators(kAccelEntries, accel_count);
  if (gc->menubar_win && gc->accel)
    send_message(gc->menubar_win, kMenuBarMessageSetAccelerators,
                 0, gc->accel);
}

// ============================================================
// Command handler
// ============================================================

void gc_handle_command(uint16_t id) {
  gc_state_t *gc = g_gc;
  if (!gc) return;

  GC_LOG("gc_handle_command: id=%d", (int)id);

  switch (id) {
    case ID_FILE_OPEN_REPO: {
      char path[512] = {0};
      openfilename_t ofn = {0};
      ofn.lStructSize = sizeof(ofn);
      ofn.lpstrFile   = path;
      ofn.nMaxFile    = sizeof(path);
      ofn.Flags       = OFN_PICKFOLDER;
      if (get_folder_name(&ofn))
        gc_open_repo(path);
      break;
    }
    case ID_FILE_CLONE:
      break;
    case ID_FILE_QUIT:
      ui_request_quit();
      break;

    case ID_REPO_REFRESH:
      gc_refresh_all();
      break;
    case ID_REPO_TERMINAL:
      if (gc->repo) {
        char cmd[600];
        snprintf(cmd, sizeof(cmd), "xterm -e 'cd \"%s\" && bash' &",
                 git_repo_path(gc->repo));
        (void)system(cmd);
      }
      break;

    case ID_BRANCH_NEW:
      if (gc->main_win)
        gc_show_new_branch_dialog(gc->main_win);
      break;
    case ID_BRANCH_CHECKOUT: {
      char name[256] = {0};
      if (gc_get_selected_branch(name, sizeof(name), NULL)) {
        if (!gc_checkout_branch(name))
          message_box(gc->main_win, "Checkout failed.", "Checkout", MB_OK);
        else
          gc_refresh_all();
      } else {
        message_box(gc->main_win, "No branch selected.", "Checkout", MB_OK);
      }
      break;
    }
    case ID_BRANCH_MERGE: {
      char name[256] = {0};
      if (gc_get_selected_branch(name, sizeof(name), NULL)) {
        if (!gc_merge_branch(name))
          message_box(gc->main_win, "Merge failed.\nCheck for conflicts.", "Merge", MB_OK);
        else
          gc_refresh_all();
      } else {
        message_box(gc->main_win, "No branch selected.", "Merge", MB_OK);
      }
      break;
    }
    case ID_BRANCH_REBASE: {
      char name[256] = {0};
      if (gc_get_selected_branch(name, sizeof(name), NULL)) {
        if (!gc_rebase_onto(name))
          message_box(gc->main_win, "Rebase failed.\nCheck for conflicts.", "Rebase", MB_OK);
        else
          gc_refresh_all();
      } else {
        message_box(gc->main_win, "No branch selected.", "Rebase", MB_OK);
      }
      break;
    }
    case ID_BRANCH_DELETE: {
      char name[256] = {0};
      bool is_cur = false;
      if (gc_get_selected_branch(name, sizeof(name), &is_cur)) {
        if (is_cur && !strncmp(name, "remotes/", 8)) {
          message_box(gc->main_win, "Cannot delete remote-tracking branch.", "Delete", MB_OK);
          break;
        }
        if (is_cur) {
          message_box(gc->main_win, "Cannot delete the current branch.", "Delete", MB_OK);
          break;
        }
        bool remote = !strncmp(name, "remotes/", 8);
        if (!gc_delete_branch(name, remote))
          message_box(gc->main_win, "Delete failed.", "Delete", MB_OK);
        else
          gc_refresh_all();
      } else {
        message_box(gc->main_win, "No branch selected.", "Delete", MB_OK);
      }
      break;
    }
    case ID_BRANCH_RENAME: {
      char cur[256] = {0};
      git_current_branch(gc->repo, cur, sizeof(cur));
      if (cur[0] && gc->main_win)
        gc_show_rename_branch_dialog(gc->main_win, cur);
      break;
    }

    case ID_COMMIT_COMMIT:
      if (gc->main_win)
        gc_show_commit_dialog(gc->main_win, false);
      break;
    case ID_COMMIT_AMEND:
      if (gc->main_win)
        gc_show_commit_dialog(gc->main_win, true);
      break;
    case ID_COMMIT_STASH:
      gc_stash();
      gc_refresh_all();
      break;
    case ID_COMMIT_STASH_POP:
      gc_stash_pop();
      gc_refresh_all();
      break;
    case ID_COMMIT_DISCARD: {
      if (message_box(gc->main_win,
                       "Discard ALL uncommitted changes?\n"
                       "This cannot be undone.",
                       "Discard Changes", MB_YESNO) == IDYES) {
        gc_discard_all();
        gc_refresh_all();
      }
      break;
    }

    case ID_REMOTE_FETCH:
      if (gc->main_win)
        gc_show_push_pull_dialog(gc->main_win, GIT_OP_FETCH);
      break;
    case ID_REMOTE_PULL:
      if (gc->main_win)
        gc_show_push_pull_dialog(gc->main_win, GIT_OP_PULL);
      break;
    case ID_REMOTE_PUSH:
      if (gc->main_win)
        gc_show_push_pull_dialog(gc->main_win, GIT_OP_PUSH);
      break;
    case ID_REMOTE_MANAGE:
      break;

    case ID_HELP_ABOUT:
      gc_show_about_dialog(gc->main_win);
      break;

    default:
      break;
  }
}
