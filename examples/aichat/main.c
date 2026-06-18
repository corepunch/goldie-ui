// AI Chat — entry point and application lifecycle.
//
// Demonstrates a chat application with:
//   - Sessions        : multiple chat sessions
//   - Messages        : user and assistant messages
//   - AI Integration  : chat with AI agent
//   - Export          : save transcripts as markdown files
//
// Architecture (MVC):
//   MODEL      : db_simple_xml.c — session/message CRUD
//   CONTROLLER : controller_app.c — app_state_t, global operations
//   VIEW       : view_main.c / view_menubar.c / view_dialogs.c

#include "aichat.h"
#include "../../gem_magic.h"
#include "../../commctl/commctl.h"
#include "../../platform/platform.h"

#ifndef SHAREDIR
#define SHAREDIR "."
#endif

#define AICHAT_PATH_MAX 1024

static bool resolve_aichat_db_path(char *out, size_t out_sz) {
    if (!out || out_sz == 0) return false;
    out[0] = '\0';

    char candidate[AICHAT_PATH_MAX];
    snprintf(candidate, sizeof(candidate), "%s/aichat_seed.xml", SHAREDIR);
    if (axPathExists(candidate)) {
        snprintf(out, out_sz, "%s", candidate);
        return true;
    }

    const char *exe_dir = ui_get_exe_dir();
    if (!exe_dir || !*exe_dir) return false;

    snprintf(candidate, sizeof(candidate), "%s/%s/aichat_seed.xml", exe_dir, SHAREDIR);
    if (axPathExists(candidate)) {
        snprintf(out, out_sz, "%s", candidate);
        return true;
    }

    snprintf(candidate, sizeof(candidate),
             "%s/../share/aichat/aichat_seed.xml", exe_dir);
    if (axPathExists(candidate)) {
        snprintf(out, out_sz, "%s", candidate);
        return true;
    }

    snprintf(candidate, sizeof(candidate),
             "%s/../../examples/aichat/share/aichat_seed.xml", exe_dir);
    if (axPathExists(candidate)) {
        snprintf(out, out_sz, "%s", candidate);
        return true;
    }

    snprintf(out, out_sz, "%s/../../examples/aichat/share/aichat_seed.xml",
             exe_dir);
    return false;
}

// ============================================================
// gem_init
// ============================================================

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
    (void)argc; (void)argv;

#if AICHAT_DEBUG
    {
        char log_path[1024];
        int n = snprintf(log_path, sizeof(log_path), "%s/aichat.log",
                         axSettingsDirectory());
        if (n > 0 && (size_t)n < sizeof(log_path))
            axSetLogFile(log_path);
    }
#endif

    // Register database class
    DB_CLASS(db_simple_xml);

    g_app = app_init();
    if (!g_app) return false;
    g_app->hinstance = hinstance;

    // Form-based windows/dialogs require commctl classes to be registered.
    register_commctl_classes();

    char db_path[AICHAT_PATH_MAX];
    if (!resolve_aichat_db_path(db_path, sizeof(db_path))) {
        AI_DEBUG("aichat_seed.xml not found in known locations; using fallback path: %s",
                 db_path);
    }
    g_app->db = create_database("aichat", "db_simple_xml", db_path);
    if (!g_app->db) {
        AI_DEBUG("Failed to create database");
        app_shutdown(g_app);
        g_app = NULL;
        return false;
    }

    // Register database with framework
    ui_set_database(g_app->db);
    
    // Register database in the registry (for declarative forms)
    register_database("db", g_app->db);

    AI_DEBUG("gem_init complete: database loaded");
    return true;
}

// ============================================================
// gem_shutdown
// ============================================================

void gem_shutdown(void) {
    if (!g_app) return;
    AI_DEBUG("gem_shutdown");
    if (g_app->db) {
        destroy_database(g_app->db);
        g_app->db = NULL;
    }
    app_shutdown(g_app);
    g_app = NULL;
#if AICHAT_DEBUG
    axSetLogFile(NULL);
#endif
}

GEM_DEFINE("AI Chat", "1.0", gem_init, gem_shutdown, NULL)

GEM_STANDALONE_MAIN("Orion AI Chat", UI_INIT_DESKTOP, SCREEN_W, SCREEN_H,
                    g_app->menubar_win, g_app->accel)