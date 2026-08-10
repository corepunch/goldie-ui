#include "gitclient.h"

static result_t identity_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  (void)lparam;
  if (msg == evCreate) {
    char name[128] = {0}, email[256] = {0};
    if (g_gc && g_gc->repo) git_get_identity(g_gc->repo, name, sizeof(name), email, sizeof(email));
    set_window_item_text(win, ID_IDENTITY_DIALOG_NAME, "%s", name);
    set_window_item_text(win, ID_IDENTITY_DIALOG_EMAIL, "%s", email);
    return true;
  }
  if (msg != evCommand || HIWORD(wparam) != btnClicked) return false;
  uint16_t id = LOWORD(wparam);
  if (id == ID_IDENTITY_DIALOG_CANCEL) { end_dialog(win, 0); return true; }
  if (id == ID_IDENTITY_DIALOG_OK) {
    char name[128] = {0}, email[256] = {0};
    window_t *nw = get_window_item(win, ID_IDENTITY_DIALOG_NAME);
    window_t *ew = get_window_item(win, ID_IDENTITY_DIALOG_EMAIL);
    if (nw) send_message(nw, edGetText, sizeof(name), name);
    if (ew) send_message(ew, edGetText, sizeof(email), email);
    bool global = send_message(get_window_item(win, ID_IDENTITY_DIALOG_GLOBAL), btnGetCheck, 0, NULL) != 0;
    if (g_gc && git_set_identity(g_gc->repo, name, email, global)) end_dialog(win, 1);
    else set_window_item_text(win, ID_IDENTITY_DIALOG_STATUS, "Enter a valid name and email address.");
    return true;
  }
  return false;
}

void gc_show_identity_dialog(window_t *parent) {
  if (!g_gc || !g_gc->repo) return;
  show_dialog_from_form(&gitclient_identity_dialog_form, "Git Identity", parent, identity_proc, NULL);
}
