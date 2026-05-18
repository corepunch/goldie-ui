// VIEW: Main window — feed list of posts (win_reportview).
//
// TB_FEED and TB_FEED_COUNT are generated from socialfeed.orion and
// declared in the generated forms header included via socialfeed.h.

#include "socialfeed.h"

#define FEED_CELL_TEXT_MAX 256

// ============================================================
// feed_refresh — refresh tableview (automatic population!)
// ============================================================

void feed_refresh(void) {
  if (!g_app || !g_app->feed_win) return;
  
  // tableview handles everything automatically
  send_message(g_app->feed_win, tvRefresh, 0, NULL);
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

      // The form creates stack -> tableview hierarchy automatically.
      // Database is auto-propagated to tableview during form creation.
      // Look up child windows by their IDs (generated from .orion).
      g_app->content_win = get_window_item(win, ID_MAIN_WINDOW_CONTENT);
      g_app->feed_win = get_window_item(win, ID_MAIN_WINDOW_FEED);

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
  int x  = 40;
  int y  = MENUBAR_HEIGHT + 40;

  // Use the form defined in socialfeed.orion - it already contains
  // the stack and tableview with all bindings configured.
  // Database is automatically registered with framework at startup.
  window_t *win = create_window_from_form(&socialfeed_main_window_form,
                                          x, y,
                                          NULL, main_win_proc,
                                          g_app->hinstance, NULL);
  show_window(win, true);
}
