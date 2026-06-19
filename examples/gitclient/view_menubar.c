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
};

// ============================================================
// Menubar window procedure
// ============================================================

lresult_t gc_menubar_proc(window_t *win, uint32_t msg,
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
    case ID_BRANCH_CHECKOUT:
    case ID_BRANCH_MERGE:
    case ID_BRANCH_REBASE:
    case ID_BRANCH_DELETE:
      break;

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
