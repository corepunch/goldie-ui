// Main window — uses generated form from gitclient.orion.

#include "gitclient.h"
#include "../../user/vga_font.h"

// ============================================================
// Open / refresh
// ============================================================

void gc_open_repo(const char *path) {
  gc_state_t *gc = g_gc;
  if (!gc) return;

  git_repo_close(gc->repo);
  gc->repo = git_repo_open(path);
  if (!gc->repo) {
    message_box(gc->main_win, "Not a valid git repository.", "Open Repository",
                MB_OK);
    return;
  }

  strncpy(gc->repo_path, path, sizeof(gc->repo_path) - 1);

  if (gc->main_win) {
    char title[600];
    snprintf(title, sizeof(title), "Git Client — %s", path);
    strncpy(gc->main_win->title, title, sizeof(gc->main_win->title) - 1);
    gc->main_win->title[sizeof(gc->main_win->title) - 1] = '\0';
    invalidate_window(gc->main_win);
  }

  gc_refresh_all();
}

void gc_refresh_all(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo) return;

  gc_load_from_git();

  if (gc->branches_win)
    send_message(gc->branches_win, tvRefresh, 0, NULL);
  if (gc->branches_win) {
    result_node_t *rows = (result_node_t *)send_db_message(
      gc->db, dbFetch, MAKEDWORD(ID_DB_BRANCHES, 0), (void *)(intptr_t)0);
    int row = 0;
    for (result_node_t *n = rows; n; n = n->next, row++) {
      db_branche_t *branch = *(db_branche_t **)n->data;
      if (branch && branch->is_current) {
        send_message(gc->branches_win, RVM_SETSELECTION, (uint32_t)row, NULL);
        break;
      }
    }
    free_result_list(rows);
  }
  // NOTE: tvRefresh on log_win below fetches ALL commits (ignoring branch
  // filter), then tableview_handle_master_selection re-filters them via
  // tvSetFilter.  The redundant fetch is harmless but means the log briefly
  // loads more data than needed.  If this becomes a perf bottleneck, swap
  // the order: call tableview_handle_master_selection first (which sends
  // tvSetFilter to children), then tvRefresh so the log fetches only the
  // filtered set.
  if (gc->log_win)
    send_message(gc->log_win, tvRefresh, 0, NULL);
  if (gc->branches_win && gc->log_win)
    tableview_handle_master_selection(get_root_window(gc->branches_win),
                                      gc->branches_win);
  // Similarly, files_win tvRefresh below uses whatever filter values were
  // left from the previous selection; the master-detail mechanism will
  // correct it after log_win's RVN_SELCHANGE propagates.  At full-refresh
  // time this is acceptable, but it does mean an extra round-trip.
  if (gc->files_win)
    send_message(gc->files_win, tvRefresh, 0, NULL);

  gc_diff_refresh();
  gc_update_status();
}

void gc_update_status(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->main_win) return;

  char status[256] = "No repository";
  if (gc->repo) {
    char branch[128] = "HEAD";
    git_current_branch(gc->repo, branch, sizeof(branch));

    char ahead[16] = "?", behind[16] = "?";
    {
      char buf[64] = {0};
      const char *aa[] = { "git", "rev-list", "--count", "@{u}..HEAD", NULL };
      if (git_run_sync(gc->repo, aa, buf, sizeof(buf))) {
        char *nl = strchr(buf, '\n');
        if (nl) *nl = '\0';
        strncpy(ahead, buf, sizeof(ahead) - 1);
      }
    }
    {
      char buf[64] = {0};
      const char *aa[] = { "git", "rev-list", "--count", "HEAD..@{u}", NULL };
      if (git_run_sync(gc->repo, aa, buf, sizeof(buf))) {
        char *nl = strchr(buf, '\n');
        if (nl) *nl = '\0';
        strncpy(behind, buf, sizeof(behind) - 1);
      }
    }
    snprintf(status, sizeof(status),
             "Branch: %s  ↑%s  ↓%s", branch, ahead, behind);
  }
  send_message(gc->main_win, evStatusBar, 0, (void *)status);
}

// ============================================================
// Main window procedure
// ============================================================

result_t gc_main_proc(window_t *win, uint32_t msg,
                      uint32_t wparam, void *lparam) {
  gc_state_t *gc = (gc_state_t *)win->userdata;

  switch (msg) {
    case evCreate: {
      gc = g_gc;
      win->userdata = gc;

      // The generated form owns hierarchy, sizing, and database propagation.
      // The controller only keeps outlets for event routing and refreshes.
      gc->branches_win = get_window_item(win, ID_MAIN_WINDOW_BRANCHES);
      gc->log_win = get_window_item(win, ID_MAIN_WINDOW_LOG);
      gc->files_win = get_window_item(win, ID_MAIN_WINDOW_FILES);
      gc->diff_win = get_window_item(win, ID_MAIN_WINDOW_DIFF);
      GC_LOG("form outlets: branches=%p log=%p files=%p diff=%p",
             (void *)gc->branches_win, (void *)gc->log_win,
             (void *)gc->files_win, (void *)gc->diff_win);

      send_message(win, evStatusBar, 0, "No repository");

      // Load VGA font (TTF → character sheet generated at runtime).
      char font_path[600];
      snprintf(font_path, sizeof(font_path),
               "%s/../share/orion/fonts/monoid.ttf",
               ui_get_exe_dir());
      vga_font_init(font_path, 16);

      return true;
    }

    case evDestroy:
      vga_font_shutdown();
      git_repo_close(gc->repo);
      gc->repo = NULL;
      return false;

    case evPaint:
      return false;

    case evCommand: {
      uint16_t code = (uint16_t)HIWORD(wparam);

      if (code == btnClicked || code == 0) {
        gc_handle_command((uint16_t)LOWORD(wparam));
        return true;
      }

      if (code == RVN_SELCHANGE) {
        if (!gc) return true;
        int sel   = (int)(int16_t)LOWORD(wparam);
        window_t *src = (window_t *)lparam;

        if (src == gc->branches_win) {
          // Selection is about to change - the master-detail cascade will
          // refresh the commits view.  Reset state so the auto-select
          // on the new commit list always triggers a load.
          gc->selected_commit = -1;
          gc->selected_file   = -1;
        } else if (src == gc->log_win) {
          if (sel != gc->selected_commit) {
            gc->selected_commit = sel;
            gc->selected_file   = -1;
            gc_load_commit_files();
            gc_diff_refresh();
          }
        } else if (src == gc->files_win) {
          if (sel != gc->selected_file) {
            gc->selected_file = sel;
            gc_diff_refresh();
          }
        }
        return true;
      }

      if (code == RVN_DBLCLK) {
        if (!gc) return false;
        int idx       = (int)(int16_t)LOWORD(wparam);
        window_t *src = (window_t *)lparam;

        if (src == gc->files_win && gc->selected_commit < 0 &&
            gc->repo && idx >= 0) {
          result_node_t *files = (result_node_t *)send_db_message(
            gc->db, dbFetch, MAKEDWORD(ID_DB_FILES, 0), (void *)(intptr_t)0);
          result_node_t *node = files;
          for (int i = 0; i < idx && node; i++) node = node->next;
          if (node) {
            db_file_t *f = *(db_file_t **)node->data;
            if (f->staged)
              gc_unstage_file(f->path);
            else
              gc_stage_file(f->path);
          }
          free_result_list(files);
          gc_refresh_all();
          return true;
        }
        return false;
      }

      return false;
    }

    case evGitOpDone: {
      git_async_result_t *res = (git_async_result_t *)lparam;
      if (res) {
        if (!res->success)
          message_box(win, res->output, "Operation failed", MB_OK);
        else
          gc_refresh_all();
        git_async_result_free(res);
      }
      return true;
    }

    case evOpenRepo:
      if (lparam)
        gc_open_repo((const char *)lparam);
      return true;

    default:
      return false;
  }
}
