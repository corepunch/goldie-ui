// Clone repository dialog — uses generated form from gitclient.orion.

#include "gitclient.h"

static result_t clone_dlg_proc(window_t *win, uint32_t msg,
                                uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      win->userdata = lparam;
      return true;

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (!src) return false;
        if (src->id == ID_CLONE_DIALOG_CANCEL) {
          end_dialog(win, 0);
          return true;
        }
        if (src->id == ID_CLONE_DIALOG_BROWSE) {
          char path[512] = {0};
          openfilename_t ofn = {0};
          ofn.lStructSize = sizeof(ofn);
          ofn.lpstrFile   = path;
          ofn.nMaxFile    = sizeof(path);
          ofn.Flags       = OFN_PICKFOLDER;
          if (get_folder_name(&ofn))
            set_window_item_text(win, ID_CLONE_DIALOG_PATH, "%s", path);
          return true;
        }
        if (src->id == ID_CLONE_DIALOG_OK) {
          gc_state_t *gc = g_gc;
          if (!gc) { end_dialog(win, 0); return true; }

          char url[512] = {0};
          char path[512] = {0};
          send_message(get_window_item(win, ID_CLONE_DIALOG_URL),
                       edGetText, sizeof(url), url);
          send_message(get_window_item(win, ID_CLONE_DIALOG_PATH),
                       edGetText, sizeof(path), path);

          if (!url[0]) {
            message_box(win, "Please enter a repository URL.", "Clone", MB_OK);
            return true;
          }
          if (!path[0]) {
            message_box(win, "Please select a destination path.", "Clone", MB_OK);
            return true;
          }

          strncpy(gc->clone_path, path, sizeof(gc->clone_path) - 1);
          const char *args[] = { "git", "clone", url, path, NULL };
          git_run_async(gc->repo, GIT_OP_CLONE, args, gc->main_win);
          end_dialog(win, 1);
          return true;
        }
      }
      return false;

    default:
      return false;
  }
}

void gc_show_clone_dialog(window_t *parent) {
  show_dialog_from_form(&gc_clone_dialog_form, "Clone Repository", parent,
                         clone_dlg_proc, NULL);
}
