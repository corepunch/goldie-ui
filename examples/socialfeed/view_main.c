// VIEW: Main window — feed list of posts (win_reportview).
//
// TB_FEED and TB_FEED_COUNT are generated from socialfeed.orion and
// declared in the generated forms header included via socialfeed.h.

#include "socialfeed.h"

// ============================================================
// feed_list_proc — thin wrapper that adjusts the Title column
//                  width on resize
// ============================================================

static int feed_title_width(window_t *win) {
  irect16_t cr  = get_client_rect(win);
  int fixed  = FEED_AUTHOR_W + FEED_LIKES_W + FEED_COMMENTS_W;
  int avail  = cr.w - fixed;
  return (avail < 20) ? 20 : avail;
}

result_t feed_list_proc(window_t *win, uint32_t msg,
                        uint32_t wparam, void *lparam) {
  result_t r = win_reportview(win, msg, wparam, lparam);
  if (msg == evResize) {
    send_message(win, RVM_SETREPORTCOLUMNWIDTH, 0,
                 (void *)(uintptr_t)feed_title_width(win));
  }
  return r;
}

// ============================================================
// feed_refresh — rebuild the reportview from g_app->posts
// ============================================================

void feed_refresh(void) {
  if (!g_app || !g_app->feed_win) return;
  window_t *win = g_app->feed_win;

  send_message(win, RVM_SETREDRAW, 0, NULL);
  send_message(win, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
  send_message(win, RVM_CLEARCOLUMNS, 0, NULL);

  reportview_column_t col_title    = { "Title",    0                };
  reportview_column_t col_author   = { "Author",   FEED_AUTHOR_W    };
  reportview_column_t col_likes    = { "Likes",    FEED_LIKES_W     };
  reportview_column_t col_comments = { "Comments", FEED_COMMENTS_W  };

  send_message(win, RVM_ADDCOLUMN, 0, &col_title);
  send_message(win, RVM_ADDCOLUMN, 0, &col_author);
  send_message(win, RVM_ADDCOLUMN, 0, &col_likes);
  send_message(win, RVM_ADDCOLUMN, 0, &col_comments);

  send_message(win, RVM_CLEAR, 0, NULL);

  char likes_buf[16];
  char cmts_buf[16];

  for (int i = 0; i < g_app->post_count; i++) {
    post_t *p = g_app->posts[i];
    if (!p) continue;

    snprintf(likes_buf, sizeof(likes_buf), "%d", p->like_count);
    snprintf(cmts_buf,  sizeof(cmts_buf),  "%d", p->comment_count);

    reportview_item_t item = {
      .text          = p->title,
      .icon          = icon8_editor_helmet,
      .color         = get_sys_color(brTextNormal),
      .userdata      = (uint32_t)i,
      .subitems      = { p->author, likes_buf, cmts_buf },
      .subitem_count = 3,
    };
    send_message(win, RVM_ADDITEM, 0, &item);
  }

  if (g_app->selected_idx >= 0 && g_app->selected_idx < g_app->post_count)
    send_message(win, RVM_SETSELECTION, (uint32_t)g_app->selected_idx, NULL);

  send_message(win, RVM_SETREPORTCOLUMNWIDTH, 0,
               (void *)(uintptr_t)feed_title_width(win));

  send_message(win, RVM_SETREDRAW, 1, NULL);
}

// ============================================================
// main_win_proc
// ============================================================

result_t main_win_proc(window_t *win, uint32_t msg,
                       uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      if (!g_app) return false;
      g_app->main_win = win;

      send_message(win, tbSetItems,
                   TB_FEED_COUNT,
                   (void *)TB_FEED);

      {
        irect16_t cr = get_client_rect(win);
        layout_view_config_t stack_cfg = {
          .layout_kind = "stack",
          .orientation = WINDOW_STACK_VERTICAL,
          .columns = 0,
        };
        g_app->content_win = create_window(
            "", WINDOW_NOTITLE | WINDOW_NOFILL,
            MAKERECT(0, 0, cr.w, cr.h),
            win, "stackview", 0, &stack_cfg);
        if (!g_app->content_win)
          return false;

        g_app->feed_win = create_window(
            "feed",
            WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
            MAKERECT(0, 0, cr.w, cr.h),
            g_app->content_win, feed_list_proc, 0, NULL);
      }

      feed_refresh();
      app_update_status();
      return true;

    case evResize:
      if (g_app && g_app->content_win) {
        irect16_t cr = get_client_rect(win);
        resize_window(g_app->content_win, cr.w, cr.h);
      }
      return false;

    case tbButtonClick:
      handle_menu_command((uint16_t)wparam);
      return true;

    case evCommand: {
      switch (HIWORD(wparam)) {
        case kMenuBarNotificationItemClick:
          handle_menu_command((uint16_t)LOWORD(wparam));
          return true;

        case RVN_SELCHANGE:
          if (g_app)
            g_app->selected_idx = (int)(int16_t)LOWORD(wparam);
          return true;

        case RVN_DBLCLK:
          handle_menu_command(ID_POST_VIEW);
          return true;

        case RVN_DELETE:
          handle_menu_command(ID_POST_DELETE);
          return true;

        default:
          return false;
      }
    }

    case evClose:
      ui_request_quit();
      return true;

    case evDestroy:
      if (g_app && g_app->main_win == win) {
        g_app->main_win = NULL;
        g_app->content_win = NULL;
        g_app->feed_win = NULL;
      }
      return false;

    default:
      return false;
  }
}

// ============================================================
// create_main_window
// ============================================================

void create_main_window(void) {
  if (!g_app) return;
  int sw = 480;
  int sh = 400;
  int x  = 40;
  int y  = MENUBAR_HEIGHT + 40;
  int w  = sw - 8;
  int h  = sh - y - 4;

  window_t *win = create_window("Social Feed",
                                WINDOW_TOOLBAR | WINDOW_STATUSBAR,
                                MAKERECT(x, y, w, h),
                                NULL, main_win_proc, g_app->hinstance, NULL);
  show_window(win, true);
}
