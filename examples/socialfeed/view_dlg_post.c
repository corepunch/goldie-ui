// VIEW: Post detail dialog — shows post body, threaded comments, and likes.
//
// Layout (client area POST_DLG_W x POST_DLG_H):
//   y=  4  Post title label       (ID_POST_DETAIL_LBL_TITLE)
//   y= 18  "by Author" label      (ID_POST_DETAIL_LBL_AUTHOR)
//   y= 32  Body text label        (ID_POST_DETAIL_LBL_BODY, wrapped, h=50)
//   y= 86  "N likes" label        (ID_POST_DETAIL_LBL_LIKES)  + [Like Post] button
//   y=108  "Comments (N):" label  (ID_POST_DETAIL_LBL_CMT_HDR)
//   y=122  Comments reportview    (ID_POST_DETAIL_COMMENTS)
//   y=280  [Add Comment] [Add Reply] [Like Comment]  [Close]
//
// All controls are declared in socialfeed.orion (post_detail form) and created
// automatically before evCreate fires.

#include "socialfeed.h"

#define COMMENT_CELL_TEXT_MAX 256

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

static const db_binding_column_t kCommentFallbackCols[] = {
  { "author", "Author", 70 },
  { "text", "Text", 0 },
  { "like_count", "Likes", 45 },
};

static const db_view_binding_t kCommentFallbackBinding = {
  .name = "post_comments_report",
  .source = "feed_comments",
  .view = "comments",
  .columns = kCommentFallbackCols,
  .column_count = (int)(sizeof(kCommentFallbackCols) / sizeof(kCommentFallbackCols[0])),
};

static const db_view_binding_t *comment_binding(void) {
  const db_view_binding_t *binding =
      db_api_find_binding_for_view(&socialfeed_database_api, "comments");
  if (!binding || !binding->columns || binding->column_count <= 0)
    return &kCommentFallbackBinding;
  return binding;
}

static int comment_visible_column_count(const db_view_binding_t *binding) {
  int cols = binding ? binding->column_count : 0;
  if (cols > REPORTVIEW_MAX_SUBITEMS + 1)
    cols = REPORTVIEW_MAX_SUBITEMS + 1;
  if (cols <= 0)
    cols = 1;
  return cols;
}

static int comment_primary_width(window_t *win, const db_view_binding_t *binding) {
  irect16_t cr = get_client_rect(win);
  int fixed = 0;
  int cols = comment_visible_column_count(binding);
  for (int i = 1; i < cols; i++) {
    if (binding->columns[i].width > 0)
      fixed += binding->columns[i].width;
  }
  int avail = cr.w - fixed;
  return (avail < 20) ? 20 : avail;
}

static int comment_author_column_index(const db_view_binding_t *binding, int col_count) {
  if (!binding || !binding->columns) return -1;
  for (int i = 0; i < col_count; i++) {
    if (binding->columns[i].field && !strcmp(binding->columns[i].field, "author"))
      return i;
  }
  return -1;
}

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
  const db_view_binding_t *binding = comment_binding();
  int col_count = comment_visible_column_count(binding);
  int author_col = comment_author_column_index(binding, col_count);

  build_flat(s);

  send_message(cv, RVM_SETREDRAW, 0, NULL);
  send_message(cv, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
  send_message(cv, RVM_CLEARCOLUMNS, 0, NULL);

  for (int i = 0; i < col_count; i++) {
    int width = binding->columns[i].width;
    if (i == 0 && width <= 0)
      width = comment_primary_width(cv, binding);
    reportview_column_t col = {
      .title = binding->columns[i].title,
      .width = (uint32_t)((width > 0) ? width : 0),
    };
    send_message(cv, RVM_ADDCOLUMN, 0, &col);
  }

  send_message(cv, RVM_CLEAR, 0, NULL);

  for (int i = 0; i < s->flat_count; i++) {
    flat_item_t *f    = &s->flat[i];
    comment_t   *item = flat_to_comment(s, i);
    if (!item) continue;

    char cell_buf[REPORTVIEW_MAX_SUBITEMS + 1][COMMENT_CELL_TEXT_MAX];
    for (int c = 0; c < col_count; c++) {
      const char *field = binding->columns[c].field;
      if (!socialfeed_comment_field_text(item, field, cell_buf[c], sizeof(cell_buf[c]))) {
        cell_buf[c][0] = '\0';
        SF_DEBUG("binding '%s' references unmapped comment field '%s' (add it to socialfeed_comment_field_text)",
                 binding->name ? binding->name : "",
                 field ? field : "");
      }
    }
    if (f->is_reply && author_col >= 0) {
      char author_buf[COMMENT_CELL_TEXT_MAX];
      snprintf(author_buf, sizeof(author_buf), "→ %s", cell_buf[author_col]);
      strncpy(cell_buf[author_col], author_buf, sizeof(cell_buf[author_col]) - 1);
      cell_buf[author_col][sizeof(cell_buf[author_col]) - 1] = '\0';
    }

    reportview_item_t row = {
      .text          = cell_buf[0],
      .icon          = f->is_reply ? -1 : icon8_editor_helmet,
      .color         = get_sys_color(f->is_reply ? brTextDisabled : brTextNormal),
      .userdata      = (uint32_t)i,
      .subitem_count = (uint32_t)((col_count > 0) ? (col_count - 1) : 0),
    };
    for (int c = 1; c < col_count; c++)
      row.subitems[c - 1] = cell_buf[c];
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

  set_window_item_text(win, ID_POST_DETAIL_LBL_TITLE,    "%s",  p->title);
  set_window_item_text(win, ID_POST_DETAIL_LBL_AUTHOR,   "by %s", p->author);
  set_window_item_text(win, ID_POST_DETAIL_LBL_BODY,     "%s",  p->body);
  set_window_item_text(win, ID_POST_DETAIL_LBL_LIKES,
                       p->like_count == 1 ? "%d like" : "%d likes",
                       p->like_count);
  set_window_item_text(win, ID_POST_DETAIL_LBL_CMT_HDR,
                       p->comment_count == 1 ? "%d comment" : "%d comments",
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
      s = (post_detail_t *)lparam;
      win->userdata    = s;
      s->selection     = (flat_sel_t){ -1, -1 };
      s->comments_win = get_window_item(win, ID_POST_DETAIL_COMMENTS);
      update_header_labels(win, s);
      window_layout_sync(win);
      refresh_comments(s);
      return true;
    }

    case evDestroy:
      if (s) {
        free(s->flat);
        s->flat = NULL;
        s->flat_cap = 0;
        s->comments_win = NULL;
      }
      return false;

    case evResize:
      if (s) {
        window_layout_sync(win);
        refresh_comments(s);
      }
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
        case ID_POST_DETAIL_LIKE_POST:
          post_like(s->post);
          update_header_labels(win, s);
          SF_DEBUG("liked post id=%d likes=%d", s->post->id, s->post->like_count);
          return true;

        // ---- Add Comment ----
        case ID_POST_DETAIL_ADD_COMMENT: {
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
        case ID_POST_DETAIL_ADD_REPLY: {
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
        case ID_POST_DETAIL_LIKE_COMMENT: {
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
        case ID_POST_DETAIL_CLOSE:
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

  show_dialog_from_form_ex(&socialfeed_post_detail_form,
                           "Post Detail",
                           parent,
                           WINDOW_VSCROLL | WINDOW_DIALOG | WINDOW_NOTRAYBUTTON,
                           post_detail_proc,
                           &state);
}
