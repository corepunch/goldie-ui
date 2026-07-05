// Search dialog — filter commits, branches, files by text.

#include "gitclient.h"
#include "../../components/gitclient/diff_view.h"

static result_t search_dlg_proc(window_t *win, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      win->userdata = lparam;
      return true;

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (!src) return false;
        if (src->id == ID_SEARCH_DIALOG_CANCEL) {
          end_dialog(win, 0);
          return true;
        }
        if (src->id == ID_SEARCH_DIALOG_OK) {
          char query[256] = {0};
          send_message(get_window_item(win, ID_SEARCH_DIALOG_QUERY),
                       edGetText, sizeof(query), query);
          gc_state_t *gc = g_gc;
          if (!gc || !gc->repo) { end_dialog(win, 0); return true; }

          // Clear stored filter on the search win (no longer needed)
          // Re-run git log with grep filter
          if (query[0]) {
            char buf[GC_DIFF_BUF_SIZE] = {0};
            const char *args[] = { "git", "log", "--all",
                                   "--oneline", "--grep", query, NULL };
            if (git_run_sync(gc->repo, args, buf, sizeof(buf))) {
              int count = 0;
              for (char *p = buf; *p; p++)
                if (*p == '\n') count++;
              char msg[128];
              snprintf(msg, sizeof(msg), "Found %d matching commits.", count);
              message_box(win, msg, "Search Results", MB_OK);
            }
          }
          end_dialog(win, 1);
          return true;
        }
      }
      return false;

    default:
      return false;
  }
}

void gc_show_search_dialog(window_t *parent) {
  show_dialog_from_form(&gc_search_dialog_form, "Search", parent,
                         search_dlg_proc, NULL);
}
