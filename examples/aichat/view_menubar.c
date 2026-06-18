// VIEW: Menu bar and command dispatch for AI Chat.

#include "aichat.h"
#include "../../gem_magic.h"

// ============================================================
// Accelerator table
// ============================================================

static const accel_t kAccelEntries[] = {
    { FCONTROL|FVIRTKEY, AX_KEY_N,     ID_FILE_NEW_SESSION    },
    { FCONTROL|FVIRTKEY, AX_KEY_S,     ID_FILE_SAVE_TRANSCRIPT },
    { FVIRTKEY,          AX_KEY_ENTER, ID_MAIN_WINDOW_SEND    },
};

// ============================================================
// handle_menu_command — dispatch File / Session / Help
// ============================================================

void handle_menu_command(uint16_t id) {
    if (!g_app) return;
    window_t *parent = g_app->main_win ? g_app->main_win
                                       : g_app->menubar_win;
    AI_DEBUG("command id=%u", (unsigned)id);

    switch (id) {
        // ---- File ----
        case ID_FILE_NEW_SESSION:
            show_new_session_dialog(parent);
            break;

        case ID_FILE_SAVE_TRANSCRIPT: {
            // For now, use a fixed filename
            // In a real app, this would show a file save dialog
            char filepath[256];
            snprintf(filepath, sizeof(filepath), "chat_session_%d.md",
                     g_app->current_session_id);
            
            if (app_save_transcript(filepath)) {
                message_box(parent, "Transcript saved successfully!",
                            "Save Transcript", MB_OK);
            } else {
                message_box(parent, "Failed to save transcript.",
                            "Error", MB_OK);
            }
            break;
        }

        case ID_FILE_QUIT:
            ui_request_quit();
            break;

        // ---- Session ----
        case ID_SESSION_RENAME: {
            if (g_app->current_session_id <= 0) {
                message_box(parent, "Select a session first.", "Rename Session", MB_OK);
                break;
            }
            
            // Get current session
            db_session_t *session = (db_session_t *)send_db_message(g_app->db, dbFind,
                MAKEDWORD(ID_DB_SESSIONS, 0), (void *)(intptr_t)g_app->current_session_id);
            
            if (session) {
                // For now, use a simple prompt
                // In a real app, this would show a dialog
                char new_title[128];
                snprintf(new_title, sizeof(new_title), "%s (renamed)", session->title);
                app_rename_session(g_app->current_session_id, new_title);
                session_list_refresh();
            }
            break;
        }

        case ID_SESSION_DELETE: {
            if (g_app->current_session_id <= 0) {
                message_box(parent, "Select a session first.", "Delete Session", MB_OK);
                break;
            }
            
            if (message_box(parent, "Delete this session and all its messages?",
                            "Delete Session", MB_YESNO) == IDYES) {
                app_delete_session(g_app->current_session_id);
                session_list_refresh();
            }
            break;
        }

        case ID_SESSION_CLEAR: {
            if (g_app->current_session_id <= 0) {
                message_box(parent, "Select a session first.", "Clear Chat", MB_OK);
                break;
            }
            
            // Delete all messages in current session
            result_node_t *messages = (result_node_t *)send_db_message(g_app->db, dbFetch,
                MAKEDWORD(ID_DB_MESSAGES, ID_DB_MESSAGES_SESSION_ID),
                (void *)(intptr_t)g_app->current_session_id);
            
            result_node_t *node = messages;
            while (node) {
                db_message_t *msg = *(db_message_t **)node->data;
                send_db_message(g_app->db, dbDelete, ID_DB_MESSAGES,
                               (void *)(intptr_t)msg->id);
                node = node->next;
            }
            free_result_list(messages);
            
            message_list_refresh();
            app_update_status();
            break;
        }

        // ---- Help ----
        case ID_HELP_ABOUT:
            show_about_dialog(parent);
            break;

        default:
            break;
    }
}

// ============================================================
// Menu bar window procedure
// ============================================================

lresult_t app_menubar_proc(window_t *win, uint32_t msg,
                          uint32_t wparam, void *lparam) {
    switch (msg) {
        case evCommand:
            if (HIWORD(wparam) == kMenuBarNotificationItemClick ||
                HIWORD(wparam) == kAcceleratorNotification) {
                handle_menu_command((uint16_t)LOWORD(wparam));
                return true;
            }
            return win_menubar(win, msg, wparam, lparam);
        default:
            return win_menubar(win, msg, wparam, lparam);
    }
}

// ============================================================
// create_menubar — build the global menu bar
// ============================================================

void create_menubar(void) {
    // NOTE: In a real build, menu definitions would be generated from .orion
    // For now, we define them manually
    
    static const menu_item_t MENU_FILE_ITEMS[] = {
        { "New Session...", ID_FILE_NEW_SESSION, NULL, 0 },
        { "Save Transcript...", ID_FILE_SAVE_TRANSCRIPT, NULL, 0 },
        { NULL, 0, NULL, 0 }, // Separator
        { "Quit", ID_FILE_QUIT, NULL, 0 },
    };
    
    static const menu_item_t MENU_SESSION_ITEMS[] = {
        { "Rename Session...", ID_SESSION_RENAME, NULL, 0 },
        { "Delete Session", ID_SESSION_DELETE, NULL, 0 },
        { NULL, 0, NULL, 0 }, // Separator
        { "Clear Chat", ID_SESSION_CLEAR, NULL, 0 },
    };
    
    static const menu_item_t MENU_HELP_ITEMS[] = {
        { "About...", ID_HELP_ABOUT, NULL, 0 },
    };
    
    static const menu_def_t kMenus[] = {
        { "File", MENU_FILE_ITEMS, (int)(sizeof(MENU_FILE_ITEMS) / sizeof(MENU_FILE_ITEMS[0])) },
        { "Session", MENU_SESSION_ITEMS, (int)(sizeof(MENU_SESSION_ITEMS) / sizeof(MENU_SESSION_ITEMS[0])) },
        { "Help", MENU_HELP_ITEMS, (int)(sizeof(MENU_HELP_ITEMS) / sizeof(MENU_HELP_ITEMS[0])) },
    };
    
    int menu_count = sizeof(kMenus) / sizeof(kMenus[0]);
    
    g_app->menubar_win = set_app_menu(app_menubar_proc, kMenus, menu_count,
                                      handle_menu_command, g_app->hinstance);

    g_app->accel = load_accelerators(kAccelEntries,
        (int)(sizeof(kAccelEntries)/sizeof(kAccelEntries[0])));

    if (g_app->menubar_win)
        send_message(g_app->menubar_win, kMenuBarMessageSetAccelerators,
                     0, g_app->accel);
}