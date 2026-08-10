#include "gitclient.h"

typedef struct {
  char name[128];
  char email[256];
  bool global;
} identity_state_t;

static const ctrl_binding_t identity_bindings[] = {
  DDX_TEXT (ID_IDENTITY_DIALOG_NAME,   identity_state_t, name),
  DDX_TEXT (ID_IDENTITY_DIALOG_EMAIL,  identity_state_t, email),
  DDX_CHECK(ID_IDENTITY_DIALOG_GLOBAL, identity_state_t, global),
};

static result_t identity_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  identity_state_t *st = (identity_state_t *)win->userdata;
  (void)lparam;

  if (msg == evCreate) {
    win->userdata = lparam; st = (identity_state_t *)lparam;
    if (g_gc && g_gc->repo)
      git_get_identity(g_gc->repo, st->name, sizeof(st->name), st->email, sizeof(st->email));
    dialog_push(win, st, identity_bindings, ARRAY_LEN(identity_bindings));
    return true;
  }
  if (msg != evCommand || HIWORD(wparam) != btnClicked) return false;
  uint16_t id = LOWORD(wparam);
  if (id == ID_IDENTITY_DIALOG_CANCEL) { end_dialog(win, 0); return true; }
  if (id == ID_IDENTITY_DIALOG_OK) {
    dialog_pull(win, st, identity_bindings, ARRAY_LEN(identity_bindings));
    if (g_gc && git_set_identity(g_gc->repo, st->name, st->email, st->global))
      end_dialog(win, 1);
    else
      set_window_item_text(win, ID_IDENTITY_DIALOG_STATUS, "Enter a valid name and email address.");
    return true;
  }
  return false;
}

void gc_show_identity_dialog(window_t *parent) {
  if (!g_gc || !g_gc->repo) return;
  identity_state_t st = {0};
  show_dialog_from_form(&gitclient_identity_dialog_form, "Git Identity", parent, identity_proc, &st);
}
