// Unified diff viewer — fill content into gc_diff_state_t
//
// Reads selected file/commit from DB to determine what diff to show.
// The window proc lives in the gitclient_components plugin (diff_view.c).

#include "gitclient.h"
#include "../../components/gitclient/diff_view.h"
#include "../../user/vga_font.h"

void gc_diff_refresh(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->diff_win) return;

  window_t *win = gc->diff_win;
  gc_diff_state_t *st = (gc_diff_state_t *)win->userdata;
  if (!st) return;

  free(st->lines);
  st->lines      = NULL;
  st->line_count = 0;
  st->scroll_y   = 0;

  if (!gc->repo || !gc->db) {
    invalidate_window(win);
    return;
  }

  const char *path = NULL;
  bool staged = false;

  if (gc->selected_file >= 0) {
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
    .nPage = (uint32_t)MAX(1, win->frame.h / VGA_CHAR_H),
    .nPos  = 0,
  };
  set_scroll_info(win, SB_VERT, &si, false);
  invalidate_window(win);
}
