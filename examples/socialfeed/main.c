// Social Feed — entry point and application lifecycle.
//
// Demonstrates a social-media style feed with:
//   - Posts            : title, author, body, likes
//   - Comments         : attached to posts, can be liked
//   - Replies          : nested under comments, can be liked
//
// Architecture (MVC):
//   MODEL      : model_feed.c  — post_t / comment_t CRUD
//   CONTROLLER : controller_app.c — app_state_t, global operations
//   VIEW       : view_main.c / view_menubar.c /
//                view_dlg_post.c / view_dlg_forms.c
//
// Appwrite structure mapping:
//   post_t.id    → Appwrite document $id in the "posts" collection
//   comment_t.id → Appwrite document $id in the "comments" collection
//   (replies are comments with a parent comment_id, stored in the same
//    "comments" collection with a "parent_id" relationship field)

#include "socialfeed.h"
#include "../../gem_magic.h"

#ifndef SHAREDIR
#define SHAREDIR "."
#endif

// ============================================================
// gem_init
// ============================================================

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
  (void)argc; (void)argv;

#if SOCIALFEED_DEBUG
  {
    char log_path[1024];
    int n = snprintf(log_path, sizeof(log_path), "%s/socialfeed.log",
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

  // Create database instance
  char db_path[512];
  snprintf(db_path, sizeof(db_path), "%s/socialfeed_seed.xml", SHAREDIR);
  g_app->db = create_database("socialfeed", "db_simple_xml", db_path);
  if (!g_app->db) {
    SF_DEBUG("Failed to create database");
    app_shutdown(g_app);
    g_app = NULL;
    return false;
  }

  // Register database with framework (NeXTSTEP-style singleton)
  ui_set_database(g_app->db);
  
  // Register database in the new registry (for declarative forms)
  // Forms with field="db.table.field" will look up "db" automatically
  register_database("db", g_app->db);

  // Database automatically loads data from source XML file
  // (no manual seed loading needed)

  create_menubar();
  create_main_window();

  SF_DEBUG("gem_init complete: database loaded");
  return true;
}

// ============================================================
// gem_shutdown
// ============================================================

void gem_shutdown(void) {
  if (!g_app) return;
  SF_DEBUG("gem_shutdown");
  if (g_app->db) {
    destroy_database(g_app->db);
    g_app->db = NULL;
  }
  app_shutdown(g_app);
  g_app = NULL;
#if SOCIALFEED_DEBUG
  axSetLogFile(NULL);
#endif
}

GEM_DEFINE("Social Feed", "1.0", gem_init, gem_shutdown, NULL)

GEM_STANDALONE_MAIN("Orion Social Feed", UI_INIT_DESKTOP, SCREEN_W, SCREEN_H,
                    g_app->menubar_win, g_app->accel)
