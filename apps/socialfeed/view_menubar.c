// VIEW: Menu bar and command dispatch for Social Feed.
//
// Menu arrays and main-form toolbar metadata are generated from socialfeed.orion.

#include "socialfeed.h"
#include <orion/gem.h>

// ============================================================
// Accelerator table
// ============================================================

// ============================================================
// handle_menu_command — dispatch File / Post / View / Help
// ============================================================

void handle_menu_command(uint16_t id) {
  if (!g_app) return;
  window_t *parent = g_app->main_win ? g_app->main_win
                                     : g_app->menubar_win;
  SF_DEBUG("command id=%u", (unsigned)id);

  switch (id) {
    // ---- File ----
    case ID_FILE_QUIT:
      ui_request_quit();
      break;

    // ---- Post ----
    case ID_POST_NEW:
      if (show_db_dialog(&socialfeed_new_post_form, "New Post", parent, 0)) {
        feed_refresh();
        app_update_status();
        SF_DEBUG("action new_post");
      }
      break;

    case ID_POST_LIKE: {
      int post_id = app_get_post_id_from_index(g_app->selected_idx);
      if (!post_id) {
        message_box(parent, "Select a post to like.", "Like Post", MB_OK);
        break;
      }
      
      if (app_like_post(post_id)) {
        feed_refresh();
        SF_DEBUG("liked post id=%d (persisted to DB)", post_id);
      }
      break;
    }

    case ID_POST_VIEW: {
      int idx = g_app->selected_idx;
      if (!app_get_post_id_from_index(idx)) {
        message_box(parent, "Select a post to view.", "View Post", MB_OK);
        break;
      }
      show_post_detail(parent, idx);
      feed_refresh();
      SF_DEBUG("action view_post idx=%d", idx);
      break;
    }

    case ID_POST_DELETE: {
      int idx = g_app->selected_idx;
      if (!app_get_post_id_from_index(idx)) {
        message_box(parent, "Select a post to delete.", "Delete Post", MB_OK);
        break;
      }
      if (message_box(parent, "Delete selected post?", "Delete Post",
                      MB_YESNO) == IDYES) {
        app_delete_post(idx);
        feed_refresh();
        app_update_status();
        SF_DEBUG("action delete_post idx=%d", idx);
      }
      break;
    }

    // ---- View ----
    case ID_VIEW_REFRESH:
      feed_refresh();
      break;

    // ---- Test ----
    case ID_TEST_EDIT_AUTHOR:
      SF_DEBUG("Testing edit author dialog with DB integration");
      test_author_edit_dialog(parent, g_app->db);
      feed_refresh(); // Refresh to show changes
      break;

    case ID_TEST_NEW_AUTHOR:
      SF_DEBUG("Testing new author dialog with DB integration");
      test_new_author_dialog(parent, g_app->db);
      feed_refresh(); // Refresh to show changes
      break;

    // ---- Help ----
    case ID_HELP_ABOUT:
      message_box(parent,
                  "Social Feed v1.0\n\n"
                  "A demonstration of posts, comments,\n"
                  "replies, and likes in Orion UI.",
                  "About Social Feed", MB_OK);
      break;

    default:
      break;
  }
}

// ============================================================
// Menu bar window procedure
// ============================================================

result_t app_menubar_proc(window_t *win, uint32_t msg,
                          uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCommand:
      if (HIWORD(wparam) == kMenuBarNotificationItemClick ||
          HIWORD(wparam) == kAcceleratorNotification) {
        handle_menu_command((uint16_t)LOWORD(wparam));
        return true;
      }
      return false;
    default:
      return win_menubar(win, msg, wparam, lparam);
  }
}

// ============================================================
// create_menubar — build the global menu bar
// ============================================================

void create_menubar(void) {
  g_app->menubar_win = set_app_menu(app_menubar_proc, kMenus, kNumMenus,
                                    handle_menu_command, g_app->hinstance);

  g_app->accel = load_accelerators(socialfeed_default_accels,
      socialfeed_default_accel_count);

  if (g_app->menubar_win)
    send_message(g_app->menubar_win, kMenuBarMessageSetAccelerators,
                 0, g_app->accel);
}
