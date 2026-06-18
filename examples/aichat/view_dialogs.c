// VIEW: Dialogs for AI Chat.

#include "aichat.h"

// ============================================================
// New Session Dialog
// ============================================================

typedef struct {
    char title[128];
} new_session_state_t;

static lresult_t new_session_proc(window_t *win, uint32_t msg,
                                  uint32_t wparam, void *lparam) {
    new_session_state_t *state = (new_session_state_t *)win->userdata;

    switch (msg) {
        case evCreate: {
            state = (new_session_state_t *)lparam;
            win->userdata = state;
            
            // Create dialog controls
            create_window("Session Title:", WINDOW_NOTITLE,
                MAKERECT(8, 8, 80, 14), win, win_label, 0, NULL);
            
            window_t *title_edit = create_window("", WINDOW_NOTITLE,
                MAKERECT(90, 8, 200, 20), win, win_textedit, 0, NULL);
            title_edit->id = ID_NEW_SESSION_TITLE;
            
            create_window("Create", WINDOW_NOTITLE,
                MAKERECT(130, 40, 60, 20), win, win_button, BUTTON_DEFAULT, NULL)->id = ID_OK;
            
            create_window("Cancel", WINDOW_NOTITLE,
                MAKERECT(200, 40, 60, 20), win, win_button, 0, NULL)->id = ID_CANCEL;
            
            return true;
        }

        case evCommand: {
            if (HIWORD(wparam) == btnClicked) {
                window_t *source = (window_t *)lparam;
                
                if (source->id == ID_OK) {
                    // Get title from edit control
                    char title[128];
                    send_message(get_window_item(win, ID_NEW_SESSION_TITLE),
                                edGetText, sizeof(title), (lParam_t)title);
                    
                    if (title[0] != '\0') {
                        // Create session
                        app_new_session(title);
                        end_dialog(win, 1);
                    } else {
                        message_box(win, "Please enter a session title.",
                                    "Error", MB_OK);
                    }
                    return true;
                }
                
                if (source->id == ID_CANCEL) {
                    end_dialog(win, 0);
                    return true;
                }
            }
            return false;
        }

        case evClose:
            end_dialog(win, 0);
            return true;

        default:
            return default_winproc(win, msg, wparam, lparam);
    }
}

void show_new_session_dialog(window_t *parent) {
    new_session_state_t state = { .title = "" };
    
    show_dialog_from_form_ex(NULL, "New Session", parent,
                             WINDOW_DIALOG | WINDOW_NOTRAYBUTTON,
                             new_session_proc, &state);
}

// ============================================================
// About Dialog
// ============================================================

static lresult_t about_proc(window_t *win, uint32_t msg,
                            uint32_t wparam, void *lparam) {
    switch (msg) {
        case evCreate: {
            // Create dialog controls
            create_window("AI Chat v1.0", WINDOW_NOTITLE,
                MAKERECT(8, 8, 200, 14), win, win_label, 0, NULL);
            
            create_window("Chat with AI agent and save transcripts as markdown files.",
                WINDOW_NOTITLE,
                MAKERECT(8, 28, 280, 28), win, win_label, 0, NULL);
            
            create_window("OK", WINDOW_NOTITLE,
                MAKERECT(130, 60, 60, 20), win, win_button, BUTTON_DEFAULT, NULL)->id = ID_OK;
            
            return true;
        }

        case evCommand: {
            if (HIWORD(wparam) == btnClicked) {
                window_t *source = (window_t *)lparam;
                
                if (source->id == ID_OK) {
                    end_dialog(win, 1);
                    return true;
                }
            }
            return false;
        }

        case evClose:
            end_dialog(win, 0);
            return true;

        default:
            return default_winproc(win, msg, wparam, lparam);
    }
}

void show_about_dialog(window_t *parent) {
    show_dialog_from_form_ex(NULL, "About AI Chat", parent,
                             WINDOW_DIALOG | WINDOW_NOTRAYBUTTON,
                             about_proc, NULL);
}