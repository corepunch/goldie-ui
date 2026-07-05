// Create tag dialog — uses generated form from gitclient.orion.

#include "gitclient.h"

static result_t create_tag_proc(window_t *win, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      win->userdata = lparam;
      return true;

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (!src) return false;
        if (src->id == ID_CREATE_TAG_DIALOG_CANCEL) {
          end_dialog(win, 0);
          return true;
        }
        if (src->id == ID_CREATE_TAG_DIALOG_OK) {
          char name[256] = {0};
          char ref[256] = {0};
          send_message(get_window_item(win, ID_CREATE_TAG_DIALOG_NAME),
                       edGetText, sizeof(name), name);
          send_message(get_window_item(win, ID_CREATE_TAG_DIALOG_REF),
                       edGetText, sizeof(ref), ref);
          if (!name[0]) {
            message_box(win, "Please enter a tag name.", "Create Tag", MB_OK);
            return true;
          }
          if (!gc_create_tag(name, ref[0] ? ref : NULL)) {
            message_box(win, "Failed to create tag.", "Create Tag", MB_OK);
            return true;
          }
          end_dialog(win, 1);
          gc_refresh_all();
          return true;
        }
      }
      return false;

    default:
      return false;
  }
}

void gc_show_create_tag_dialog(window_t *parent) {
  show_dialog_from_form(&gc_create_tag_dialog_form, "Create Tag", parent,
                         create_tag_proc, NULL);
}
