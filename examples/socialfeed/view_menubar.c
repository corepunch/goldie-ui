// VIEW: Menu bar and command dispatch for Social Feed.
//
// Menu arrays (MENU_FILE_ITEMS, MENU_POST_ITEMS, …), kMenus/kNumMenus, and the
// toolbar array TB_FEED are generated from socialfeed.orion and declared in the
// generated forms header included via socialfeed.h.

#include "socialfeed.h"
#include "../../gem_magic.h"

// ============================================================
// Accelerator table
// ============================================================

static const accel_t kAccelEntries[] = {
  { FCONTROL|FVIRTKEY, AX_KEY_N,     ID_POST_NEW    },
  { FCONTROL|FVIRTKEY, AX_KEY_L,     ID_POST_LIKE   },
  { FVIRTKEY,          AX_KEY_ENTER, ID_POST_VIEW   },
  { FVIRTKEY,          AX_KEY_DEL,   ID_POST_DELETE },
};

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
      if (show_new_post_dialog(parent)) {
        feed_refresh();
        app_update_status();
        SF_DEBUG("action new_post");
      }
      break;

    case ID_POST_LIKE: {
      post_t *p = app_get_post(g_app->selected_idx);
      if (!p) {
        message_box(parent, "Select a post to like.", "Like Post", MB_OK);
        break;
      }
      int post_id = p->id;
      free(p->title);
      free(p->body);
      free(p->author);
      free(p);
      
      if (app_like_post(post_id)) {
        feed_refresh();
        SF_DEBUG("liked post id=%d (persisted to DB)", post_id);
      }
      break;
    }

    case ID_POST_VIEW: {
      int idx = g_app->selected_idx;
      if (!app_get_post(idx)) {
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
      if (!app_get_post(idx)) {
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

  g_app->accel = load_accelerators(kAccelEntries,
      (int)(sizeof(kAccelEntries)/sizeof(kAccelEntries[0])));

  if (g_app->menubar_win)
    send_message(g_app->menubar_win, kMenuBarMessageSetAccelerators,
                 0, g_app->accel);
}
