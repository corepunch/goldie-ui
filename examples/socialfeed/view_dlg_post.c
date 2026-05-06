// VIEW: Post detail dialog — shows post body, threaded comments, and likes.
//
// Layout (client area POST_DLG_W x POST_DLG_H):
//   y=  4  Post title label       (ID_LBL_POST_TITLE)
//   y= 18  "by Author" label      (ID_LBL_POST_AUTHOR)
//   y= 32  Body text label        (ID_LBL_POST_BODY, wrapped, h=50)
//   y= 86  "N likes" label        (ID_LBL_POST_LIKES)  + [Like Post] button
//   y=108  "Comments (N):" label  (ID_LBL_COMMENTS_HDR)
//   y=122  Comments reportview    (ID_COMMENTS_VIEW)
//   y=280  [Add Comment] [Add Reply] [Like Comment]  [Close]
//
// All controls are declared in socialfeed.orion (post_detail form) and created
// automatically before evCreate fires.

#include "socialfeed.h"

// ============================================================
// Flat comment item — represents one row in the comment list
// (either a top-level comment or a reply)
// ============================================================

typedef struct {
  bool is_reply;
  int  comment_idx; // index into post->comments[]
  int  reply_idx;   // index into comment->replies[] (-1 for top-level)
} flat_item_t;

// ============================================================
// Stable selection identity — survives flat-list rebuilds
// ============================================================

typedef struct {
  int comment_idx;  // index into post->comments[], or -1 for no selection
  int reply_idx;    // index into comment->replies[], or -1 for top-level
} flat_sel_t;

// ============================================================
// Post-detail dialog state
// ============================================================

typedef struct {
  post_t      *post;
  int          post_idx;
  flat_item_t *flat;        // dynamically allocated; freed in evDestroy
  int          flat_count;
  int          flat_cap;
  flat_sel_t   selection;   // stable identity; resolved to flat index on demand
  window_t    *comments_win;
} post_detail_t;

// ============================================================
// build_flat — flatten comments+replies into the flat[] array
// ============================================================

static void build_flat(post_detail_t *s) {
  s->flat_count = 0;
  for (int ci = 0; ci < s->post->comment_count; ci++) {
    comment_t *c = s->post->comments[ci];
    if (!c) continue;

    int need = s->flat_count + 1 + c->reply_count;
    if (need > s->flat_cap) {
      int new_cap = need + 32;
      flat_item_t *p = realloc(s->flat, (size_t)new_cap * sizeof(flat_item_t));
      if (!p) continue;
      s->flat     = p;
      s->flat_cap = new_cap;
    }

    s->flat[s->flat_count].is_reply    = false;
    s->flat[s->flat_count].comment_idx = ci;
    s->flat[s->flat_count].reply_idx   = -1;
    s->flat_count++;

    for (int ri = 0; ri < c->reply_count; ri++) {
      s->flat[s->flat_count].is_reply    = true;
      s->flat[s->flat_count].comment_idx = ci;
      s->flat[s->flat_count].reply_idx   = ri;
      s->flat_count++;
    }
  }
}

// ============================================================
// selection_to_flat — resolve stable identity → current flat index
// ============================================================

static int selection_to_flat(post_detail_t *s) {
  if (s->selection.comment_idx < 0) return -1;
  for (int i = 0; i < s->flat_count; i++) {
    flat_item_t *f = &s->flat[i];
    if (f->comment_idx == s->selection.comment_idx &&
        f->reply_idx   == s->selection.reply_idx)
      return i;
  }
  return -1;
}

// ============================================================
// get_flat_item — return comment_t* for a flat row
// ============================================================

static comment_t *flat_to_comment(post_detail_t *s, int fi) {
  if (fi < 0 || fi >= s->flat_count) return NULL;
  flat_item_t *f = &s->flat[fi];
  comment_t   *c = s->post->comments[f->comment_idx];
  if (!c) return NULL;
  return f->is_reply ? c->replies[f->reply_idx] : c;
}

// ============================================================
// refresh_comments — rebuild comment reportview
// ============================================================

static void refresh_comments(post_detail_t *s) {
  if (!s || !s->comments_win) return;
  window_t *cv = s->comments_win;

  build_flat(s);

  send_message(cv, RVM_SETREDRAW, 0, NULL);
  send_message(cv, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
  send_message(cv, RVM_CLEARCOLUMNS, 0, NULL);

  irect16_t cr  = get_client_rect(cv);
  int cv_w   = cr.w;
  int auth_w  = 70;
  int like_w  = 45;
  int text_w  = cv_w - auth_w - like_w;
  if (text_w < 20) text_w = 20;

  reportview_column_t col_author = { "Author",  (uint32_t)auth_w };
  reportview_column_t col_text   = { "Text",    (uint32_t)text_w };
  reportview_column_t col_likes  = { "Likes",   (uint32_t)like_w };

  send_message(cv, RVM_ADDCOLUMN, 0, &col_author);
  send_message(cv, RVM_ADDCOLUMN, 0, &col_text);
  send_message(cv, RVM_ADDCOLUMN, 0, &col_likes);

  send_message(cv, RVM_CLEAR, 0, NULL);

  char author_buf[128];
  char likes_buf[16];

  for (int i = 0; i < s->flat_count; i++) {
    flat_item_t *f    = &s->flat[i];
    comment_t   *item = flat_to_comment(s, i);
    if (!item) continue;

    snprintf(likes_buf, sizeof(likes_buf), "%d", item->like_count);

    if (f->is_reply) {
      snprintf(author_buf, sizeof(author_buf), "→ %s", item->author);
    } else {
      strncpy(author_buf, item->author, sizeof(author_buf) - 1);
      author_buf[sizeof(author_buf) - 1] = '\0';
    }

    reportview_item_t row = {
      .text          = author_buf,
      .icon          = f->is_reply ? -1 : icon8_editor_helmet,
      .color         = get_sys_color(f->is_reply ? brTextDisabled : brTextNormal),
      .userdata      = (uint32_t)i,
      .subitems      = { item->text, likes_buf },
      .subitem_count = 2,
    };
    send_message(cv, RVM_ADDITEM, 0, &row);
  }

  int sel = selection_to_flat(s);
  if (sel >= 0 && sel < s->flat_count)
    send_message(cv, RVM_SETSELECTION, (uint32_t)sel, NULL);

  send_message(cv, RVM_SETREDRAW, 1, NULL);
}

// ============================================================
// update_header_labels — push current post data into the label controls
// ============================================================

static void update_header_labels(window_t *win, post_detail_t *s) {
  post_t *p = s->post;

  set_window_item_text(win, ID_LBL_POST_TITLE,    "%s",  p->title);
  set_window_item_text(win, ID_LBL_POST_AUTHOR,   "by %s", p->author);
  set_window_item_text(win, ID_LBL_POST_BODY,     "%s",  p->body);
  set_window_item_text(win, ID_LBL_POST_LIKES,
                       p->like_count == 1 ? "%d like" : "%d likes",
                       p->like_count);
  set_window_item_text(win, ID_LBL_COMMENTS_HDR, "Comments (%d):",
                       p->comment_count);
}

// ============================================================
// Post detail dialog window procedure
// ============================================================

static result_t post_detail_proc(window_t *win, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  post_detail_t *s = (post_detail_t *)win->userdata;

  switch (msg) {
    case evCreate: {
      // All children (including the reportview) were created by the form before
      // evCreate fired. Just get the reportview reference.
      s = (post_detail_t *)lparam;
      win->userdata    = s;
      s->selection     = (flat_sel_t){ -1, -1 };

      s->comments_win = get_window_item(win, ID_COMMENTS_VIEW);

      update_header_labels(win, s);
      refresh_comments(s);
      return true;
    }

    case evDestroy:
      if (s) { free(s->flat); s->flat = NULL; s->flat_cap = 0; }
      return false;

    case evCommand: {
      uint16_t notif  = (uint16_t)HIWORD(wparam);
      window_t *src   = (window_t *)lparam;

      // ---- Reportview notifications ----
      if (notif == RVN_SELCHANGE) {
        int fi = (int)(int16_t)LOWORD(wparam);
        if (fi >= 0 && fi < s->flat_count) {
          s->selection.comment_idx = s->flat[fi].comment_idx;
          s->selection.reply_idx   = s->flat[fi].reply_idx;
        } else {
          s->selection = (flat_sel_t){ -1, -1 };
        }
        return true;
      }
      if (notif == RVN_DBLCLK) {
        // Double-click: treat as "like comment"
        comment_t *c = flat_to_comment(s, selection_to_flat(s));
        if (c) {
          comment_like(c);
          refresh_comments(s);
        }
        return true;
      }

      // ---- Button clicks ----
      if (notif != btnClicked || !src) return false;

      switch (src->id) {
        // ---- Like Post ----
        case ID_BTN_LIKE_POST:
          post_like(s->post);
          update_header_labels(win, s);
          SF_DEBUG("liked post id=%d likes=%d", s->post->id, s->post->like_count);
          return true;

        // ---- Add Comment ----
        case ID_BTN_ADD_COMMENT: {
          char author[64] = "";
          char text[512]  = "";
          if (show_new_comment_dialog(win, "New Comment",
                                      author, sizeof(author),
                                      text,   sizeof(text))) {
            comment_t *c = comment_create(author, text);
            if (c) {
              app_add_comment(s->post, c);
              refresh_comments(s);
              update_header_labels(win, s);
              SF_DEBUG("comment added post_id=%d comment_id=%d", s->post->id, c->id);
            }
          }
          return true;
        }

        // ---- Add Reply ----
        case ID_BTN_ADD_REPLY: {
          int fi = selection_to_flat(s);
          if (fi < 0 || fi >= s->flat_count) {
            message_box(win, "Select a comment to reply to.",
                        "Add Reply", MB_OK);
            return true;
          }

          // Replies always attach to the top-level comment.
          int ci = s->flat[fi].comment_idx;
          comment_t *parent_c = s->post->comments[ci];
          if (!parent_c) return true;

          char author[64] = "";
          char text[512]  = "";
          char prompt[128];
          snprintf(prompt, sizeof(prompt), "Reply to %s", parent_c->author);

          if (show_new_comment_dialog(win, prompt,
                                      author, sizeof(author),
                                      text,   sizeof(text))) {
            comment_t *reply = comment_create(author, text);
            if (reply) {
              app_add_reply(parent_c, reply);
              refresh_comments(s);
              SF_DEBUG("reply added comment_idx=%d reply_id=%d", ci, reply->id);
            }
          }
          return true;
        }

        // ---- Like Comment ----
        case ID_BTN_LIKE_COMMENT: {
          int fi = selection_to_flat(s);
          comment_t *c = flat_to_comment(s, fi);
          if (!c) {
            message_box(win, "Select a comment to like.",
                        "Like Comment", MB_OK);
            return true;
          }
          comment_like(c);
          refresh_comments(s);
          SF_DEBUG("liked comment idx=%d likes=%d", fi, c->like_count);
          return true;
        }

        // ---- Close ----
        case ID_BTN_CLOSE:
          end_dialog(win, 0);
          return true;

        default:
          return false;
      }
    }

    case evClose:
      end_dialog(win, 0);
      return true;

    default:
      return false;
  }
}

// ============================================================
// show_post_detail — public entry point
// ============================================================

void show_post_detail(window_t *parent, int post_idx) {
  post_t *p = app_get_post(post_idx);
  if (!p) return;

  post_detail_t state = {
    .post          = p,
    .post_idx      = post_idx,
    .flat          = NULL,
    .flat_count    = 0,
    .flat_cap      = 0,
    .selection     = { -1, -1 },
    .comments_win  = NULL,
  };

  show_dialog_from_form_ex(&socialfeed_post_detail_form, "Post Detail",
                           parent, WINDOW_DIALOG | WINDOW_NOTRAYBUTTON,
                           post_detail_proc, &state);
}
