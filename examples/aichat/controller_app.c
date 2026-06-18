// CONTROLLER: Application state and business logic.

#include "aichat.h"
#include <time.h>

// ============================================================
// Global app-state pointer
// ============================================================

app_state_t *g_app = NULL;

// ============================================================
// app_init
// ============================================================

app_state_t *app_init(void) {
    app_state_t *app = (app_state_t *)calloc(1, sizeof(app_state_t));
    if (!app) return NULL;

    app->selected_session_idx = -1;
    app->selected_message_idx = -1;
    app->current_session_id = 0;
    return app;
}

// ============================================================
// app_shutdown
// ============================================================

void app_shutdown(app_state_t *app) {
    if (!app) return;
    if (app->accel)
        free_accelerators(app->accel);
    free(app);
}

// ============================================================
// app_new_session — create a new chat session
// ============================================================

bool app_new_session(const char *title) {
    if (!g_app || !g_app->db || !title || !*title) return false;
    
    // Create timestamp
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);
    
    // Create session record
    db_session_t session = { .id = 0 };
    strncpy(session.title, title, sizeof(session.title) - 1);
    session.title[sizeof(session.title) - 1] = '\0';
    strncpy(session.created_at, timestamp, sizeof(session.created_at) - 1);
    session.created_at[sizeof(session.created_at) - 1] = '\0';
    strncpy(session.updated_at, timestamp, sizeof(session.updated_at) - 1);
    session.updated_at[sizeof(session.updated_at) - 1] = '\0';
    
    // Insert into database
    db_session_t *inserted = (db_session_t *)send_db_message(
        g_app->db, dbInsert, ID_DB_SESSIONS, &session);
    
    if (!inserted) return false;
    
    // Add welcome message
    db_message_t welcome_msg = { .id = 0 };
    welcome_msg.session_id = inserted->id;
    strncpy(welcome_msg.role, "assistant", sizeof(welcome_msg.role) - 1);
    welcome_msg.role[sizeof(welcome_msg.role) - 1] = '\0';
    strncpy(welcome_msg.content, "Hello! I'm your AI assistant. How can I help you today?",
            sizeof(welcome_msg.content) - 1);
    welcome_msg.content[sizeof(welcome_msg.content) - 1] = '\0';
    strncpy(welcome_msg.timestamp, timestamp, sizeof(welcome_msg.timestamp) - 1);
    welcome_msg.timestamp[sizeof(welcome_msg.timestamp) - 1] = '\0';
    
    send_db_message(g_app->db, dbInsert, ID_DB_MESSAGES, &welcome_msg);
    
    AI_DEBUG("created session: id=%d title='%s'", inserted->id, title);
    
    // Select the new session
    app_select_session(inserted->id);
    
    return true;
}

// ============================================================
// app_delete_session — remove session and its messages
// ============================================================

bool app_delete_session(int session_id) {
    if (!g_app || !g_app->db || session_id <= 0) return false;
    
    bool success = send_db_message(g_app->db, dbDelete, ID_DB_SESSIONS,
                                   (void *)(intptr_t)session_id) != 0;
    
    if (success) {
        // If we deleted the current session, select another one
        if (g_app->current_session_id == session_id) {
            g_app->current_session_id = 0;
            g_app->selected_session_idx = -1;
            message_list_refresh();
        }
    }
    
    return success;
}

// ============================================================
// app_rename_session — update session title
// ============================================================

bool app_rename_session(int session_id, const char *new_title) {
    if (!g_app || !g_app->db || session_id <= 0 || !new_title || !*new_title) return false;
    
    db_session_t *session = (db_session_t *)send_db_message(g_app->db, dbFind,
        MAKEDWORD(ID_DB_SESSIONS, 0), (void *)(intptr_t)session_id);
    
    if (!session) return false;
    
    strncpy(session->title, new_title, sizeof(session->title) - 1);
    session->title[sizeof(session->title) - 1] = '\0';
    
    // Update timestamp
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(session->updated_at, sizeof(session->updated_at), "%Y-%m-%d %H:%M:%S", tm);
    
    bool success = send_db_message(g_app->db, dbUpdate, ID_DB_SESSIONS, session) != 0;
    
    return success;
}

// ============================================================
// app_select_session — set current active session
// ============================================================

void app_select_session(int session_id) {
    if (!g_app) return;
    
    g_app->current_session_id = session_id;
    message_list_refresh();
    app_update_status();
}

// ============================================================
// app_send_message — send user message and get AI response
// ============================================================

bool app_send_message(const char *content) {
    if (!g_app || !g_app->db || !content || !*content) return false;
    if (g_app->current_session_id <= 0) return false;
    
    // Create timestamp
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);
    
    // Insert user message
    db_message_t user_msg = { .id = 0 };
    user_msg.session_id = g_app->current_session_id;
    strncpy(user_msg.role, "user", sizeof(user_msg.role) - 1);
    user_msg.role[sizeof(user_msg.role) - 1] = '\0';
    strncpy(user_msg.content, content, sizeof(user_msg.content) - 1);
    user_msg.content[sizeof(user_msg.content) - 1] = '\0';
    strncpy(user_msg.timestamp, timestamp, sizeof(user_msg.timestamp) - 1);
    user_msg.timestamp[sizeof(user_msg.timestamp) - 1] = '\0';
    
    db_message_t *inserted_user = (db_message_t *)send_db_message(
        g_app->db, dbInsert, ID_DB_MESSAGES, &user_msg);
    
    if (!inserted_user) return false;
    
    // Get AI response (simulated for now)
    char ai_response[8192];
    if (ai_send_message(content, ai_response, sizeof(ai_response))) {
        // Insert AI response
        db_message_t ai_msg = { .id = 0 };
        ai_msg.session_id = g_app->current_session_id;
        strncpy(ai_msg.role, "assistant", sizeof(ai_msg.role) - 1);
        ai_msg.role[sizeof(ai_msg.role) - 1] = '\0';
        strncpy(ai_msg.content, ai_response, sizeof(ai_msg.content) - 1);
        ai_msg.content[sizeof(ai_msg.content) - 1] = '\0';
        strncpy(ai_msg.timestamp, timestamp, sizeof(ai_msg.timestamp) - 1);
        ai_msg.timestamp[sizeof(ai_msg.timestamp) - 1] = '\0';
        
        send_db_message(g_app->db, dbInsert, ID_DB_MESSAGES, &ai_msg);
    }
    
    // Refresh message list
    message_list_refresh();
    
    AI_DEBUG("sent message: session=%d", g_app->current_session_id);
    return true;
}

// ============================================================
// app_save_transcript — save chat as markdown file
// ============================================================

bool app_save_transcript(const char *filepath) {
    if (!g_app || !g_app->db || !filepath || !*filepath) return false;
    if (g_app->current_session_id <= 0) return false;
    
    // Get session info
    db_session_t *session = (db_session_t *)send_db_message(g_app->db, dbFind,
        MAKEDWORD(ID_DB_SESSIONS, 0), (void *)(intptr_t)g_app->current_session_id);
    
    if (!session) return false;
    
    // Get all messages for this session
    result_node_t *messages = (result_node_t *)send_db_message(g_app->db, dbFetch,
        MAKEDWORD(ID_DB_MESSAGES, ID_DB_MESSAGES_SESSION_ID),
        (void *)(intptr_t)g_app->current_session_id);
    
    if (!messages) return false;
    
    // Open file for writing
    FILE *f = fopen(filepath, "w");
    if (!f) {
        free_result_list(messages);
        return false;
    }
    
    // Write markdown header
    fprintf(f, "# %s\n\n", session->title);
    fprintf(f, "**Created:** %s  \n", session->created_at);
    fprintf(f, "**Updated:** %s  \n\n", session->updated_at);
    fprintf(f, "---\n\n");
    
    // Write messages
    result_node_t *node = messages;
    while (node) {
        db_message_t *msg = *(db_message_t **)node->data;
        
        // Format role with proper capitalization
        const char *role = "Unknown";
        if (strcmp(msg->role, "user") == 0) {
            role = "User";
        } else if (strcmp(msg->role, "assistant") == 0) {
            role = "Assistant";
        }
        
        fprintf(f, "### %s\n", role);
        fprintf(f, "*%s*\n\n", msg->timestamp);
        fprintf(f, "%s\n\n", msg->content);
        
        node = node->next;
    }
    
    fclose(f);
    free_result_list(messages);
    
    AI_DEBUG("saved transcript: session=%d path='%s'", g_app->current_session_id, filepath);
    return true;
}

// ============================================================
// app_update_status — refresh main window status bar
// ============================================================

void app_update_status(void) {
    if (!g_app || !g_app->main_win || !g_app->db) return;
    
    // Get session count
    result_node_t *sessions = (result_node_t *)send_db_message(g_app->db, dbFetch,
        MAKEDWORD(ID_DB_SESSIONS, 0), (void *)(intptr_t)0);
    int session_count = count_result_list(sessions);
    free_result_list(sessions);
    
    // Get message count for current session
    int message_count = 0;
    if (g_app->current_session_id > 0) {
        result_node_t *messages = (result_node_t *)send_db_message(g_app->db, dbFetch,
            MAKEDWORD(ID_DB_MESSAGES, ID_DB_MESSAGES_SESSION_ID),
            (void *)(intptr_t)g_app->current_session_id);
        message_count = count_result_list(messages);
        free_result_list(messages);
    }
    
    char buf[128];
    snprintf(buf, sizeof(buf), "Session %d of %d | %d messages",
             g_app->current_session_id > 0 ? g_app->current_session_id : 0,
             session_count, message_count);
    send_message(g_app->main_win, evStatusBar, 0, buf);
}