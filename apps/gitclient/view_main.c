// Main window — uses generated form from gitclient.orion.

#include "gitclient.h"
#include <orion/user/vga_font.h>
#include <orion/commctl/menubar.h>

// ============================================================
// Open / refresh
// ============================================================

void gc_set_view_mode(bool history) {
  gc_state_t *gc = g_gc; if (!gc || !gc->main_win) return;
  window_t *log = get_window_item(gc->main_win, ID_MAIN_WINDOW_LOG);
  window_t *changes = get_window_item(gc->main_win, ID_MAIN_WINDOW_CHANGES_PANEL);
  window_t *split = get_window_item(gc->main_win, ID_MAIN_WINDOW_LOG_FILES_SPLIT);
  gc->history_mode = history;
  if (log) window_set_state(log, WINDOW_STATE_VISIBLE, history);
  if (changes) window_set_state(changes, WINDOW_STATE_VISIBLE, !history);
  if (!history && gc->files_win) {
    gc->selected_commit = -1;
    send_message(gc->files_win, tvSetFilter, ID_DB_FILES_COMMIT_ID, (void *)(intptr_t)0);
  } else if (history && gc->branches_win) {
    tableview_handle_master_selection(get_root_window(gc->branches_win), gc->branches_win);
  }
  if (split) send_message(split, evResize, 0, NULL);
  invalidate_window(gc->main_win);
}

void gc_open_repo(const char *path) {
  gc_state_t *gc = g_gc;
  if (!gc) return;

  git_repo_t *next = git_repo_open(path);
  if (!next) {
    message_box(gc->main_win, "Not a valid git repository.", "Open Repository",
                MB_OK);
    return;
  }
  git_repo_close(gc->repo);
  gc->repo = next;

  strncpy(gc->repo_path, path, sizeof(gc->repo_path) - 1);
  gc_recent_add(git_repo_path(gc->repo));

  if (gc->main_win) {
    char title[600];
    snprintf(title, sizeof(title), "Git Client - %s", path);
    strncpy(gc->main_win->title, title, sizeof(gc->main_win->title) - 1);
    gc->main_win->title[sizeof(gc->main_win->title) - 1] = '\0';
    invalidate_window(gc->main_win);
  }

  gc_refresh_all();
}

void gc_refresh_all(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo) return;

  // Reset selection indices so that the cascade-driven RVN_SELCHANGE on row 0
  // is never suppressed by a stale matching value from the previous load.
  gc->selected_commit = -1;
  gc->selected_file   = -1;

  send_db_message(gc->db, dbLoad, 0, gc->repo);

  if (gc->branches_win)
    send_message(gc->branches_win, tvRefresh, 0, NULL);
  if (gc->tags_win)
    send_message(gc->tags_win, tvRefresh, 0, NULL);
  if (gc->stash_win)
    send_message(gc->stash_win, tvRefresh, 0, NULL);
  // Select the current branch — the master-detail cascade then automatically
  // populates commits (filtered by branch_id) and files (lazy-loaded per commit).
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
    tableview_handle_master_selection(get_root_window(gc->branches_win),
                                      gc->branches_win);
  }
  if (!gc->history_mode && gc->files_win) {
    gc->selected_commit = -1;
    send_message(gc->files_win, tvSetFilter, ID_DB_FILES_COMMIT_ID, (void *)(intptr_t)0);
  }

  gc_diff_refresh();
  gc_update_status();
}

void gc_update_status(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->main_win) return;

    char status[384] = "No repository";
    if (gc->repo) {
      git_sync_status_t st = {0}; git_get_sync_status(gc->repo, &st);
      const char *kind = st.initial ? "first commit" : st.detached ? "detached" :
                         st.gone ? "upstream gone" : !st.upstream[0] ? "not published" : NULL;
      snprintf(status, sizeof(status), "Branch: %s%s  ^%d  v%d%s%s%s%s",
               st.head, st.dirty ? " *" : "", st.ahead, st.behind,
               st.upstream[0] ? "  " : "", st.upstream[0] ? st.upstream : "",
               kind ? "  (" : "", kind ? kind : "");
      if (kind) strncat(status, ")", sizeof(status) - strlen(status) - 1);
    }
  send_message(gc->main_win, evStatusBar, 0, (void *)status);
  if (gc->repo) {
    git_sync_status_t st = {0}; git_get_sync_status(gc->repo, &st);
    set_window_item_text(gc->main_win, ID_MAIN_WINDOW_COMMIT_HINT,
                         st.initial ? "Create the first commit" : "Commit staged changes");
    set_window_item_text(gc->main_win, ID_MAIN_WINDOW_COMMIT_NOW,
                         st.head[0] && !st.detached ? "Commit to %s" : "Commit", st.head);
  }
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
      gc->main_win = win;

      // The generated form owns hierarchy, sizing, and database propagation.
      // The controller only keeps outlets for event routing and refreshes.
      gc->branches_win = get_window_item(win, ID_MAIN_WINDOW_BRANCHES);
      gc->tags_win = get_window_item(win, ID_MAIN_WINDOW_TAGS);
      gc->stash_win = get_window_item(win, ID_MAIN_WINDOW_STASH_LIST);
      gc->log_win = get_window_item(win, ID_MAIN_WINDOW_LOG);
      gc->files_win = get_window_item(win, ID_MAIN_WINDOW_FILES);
      gc->diff_win = get_window_item(win, ID_MAIN_WINDOW_DIFF);
      GC_LOG("form outlets: branches=%p log=%p files=%p diff=%p",
             (void *)gc->branches_win, (void *)gc->log_win,
             (void *)gc->files_win, (void *)gc->diff_win);

      send_message(win, evStatusBar, 0, "No repository");
      gc->refresh_timer = axSetTimer(win, 15000, NULL, true);
      gc_set_view_mode(false);

      // Load VGA font (TTF → character sheet generated at runtime).
      char font_path[600];
      snprintf(font_path, sizeof(font_path),
               "%s/../share/orion/fonts/monoid.ttf",
               ui_get_exe_dir());
      vga_font_init(font_path, 12.0f);

      return true;
    }

    case evDestroy:
      if (gc && gc->refresh_timer) { axCancelTimer(gc->refresh_timer); gc->refresh_timer = 0; }
      vga_font_shutdown();
      git_repo_close(gc->repo);
      gc->repo = NULL;
      return false;

    case evActivate:
      if (gc && gc->repo && wparam != WA_INACTIVE) gc_refresh_all();
      return false;

    case evTimer:
      if (gc && gc->repo && wparam == gc->refresh_timer) { gc_refresh_all(); return true; }
      return false;

    case evPaint:
      return false;

    case evCommand: {
      uint16_t code = (uint16_t)HIWORD(wparam);

      if (code == kMenuBarNotificationItemClick) {
        gc_handle_command(LOWORD(wparam));
        return true;
      }

      if (code == btnClicked || code == 0) {
        uint16_t id = (uint16_t)LOWORD(wparam);
        if (id == ID_MAIN_WINDOW_COMMIT_NOW) {
          char summary[256] = {0}, desc[512] = {0}, message[800] = {0};
          window_t *sw = get_window_item(win, ID_MAIN_WINDOW_COMMIT_SUMMARY);
          window_t *dw = get_window_item(win, ID_MAIN_WINDOW_COMMIT_DESCRIPTION);
          if (sw) send_message(sw, edGetText, sizeof(summary), summary);
          if (dw) send_message(dw, edGetText, sizeof(desc), desc);
          if (!summary[0]) { message_box(win, "Enter a commit summary.", "Commit", MB_OK); return true; }
          char identity_name[128], identity_email[256];
          if (!git_get_identity(gc->repo, identity_name, sizeof(identity_name), identity_email, sizeof(identity_email))) {
            gc_show_identity_dialog(win); return true;
          }
          snprintf(message, sizeof(message), "%s%s%s", summary, desc[0] ? "\n\n" : "", desc);
          if (gc_commit(message, false)) {
            if (sw) send_message(sw, edSetText, 0, "");
            if (dw) send_message(dw, edSetText, 0, "");
            gc_refresh_all();
          } else message_box(win, "Commit failed. Check staged files and Git identity.", "Commit", MB_OK);
          return true;
        }
        if (id == ID_MAIN_WINDOW_STASH_INLINE) {
          gc_stash(); gc_refresh_all(); return true;
        }
        gc_handle_command(id);
        return true;
      }

      if (code == RVN_SELCHANGE) {
        if (!gc) return true;
        int sel   = (int)(int16_t)LOWORD(wparam);
        window_t *src = (window_t *)lparam;

        if (src == gc->branches_win) {
          // Branch changed: reset downstream indices so the cascade refresh
          // on log_win row 0 and files_win row 0 is never suppressed.
          gc->selected_commit = -1;
          gc->selected_file   = -1;
        } else if (src == gc->log_win) {
          if (sel != gc->selected_commit) {
            gc->selected_commit = sel;
            gc->selected_file   = -1;
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

      if (code == RVN_ITEMCHECK) {
        if (!gc || (window_t *)lparam != gc->files_win) return true;
        int row = (int)(int16_t)LOWORD(wparam);
        db_file_t *file = (db_file_t *)(intptr_t)send_message(
          gc->files_win, tvGetRecord, (uint32_t)row, NULL);
        bool checked = ReportView_GetCheckState(gc->files_win, row);
        if (file) {
          bool ok = checked ? gc_stage_file(file->path) : gc_unstage_file(file->path);
          if (!ok) message_box(gc->main_win, "Operation failed.", "File", MB_OK);
          gc_refresh_all();
        }
        return true;
      }

      if (code == GC_DIFF_TOGGLE_UNIFIED) {
        if (!gc) return true;
        if (gc->diff_win) {
          gc_diff_state_t *st = (gc_diff_state_t *)gc->diff_win->userdata;
          if (st) {
            gc->unified_diff = st->unified_mode;
            gc_diff_refresh();
          }
        }
        return true;
      }

      if (code == GC_DIFF_STAGE_HUNK) {
        if (!gc) return true;
        int hunk_idx = (int)(int16_t)LOWORD(wparam);
        if (gc->diff_win) {
          gc_diff_state_t *st = (gc_diff_state_t *)gc->diff_win->userdata;
          if (st && st->hunk_path[0]) {
            gc_stage_hunk(st->hunk_path, hunk_idx);
            gc_refresh_all();
          }
        }
        return true;
      }

      if (code == RVN_DBLCLK) {
        if (!gc) return false;
        int idx       = (int)(int16_t)LOWORD(wparam);
        window_t *src = (window_t *)lparam;

        if (src == gc->stash_win) {
          gc_stash_pop();
          gc_refresh_all();
          return true;
        }

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
        if (!res->success) {
          message_box(win, res->output, "Operation failed", MB_OK);
        } else if (res->op == GIT_OP_CLONE) {
          gc_state_t *gc_ = g_gc;
          if (gc_ && gc_->clone_path[0])
            gc_open_repo(gc_->clone_path);
          gc_->clone_path[0] = '\0';
        } else {
          gc_refresh_all();
        }
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
