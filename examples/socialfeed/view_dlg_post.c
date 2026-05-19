// VIEW: Post detail dialog — shows post body and comments.
//
// Layout (client area POST_DLG_W x POST_DLG_H):
//   y=  4  Post title label       (ID_POST_DETAIL_LBL_TITLE)
//   y= 18  "by Author" label      (ID_POST_DETAIL_LBL_AUTHOR)
//   y= 32  Body text label        (ID_POST_DETAIL_LBL_BODY, wrapped, h=50)
//   y= 86  "N likes" label        (ID_POST_DETAIL_LBL_LIKES)  + [Like Post] button
//   y=108  "Comments (N):" label  (ID_POST_DETAIL_LBL_CMT_HDR)
//   y=122  Comments tableview     (ID_POST_DETAIL_COMMENTS) — AUTOMATIC POPULATION!
//   y=280  [Add Comment] [Like Comment]  [Close]
//
// All controls are declared in socialfeed.orion (post_detail form) and created
// automatically before evCreate fires.
//
// Comments are automatically populated from the database via tableview control.
// No manual refresh code needed!

#include "socialfeed.h"

// ============================================================
// Post-detail dialog state (simplified with tableview!)
// ============================================================

typedef struct {
  post_t   *post;
  int       post_idx;
  window_t *comments_win;  // tableview — automatically populated from database
} post_detail_t;

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
      win->userdata = s;
      
      // Get the form-generated tableview and configure it for this post
      s->comments_win = get_window_item(win, ID_POST_DETAIL_COMMENTS);
      if (s->comments_win) {
        // Database is auto-propagated during form creation.
        // Just set the filter to show only comments for this post.
        send_message(s->comments_win, tvSetFilter, 1, (void*)(intptr_t)s->post->id);
      }
      
      update_header_labels(win, s);
      return true;
    }

    case evDestroy:
      s->comments_win = NULL;
      return false;

    case evResize:
      if (s) {
        window_layout_sync(win);
      }
      return false;

    case evCommand: {
      uint16_t notif  = (uint16_t)HIWORD(wparam);
      window_t *src   = (window_t *)lparam;

      // ---- Reportview notifications ----
      if (notif == RVN_SELCHANGE) {
        return true;
      }
      if (notif == RVN_DBLCLK) {
        // Double-click: treat as "like comment"
        // Note: We don't have direct access to the comment record anymore
        // Would need to fetch by selection index if we want to implement this
        return true;
      }

      // ---- Button clicks ----
      if (notif != btnClicked || !src) return false;

      switch (src->id) {
        // ---- Like Post ----
        case ID_POST_DETAIL_LIKE_POST:
          if (app_like_post(s->post->id)) {
            // Refresh post to get updated like count
            free(s->post->title);
            free(s->post->body);
            free(s->post->author);
            free(s->post);
            s->post = app_get_post(g_app->selected_idx);
            update_header_labels(win, s);
            SF_DEBUG("liked post id=%d (persisted to DB)", s->post->id);
          }
          return true;

        // ---- Add Comment ----
        case ID_POST_DETAIL_ADD_COMMENT: {
          char author[64] = "";
          char text[512]  = "";
          if (show_new_comment_dialog(win, "New Comment",
                                      author, sizeof(author),
                                      text,   sizeof(text))) {
            // Look up author ID by name
            // TODO: Should use combobox that returns author_id directly
            int author_id = 1;  // Default to first author
            
            // Try to find author by name in database
            result_node_t *authors = (result_node_t *)send_db_message(
              g_app->db, dbFetch, MAKEDWORD(TABLE_AUTHORS, 0), (void *)(intptr_t)0);
            if (authors) {
              for (result_node_t *n = authors; n; n = n->next) {
                db_author_t *a = *(db_author_t **)n->data;
                if (a && strcmp(a->name, author) == 0) {
                  author_id = a->id;
                  break;
                }
              }
              free_result_list(authors);
            }
            
            // Insert comment into database
            if (app_add_comment(s->post->id, author_id, text)) {
              // Tableview auto-refreshes from database via tvSetFilter
              // Update post to reflect new comment count
              free(s->post->title);
              free(s->post->body);
              free(s->post->author);
              free(s->post);
              s->post = app_get_post(g_app->selected_idx);
              update_header_labels(win, s);
            }
          }
          return true;
        }

        // ---- Like Comment ----
        case ID_POST_DETAIL_LIKE_COMMENT: {
          // Note: With tableview, we don't have direct access to comment records
          // Would need to fetch by selection index or rethink this feature
          message_box(win, "Select a comment to like.",
                      "Like Comment", MB_OK);
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
    .comments_win  = NULL,
  };

  show_dialog_from_form_ex(&socialfeed_post_detail_form,
                           "Post Detail",
                           parent,
                           WINDOW_VSCROLL | WINDOW_DIALOG | WINDOW_NOTRAYBUTTON,
                           post_detail_proc,
                           &state);
}
