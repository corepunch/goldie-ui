// VIEW: Main window — session list and chat area.

#include "aichat.h"

#define FEED_CELL_TEXT_MAX 256

// ============================================================
// session_list_refresh — refresh session tableview
// ============================================================

void session_list_refresh(void) {
    if (!g_app || !g_app->session_list_win) return;
    send_message(g_app->session_list_win, tvRefresh, 0, NULL);
}

// ============================================================
// message_list_refresh — refresh message tableview
// ============================================================

void message_list_refresh(void) {
    if (!g_app || !g_app->message_list_win) return;
    
    // Set filter to current session
    if (g_app->current_session_id > 0) {
        send_message(g_app->message_list_win, tvSetFilter, 0,
                     (void *)(intptr_t)g_app->current_session_id);
    }
    
    send_message(g_app->message_list_win, tvRefresh, 0, NULL);
}

// ============================================================
// main_win_proc
// ============================================================

lresult_t main_win_proc(window_t *win, uint32_t msg,
                       uint32_t wparam, void *lparam) {
    switch (msg) {
        case evCreate:
            if (!g_app) return false;
            g_app->main_win = win;

            // Get child windows by ID
            g_app->session_list_win = get_window_item(win, ID_MAIN_WINDOW_SESSION_LIST);
            g_app->message_list_win = get_window_item(win, ID_MAIN_WINDOW_MESSAGE_LIST);
            g_app->user_input_win = get_window_item(win, ID_MAIN_WINDOW_USER_INPUT);

            app_update_status();
            session_list_refresh();
            message_list_refresh();
            return true;

        case evResize:
            // Let framework handle layout
            window_layout_sync(win);
            return false;

        case evCommand: {
            uint16_t notification = HIWORD(wparam);
            window_t *source = (window_t *)lparam;

            // Handle toolbar button clicks
            if (notification == tbButtonClick) {
                handle_menu_command((uint16_t)wparam);
                return true;
            }

            // Handle menu commands
            if (notification == kMenuBarNotificationItemClick) {
                handle_menu_command((uint16_t)LOWORD(wparam));
                return true;
            }

            // Handle session list selection
            if (source && source->id == ID_MAIN_WINDOW_SESSION_LIST) {
                if (notification == RVN_SELCHANGE) {
                    int index = (int)(int16_t)LOWORD(wparam);
                    g_app->selected_session_idx = index;
                    
                    // Get session ID from index
                    result_node_t *sessions = (result_node_t *)send_db_message(g_app->db, dbFetch,
                        MAKEDWORD(ID_DB_SESSIONS, 0), (void *)(intptr_t)0);
                    
                    result_node_t *node = sessions;
                    for (int i = 0; i < index && node; i++)
                        node = node->next;
                    
                    if (node) {
                        db_session_t *session = *(db_session_t **)node->data;
                        app_select_session(session->id);
                    }
                    
                    free_result_list(sessions);
                    return true;
                }
                
                if (notification == RVN_DBLCLK) {
                    // Double-click could open session in new window
                    return true;
                }
            }

            // Handle message list selection
            if (source && source->id == ID_MAIN_WINDOW_MESSAGE_LIST) {
                if (notification == RVN_SELCHANGE) {
                    g_app->selected_message_idx = (int)(int16_t)LOWORD(wparam);
                    return true;
                }
            }

            // Handle send button
            if (source && source->id == ID_MAIN_WINDOW_SEND) {
                if (notification == btnClicked) {
                    // Get text from input
                    char buffer[8192];
                    send_message(get_window_item(win, ID_MAIN_WINDOW_USER_INPUT),
                                edGetText, sizeof(buffer), (lParam_t)buffer);
                    
                    if (buffer[0] != '\0') {
                        // Send message
                        app_send_message(buffer);
                        
                        // Clear input
                        set_window_item_text(win, ID_MAIN_WINDOW_USER_INPUT, "");
                    }
                    return true;
                }
            }

            return default_winproc(win, msg, wparam, lparam);
        }

        case evClose:
            ui_request_quit();
            return true;

        case evDestroy:
            if (g_app && g_app->main_win == win) {
                g_app->main_win = NULL;
                g_app->session_list_win = NULL;
                g_app->message_list_win = NULL;
                g_app->user_input_win = NULL;
            }
            return false;

        default:
            return default_winproc(win, msg, wparam, lparam);
    }
}

// ============================================================
// create_main_window
// ============================================================

void create_main_window(void) {
    if (!g_app) return;
    int x = 40;
    int y = MENUBAR_HEIGHT + 40;

    // NOTE: In a real build, this would use the generated form from .orion
    // For now, we create the window manually
    
    window_t *win = create_window(
        "AI Chat",
        WINDOW_TOOLBAR | WINDOW_STATUSBAR,
        MAKERECT(x, y, SCREEN_W, SCREEN_H),
        NULL,
        main_win_proc,
        g_app->hinstance,
        NULL
    );
    
    if (!win) return;
    
    // Create child controls
    // Session list panel (left side)
    create_window("Sessions", WINDOW_NOTITLE,
        MAKERECT(4, 4, 150, 14), win, win_label, 0, NULL);
    
    window_t *session_list = create_window("", WINDOW_NOTITLE,
        MAKERECT(4, 20, 150, SCREEN_H - 60), win, win_reportview, 0, NULL);
    session_list->id = ID_MAIN_WINDOW_SESSION_LIST;
    
    // Chat panel (right side)
    create_window("Chat", WINDOW_NOTITLE,
        MAKERECT(160, 4, SCREEN_W - 164, 14), win, win_label, 0, NULL);
    
    window_t *message_list = create_window("", WINDOW_NOTITLE,
        MAKERECT(160, 20, SCREEN_W - 164, SCREEN_H - 80), win, win_reportview, 0, NULL);
    message_list->id = ID_MAIN_WINDOW_MESSAGE_LIST;
    
    // Input area
    window_t *user_input = create_window("", WINDOW_NOTITLE,
        MAKERECT(160, SCREEN_H - 56, SCREEN_W - 220, 40), win, win_textedit, 0, NULL);
    user_input->id = ID_MAIN_WINDOW_USER_INPUT;
    
    create_window("Send", WINDOW_NOTITLE,
        MAKERECT(SCREEN_W - 56, SCREEN_H - 56, 52, 40), win, win_button, 0, NULL)->id = ID_MAIN_WINDOW_SEND;
    
    show_window(win, true);
}