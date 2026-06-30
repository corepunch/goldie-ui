// Unified diff viewer — custom window proc using VGA monospace font.
//
// Reads selected file/commit from DB to determine what diff to show.

#include "gitclient.h"
#include "../../user/vga_font.h"
#include "../../user/ansi.h"
#include "../../user/vga_text.h"

// ============================================================
// Colours (0xAARRGGBB)
// ============================================================

#define CLR_ADD_BG   0xFF1A3A1A
#define CLR_ADD_FG   0xFF66FF66
#define CLR_DEL_BG   0xFF3A1A1A
#define CLR_DEL_FG   0xFFFF6666
#define CLR_HUNK_BG  0xFF1A1A3A
#define CLR_HUNK_FG   0xFF66CCFF
#define CLR_CTX_BG   0xFF1E1E1E
#define CLR_CTX_FG   0xFFCCCCCC
#define CLR_LNUM_BG  0xFF2A2A2A
#define CLR_LNUM_FG  0xFF888888
#define CLR_HEADER_BG 0xFF2E2E2E
#define CLR_HEADER_FG 0xFFAAAAAA

#define LINE_NUM_W    (5 * VGA_CHAR_W)

#ifndef GC_DIFF_USE_PREFIX_COLORS
#define GC_DIFF_USE_PREFIX_COLORS 0
#endif

// ============================================================
// Per-window state
// ============================================================

typedef struct {
  char  **lines;
  int     line_count;
  int     scroll_y;
  vga_text_grid_t grid;
  char    diff_buf[256 * 1024];
} diff_state_t;

static int visible_lines(window_t *win) {
  return MAX(1, win->frame.h / VGA_CHAR_H);
}

static int max_scroll_start(window_t *win, const diff_state_t *st) {
  if (!win || !st) return 0;
  return MAX(0, st->line_count - visible_lines(win));
}

// ============================================================
// Refresh: get diff from git based on DB selection
// ============================================================

void gc_diff_refresh(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->diff_win) return;

  window_t *win = gc->diff_win;
  diff_state_t *st = (diff_state_t *)win->userdata;
  if (!st) return;

  free(st->lines);
  st->lines      = NULL;
  st->line_count = 0;
  st->scroll_y   = 0;

  if (!gc->repo || !gc->db) {
    invalidate_window(win);
    return;
  }

  // Determine what diff to show from DB state.
  const char *path = NULL;
  bool staged = false;

  if (gc->selected_file >= 0) {
    // Fetch the selected file from DB.
    result_node_t *files = (result_node_t *)send_db_message(
      gc->db, dbFetch, MAKEDWORD(ID_DB_FILES, 0), (void *)(intptr_t)0);
    result_node_t *node = files;
    for (int i = 0; i < gc->selected_file && node; i++) node = node->next;
    if (node) {
      db_file_t *f = *(db_file_t **)node->data;
      path = f->path;
      staged = f->staged;
    }
    free_result_list(files);
  }

  if (gc->selected_commit >= 0) {
    // Fetch the selected commit from DB.
    result_node_t *commits = (result_node_t *)send_db_message(
      gc->db, dbFetch, MAKEDWORD(ID_DB_COMMITS, 0), (void *)(intptr_t)0);
    result_node_t *node = commits;
    for (int i = 0; i < gc->selected_commit && node; i++) node = node->next;
    if (node) {
      db_commit_t *c = *(db_commit_t **)node->data;
      if (path && path[0]) {
        const char *args[] = {
          "git", "show", "--color=always", "--pretty=format:", c->hash,
          "--", path, NULL
        };
        git_run_sync(gc->repo, args, st->diff_buf, sizeof(st->diff_buf));
      } else {
        const char *args[] = {
          "git", "show", "--color=always", "--pretty=format:", c->hash, NULL
        };
        git_run_sync(gc->repo, args, st->diff_buf, sizeof(st->diff_buf));
      }
    }
    free_result_list(commits);
  } else {
    git_get_diff(gc->repo, path, staged, st->diff_buf, sizeof(st->diff_buf));
  }

  if (!st->diff_buf[0]) {
    invalidate_window(win);
    return;
  }

  // Count lines.
  int count = 0;
  for (char *p = st->diff_buf; *p; p++)
    if (*p == '\n') count++;
  if (st->diff_buf[0]) count++;

  st->lines = (char **)malloc((size_t)count * sizeof(char *));
  if (!st->lines) { invalidate_window(win); return; }

  char *p = st->diff_buf;
  while (*p) {
    st->lines[st->line_count++] = p;
    char *nl = strchr(p, '\n');
    if (!nl) break;
    *nl = '\0';
    p = nl + 1;
  }

  scroll_info_t si = {
    .fMask = SIF_RANGE | SIF_PAGE | SIF_POS,
    .nMin  = 0,
    .nMax  = st->line_count,
    .nPage = (uint32_t)visible_lines(win),
    .nPos  = 0,
  };
  set_scroll_info(win, SB_VERT, &si, false);
  invalidate_window(win);
}

// ============================================================
// Window procedure
// ============================================================

result_t gc_diff_proc(window_t *win, uint32_t msg,
                      uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate: {
      diff_state_t *st = (diff_state_t *)calloc(1, sizeof(diff_state_t));
      win->userdata = st;
      return true;
    }

    case evDestroy: {
      diff_state_t *st = (diff_state_t *)win->userdata;
      if (st) {
        free(st->lines);
        vga_text_free_grid(&st->grid);
        free(st);
        win->userdata = NULL;
      }
      return false;
    }

    case evResize: {
      diff_state_t *st = (diff_state_t *)win->userdata;
      if (!st) return false;
      st->scroll_y = CLAMP(st->scroll_y, 0, max_scroll_start(win, st));
      scroll_info_t si = {
        .fMask = SIF_PAGE | SIF_POS,
        .nPage = (uint32_t)visible_lines(win),
        .nPos = st->scroll_y,
      };
      set_scroll_info(win, SB_VERT, &si, true);
      invalidate_window(win);
      return false;
    }

    case evVScroll: {
      diff_state_t *st = (diff_state_t *)win->userdata;
      if (!st) return false;
      st->scroll_y = CLAMP((int)wparam, 0, max_scroll_start(win, st));
      scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_y };
      set_scroll_info(win, SB_VERT, &si, false);
      invalidate_window(win);
      return true;
    }

    case evWheel: {
      diff_state_t *st = (diff_state_t *)win->userdata;
      if (!st || !st->line_count) return false;
      int delta = (int16_t)HIWORD((uintptr_t)lparam);
      if (delta == 0) return false;
      int lines = delta < 0 ? 3 : -3;
      int max_start = max_scroll_start(win, st);
      int new_pos = CLAMP(st->scroll_y + lines, 0, max_start);
      if (new_pos != st->scroll_y) {
        st->scroll_y = new_pos;
        scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_y };
        set_scroll_info(win, SB_VERT, &si, false);
        invalidate_window(win);
      }
      return true;
    }

    case evPaint: {
      diff_state_t *st = (diff_state_t *)win->userdata;
      irect16_t cr = get_client_rect(win);

      fill_rect(CLR_CTX_BG, cr);

      if (!st || !st->lines) {
        if (!g_gc || !g_gc->repo) {
          draw_text_small("No repository open.", cr.x + 4, cr.y + 4,
                          get_sys_color(brTextDisabled));
        } else {
          draw_text_small("Select a file to view its diff.",
                          cr.x + 4, cr.y + 4,
                          get_sys_color(brTextDisabled));
        }
        return true;
      }

      int vis   = visible_lines(win);
      int start = CLAMP(st->scroll_y, 0, max_scroll_start(win, st));
      int end   = MIN(start + vis, st->line_count);

      int text_w = cr.w - LINE_NUM_W;
      if (text_w < 8) text_w = 8;

      int max_cols = text_w / VGA_CHAR_W;
      int gutter_cols = LINE_NUM_W / VGA_CHAR_W;
      int total_cols = gutter_cols + max_cols;

      if (total_cols <= 0 || vis <= 0) return true;

      if (!vga_text_ensure_grid(&st->grid, total_cols, vis)) return true;

      int def_fg_idx = nearest_ansi_index(CLR_CTX_FG);
      int def_bg_idx = nearest_ansi_index(CLR_CTX_BG);
      vga_text_clear_grid(&st->grid, def_fg_idx, def_bg_idx);

      int lnum_fg_idx = nearest_ansi_index(CLR_LNUM_FG);
      int lnum_bg_idx = nearest_ansi_index(CLR_LNUM_BG);

      for (int li = start; li < end; li++) {
        const char *line = st->lines[li];
        int row = li - start;

        uint32_t fg = CLR_CTX_FG;
        uint32_t bg = CLR_CTX_BG;
#if GC_DIFF_USE_PREFIX_COLORS
        char first = line[0];
        if (first == '+') { fg = CLR_ADD_FG; bg = CLR_ADD_BG; }
        else if (first == '-') { fg = CLR_DEL_FG; bg = CLR_DEL_BG; }
        else if (first == '@') { fg = CLR_HUNK_FG; bg = CLR_HUNK_BG; }
        else if (first == 'd' || first == 'i' || first == 'n' || first == 'B')
          { fg = CLR_HEADER_FG; bg = CLR_HEADER_BG; }
#endif

        char lnum[16];
        snprintf(lnum, sizeof(lnum), "%4d ", li + 1);
        for (int c = 0; c < gutter_cols; c++) {
          unsigned char ch = (unsigned char)(lnum[c] ? lnum[c] : ' ');
          vga_text_set_cell(&st->grid, c, row, ch, lnum_fg_idx, lnum_bg_idx);
        }

        vga_text_write_ansi_line(line, &st->grid, row, gutter_cols, max_cols, fg, bg);
      }

      if (R_UpdateTextureRG8(st->grid.cells_tex, 0, 0, st->grid.cells_w, st->grid.cells_h, st->grid.cells)) {
        R_VgaBuffer buf = {
          .vga_buffer = st->grid.cells_tex,
          .width = st->grid.cells_w,
          .height = st->grid.cells_h,
        };
        R_DrawVGABuffer(&buf,
                        cr.x, cr.y,
                        st->grid.cells_w * VGA_CHAR_W,
                        st->grid.cells_h * VGA_CHAR_H,
                        vga_font_texture_id(),
                        kAnsi16);
      }

      return true;
    }

    default:
      return false;
  }
}
