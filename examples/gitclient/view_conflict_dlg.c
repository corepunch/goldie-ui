// Conflict resolution dialog — lists conflicted files with resolution options.

#include "gitclient.h"

static void refresh_file_list(window_t *win) {
  window_t *cb = get_window_item(win, ID_CONFLICT_DIALOG_FILE_LIST);
  send_message(cb, cbClear, 0, NULL);
  char files[64][512];
  int n = gc_get_conflicted_files(files, 64);
  for (int i = 0; i < n; i++)
    send_message(cb, cbAddString, 0, files[i]);
  if (n > 0)
    send_message(cb, cbSetCurrentSelection, (uint32_t)0, NULL);
}

static const char *get_selected_file(window_t *win) {
  window_t *cb = get_window_item(win, ID_CONFLICT_DIALOG_FILE_LIST);
  int sel = (int)send_message(cb, cbGetCurrentSelection, 0, NULL);
  if (sel < 0) return NULL;
  return (const char *)send_message(cb, cbGetListBoxText, (uint32_t)sel, NULL);
}

static result_t conflict_dlg_proc(window_t *win, uint32_t msg,
                                   uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      win->userdata = lparam;
      refresh_file_list(win);
      return true;

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (!src) return false;

        if (src->id == ID_CONFLICT_DIALOG_CLOSE) {
          end_dialog(win, 0);
          return true;
        }

        const char *file = get_selected_file(win);
        if (!file) {
          message_box(win, "No conflicted file selected.", "Conflict", MB_OK);
          return true;
        }

        if (src->id == ID_CONFLICT_DIALOG_OURS) {
          gc_conflict_resolve(file, "ours");
          refresh_file_list(win);
          return true;
        }
        if (src->id == ID_CONFLICT_DIALOG_THEIRS) {
          gc_conflict_resolve(file, "theirs");
          refresh_file_list(win);
          return true;
        }
        if (src->id == ID_CONFLICT_DIALOG_BOTH) {
          gc_conflict_resolve(file, "both");
          refresh_file_list(win);
          return true;
        }
        if (src->id == ID_CONFLICT_DIALOG_ABORT) {
          gc_abort_merge();
          end_dialog(win, 0);
          gc_refresh_all();
          return true;
        }
      }
      return false;

    default:
      return false;
  }
}

void gc_show_conflict_dialog(window_t *parent) {
  show_dialog_from_form(&gc_conflict_dialog_form, "Merge Conflicts", parent,
                         conflict_dlg_proc, NULL);
  gc_refresh_all();
}
