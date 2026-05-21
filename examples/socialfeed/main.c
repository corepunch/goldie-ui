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
#include "../../commctl/commctl.h"
#include "../../platform/platform.h"

#ifndef SHAREDIR
#define SHAREDIR "."
#endif

#define SOCIALFEED_PATH_MAX 1024

static bool resolve_socialfeed_db_path(char *out, size_t out_sz) {
  if (!out || out_sz == 0) return false;
  out[0] = '\0';

  const char *exe_dir = ui_get_exe_dir();
  if (!exe_dir || !*exe_dir) return false;

  char candidate[SOCIALFEED_PATH_MAX];

  snprintf(candidate, sizeof(candidate), "%s/%s/socialfeed_seed.xml",
           exe_dir, SHAREDIR);
  if (axPathExists(candidate)) {
    snprintf(out, out_sz, "%s", candidate);
    return true;
  }

  snprintf(candidate, sizeof(candidate), "%s/../share/orion/socialfeed_seed.xml",
           exe_dir);
  if (axPathExists(candidate)) {
    snprintf(out, out_sz, "%s", candidate);
    return true;
  }

  snprintf(candidate, sizeof(candidate),
           "%s/../../examples/socialfeed/socialfeed_seed.xml", exe_dir);
  if (axPathExists(candidate)) {
    snprintf(out, out_sz, "%s", candidate);
    return true;
  }

  snprintf(out, out_sz, "%s/%s/socialfeed_seed.xml", exe_dir, SHAREDIR);
  return false;
}

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

  // Form-based windows/dialogs require commctl classes to be registered.
  register_commctl_classes();

  char db_path[SOCIALFEED_PATH_MAX];
  if (!resolve_socialfeed_db_path(db_path, sizeof(db_path))) {
    SF_DEBUG("socialfeed_seed.xml not found in known locations; using fallback path: %s",
             db_path);
  }
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
  {
    result_node_t *posts = (result_node_t *)send_db_message(g_app->db, dbFetch,
      MAKEDWORD(TABLE_POSTS, 0), (void *)(intptr_t)0);
    result_node_t *authors = (result_node_t *)send_db_message(g_app->db, dbFetch,
      MAKEDWORD(TABLE_AUTHORS, 0), (void *)(intptr_t)0);
    result_node_t *comments = (result_node_t *)send_db_message(g_app->db, dbFetch,
      MAKEDWORD(TABLE_COMMENTS, 0), (void *)(intptr_t)0);
    SF_DEBUG("database loaded: path='%s' authors=%d posts=%d comments=%d",
             db_path, count_result_list(authors), count_result_list(posts),
             count_result_list(comments));
    free_result_list(posts);
    free_result_list(authors);
    free_result_list(comments);
  }

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
