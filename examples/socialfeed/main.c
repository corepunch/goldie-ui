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

  g_app = app_init();
  if (!g_app) return false;
  g_app->hinstance = hinstance;

  if (!socialfeed_load_seed_data(SHAREDIR "/socialfeed_seed.xml")) {
    app_shutdown(g_app);
    g_app = NULL;
    return false;
  }

  create_menubar();
  create_main_window();

  SF_DEBUG("gem_init complete: %d posts seeded (next_comment_id=%d)",
           g_app->post_count, g_app->next_comment_id);
  return true;
}

// ============================================================
// gem_shutdown
// ============================================================

void gem_shutdown(void) {
  if (!g_app) return;
  SF_DEBUG("gem_shutdown");
  app_shutdown(g_app);
  g_app = NULL;
#if SOCIALFEED_DEBUG
  axSetLogFile(NULL);
#endif
}

GEM_DEFINE("Social Feed", "1.0", gem_init, gem_shutdown, NULL)

GEM_STANDALONE_MAIN("Orion Social Feed", UI_INIT_DESKTOP, SCREEN_W, SCREEN_H,
                    g_app->menubar_win, g_app->accel)
