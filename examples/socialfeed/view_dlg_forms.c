// VIEW: Form-based dialogs — New Comment / Reply.
//
// Form layout (kNewCommentChildren) is generated from socialfeed.orion.
// Only the window procedure and binding table live here.
//
// NOTE: New Post dialog is now handled by show_db_dialog() directly — no
// manual dialog proc needed. See view_menubar.c ID_POST_NEW handler.

#include "socialfeed.h"

// ============================================================
// New Comment / Reply dialog (form-based)
// ============================================================

typedef struct {
  char *author_buf;
  size_t author_sz;
  char *text_buf;
  size_t text_sz;
  bool accepted;
} new_comment_state_t;

static result_t new_comment_proc(window_t *win, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  new_comment_state_t *s = (new_comment_state_t *)win->userdata;

  switch (msg) {
    case evCreate:
      win->userdata = lparam;
      
      // Set database on combobox for auto-population
      {
        database_t *db = get_database_by_name("db");
        if (db) {
          window_t *author_cb = get_window_item(win, ID_NEW_COMMENT_AUTHOR);
          if (author_cb) {
            send_message(author_cb, cbSetDatabase, 0, db);
          }
        }
      }
      
      return true;

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;
        if (src->id == ID_NEW_COMMENT_OK) {
          s = (new_comment_state_t *)win->userdata;

          window_t *ea = get_window_item(win, ID_NEW_COMMENT_AUTHOR);
          window_t *et = get_window_item(win, ID_NEW_COMMENT_TEXT);

          if (!ea || ea->title[0] == '\0') {
            message_box(win, "Author is required.", "Validation", MB_OK);
            return true;
          }
          if (!et || et->title[0] == '\0') {
            message_box(win, "Text is required.", "Validation", MB_OK);
            return true;
          }

          if (s->author_sz < 2 || s->text_sz < 2) {
            end_dialog(win, 0);
            return true;
          }
          strncpy(s->author_buf, ea->title, s->author_sz - 1);
          s->author_buf[s->author_sz - 1] = '\0';
          strncpy(s->text_buf,   et->title, s->text_sz - 1);
          s->text_buf[s->text_sz - 1] = '\0';

          s->accepted = true;
          end_dialog(win, 1);
          return true;
        }
        if (src->id == ID_NEW_COMMENT_CANCEL) {
          end_dialog(win, 0);
          return true;
        }
      }
      return false;

    default:
      return false;
  }
}

bool show_new_comment_dialog(window_t *parent, const char *prompt_title,
                             char *author_buf, size_t author_sz,
                             char *text_buf,   size_t text_sz) {
  new_comment_state_t state = {
    .author_buf = author_buf,
    .author_sz  = author_sz,
    .text_buf   = text_buf,
    .text_sz    = text_sz,
    .accepted   = false,
  };

  show_dialog_from_form_ex(&socialfeed_new_comment_form, prompt_title, parent,
                           WINDOW_DIALOG | WINDOW_NOTRAYBUTTON,
                           new_comment_proc, &state);

  return state.accepted;
}
