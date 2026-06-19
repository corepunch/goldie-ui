// Main window — uses generated form from gitclient.orion.

#include "gitclient.h"
#include "vga_font.h"

// ============================================================
// Layout helpers
// ============================================================

static void compute_layout(gc_state_t *gc, irect16_t *cr,
                            irect16_t *r_log,
                            irect16_t *r_files,
                            irect16_t *r_diff) {
  int lw = PANEL_LEFT_W_DEFAULT + PANEL_SPLITTER;
  int rw = gc->right_w;
  int total_w = cr->w;
  int total_h = cr->h;

  rw = CLAMP(rw, 80, total_w - lw - 80);
  gc->right_w = rw;

  int center_x = cr->x + lw;
  int center_w = total_w - lw - rw - PANEL_SPLITTER;
  if (center_w < 20) center_w = 20;

  int vs = CLAMP(gc->vsplit_y, 40, total_h - 40);
  gc->vsplit_y = vs;

  *r_diff  = (irect16_t){ cr->x + total_w - rw, cr->y, rw, total_h };
  *r_log   = (irect16_t){ center_x, cr->y, center_w, vs };
  *r_files = (irect16_t){ center_x, cr->y + vs + PANEL_SPLITTER,
                        center_w, total_h - vs - PANEL_SPLITTER };
}

void gc_layout_panels(window_t *win) {
  gc_state_t *gc = (gc_state_t *)win->userdata;
  if (!gc) return;

  irect16_t cr = get_client_rect(win);
  irect16_t rl, rf, rd;
  compute_layout(gc, &cr, &rl, &rf, &rd);

  if (win->sidebar) {
    int sb_w = win->sidebar->layout.layout_fixed_w;
    if (sb_w <= 0) sb_w = win->sidebar->frame.w;
    if (sb_w <= 0) sb_w = SIDEBAR_DEFAULT_WIDTH;
    move_window(win->sidebar, 0, 0);
    resize_window(win->sidebar, sb_w, cr.h);
  }
  if (gc->log_win) {
    move_window(gc->log_win, rl.x, rl.y);
    resize_window(gc->log_win, rl.w, rl.h);
  }
  if (gc->files_win) {
    move_window(gc->files_win, rf.x, rf.y);
    resize_window(gc->files_win, rf.w, rf.h);
  }
  if (gc->diff_win) {
    move_window(gc->diff_win, rd.x, rd.y);
    resize_window(gc->diff_win, rd.w, rd.h);
  }

  int lw = PANEL_LEFT_W_DEFAULT + PANEL_SPLITTER;
  int center_w = cr.w - lw - gc->right_w - PANEL_SPLITTER;
  if (center_w < 20) center_w = 20;

  if (gc->vsplitter_win) {
    int spl_x = cr.x + lw + center_w;
    move_window(gc->vsplitter_win, spl_x, cr.y);
    resize_window(gc->vsplitter_win, PANEL_SPLITTER, cr.h);
  }
  if (gc->hsplitter_win) {
    int spl_x = cr.x + lw;
    move_window(gc->hsplitter_win, spl_x, cr.y + gc->vsplit_y);
    resize_window(gc->hsplitter_win, center_w, PANEL_SPLITTER);
  }
}

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
  if (gc->log_win)
    send_message(gc->log_win, tvRefresh, 0, NULL);
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

      // Form creates toolbar, statusbar, and sidebar automatically.
      // Look up child windows by generated IDs.
      gc->branches_win = win->sidebar;
      gc->log_win = get_window_item(win, ID_MAIN_WINDOW_LOG);
      gc->files_win = get_window_item(win, ID_MAIN_WINDOW_FILES);
      gc->diff_win = get_window_item(win, ID_MAIN_WINDOW_DIFF);

      // Set database on tableviews.
      if (gc->log_win)
        send_message(gc->log_win, evCreate, 0,
                     (void *)&main_window_log_tableview_params);
      if (gc->files_win)
        send_message(gc->files_win, evCreate, 0,
                     (void *)&main_window_files_tableview_params);

      send_message(win, evStatusBar, 0, "No repository");

      // Create splitters.
      irect16_t cr = get_client_rect(win);
      int lx = PANEL_LEFT_W_DEFAULT + PANEL_SPLITTER;
      int center_w = cr.w - lx - gc->right_w - PANEL_SPLITTER;

      gc->vsplitter_win = create_window("",
          WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_NOTRAYBUTTON,
          MAKERECT(lx + center_w, cr.y, PANEL_SPLITTER, cr.h),
          win, win_splitter, gc->hinstance, (void *)SPLIT_VERT);

      gc->hsplitter_win = create_window("",
          WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_NOTRAYBUTTON,
          MAKERECT(lx, cr.y + gc->vsplit_y, center_w, PANEL_SPLITTER),
          win, win_splitter, gc->hinstance, (void *)SPLIT_HORZ);

      show_window(gc->vsplitter_win, true);
      show_window(gc->hsplitter_win, true);

      // Load VGA font.
      char font_path[600];
      snprintf(font_path, sizeof(font_path),
               "%s/../share/orion/fonts/vga-rom-font-8x16.png",
               ui_get_exe_dir());
      vga_font_init(font_path);

      return true;
    }

    case evDestroy:
      vga_font_shutdown();
      git_repo_close(gc->repo);
      gc->repo = NULL;
      return false;

    case evResize:
      if (gc) gc_layout_panels(win);
      return false;

    case evPaint:
      return false;

    case evMouseMove: {
      if (!gc || !gc->dragging_splitter) return false;
      int mx = (int)LOWORD(wparam);
      int my = (int)HIWORD(wparam);
      int orient = win_splitter_orientation(gc->dragging_splitter);
      int delta  = (orient == SPLIT_HORZ)
                   ? my - gc->drag_start_mouse
                   : mx - gc->drag_start_mouse;
      if (orient == SPLIT_VERT)
        gc->right_w = gc->drag_start_val - delta;
      else
        gc->vsplit_y = gc->drag_start_val + delta;
      gc_layout_panels(win);
      invalidate_window(win);
      return true;
    }

    case evLeftButtonUp:
      if (gc && gc->dragging_splitter) {
        gc->dragging_splitter = NULL;
        set_capture(NULL);
        return true;
      }
      return false;

    case evCommand: {
      uint16_t code = (uint16_t)HIWORD(wparam);

      if (code == btnClicked || code == 0) {
        gc_handle_command((uint16_t)LOWORD(wparam));
        return true;
      }

      if (code == spnDragStart) {
        if (!gc) return false;
        uint32_t pos  = (uint32_t)(uintptr_t)lparam;
        int px = (int)(int16_t)LOWORD(pos);
        int py = (int)(int16_t)HIWORD(pos);
        uint16_t spl_id = (uint16_t)LOWORD(wparam);
        window_t *spl = NULL;
        if (gc->vsplitter_win && gc->vsplitter_win->id == spl_id)
          spl = gc->vsplitter_win;
        else if (gc->hsplitter_win && gc->hsplitter_win->id == spl_id)
          spl = gc->hsplitter_win;
        if (!spl) return false;
        gc->dragging_splitter = spl;
        int orient = win_splitter_orientation(spl);
        gc->drag_start_mouse = (orient == SPLIT_HORZ) ? py : px;
        gc->drag_start_val   = (orient == SPLIT_VERT) ? gc->right_w
                                                       : gc->vsplit_y;
        set_capture(win);
        return true;
      }

      if (code == RVN_SELCHANGE) {
        if (!gc) return true;
        int sel   = (int)(int16_t)LOWORD(wparam);
        window_t *src = (window_t *)lparam;

        if (src == gc->log_win) {
          if (sel != gc->selected_commit) {
            gc->selected_commit = sel;
            gc->selected_file   = -1;
            send_message(gc->files_win, tvRefresh, 0, NULL);
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
