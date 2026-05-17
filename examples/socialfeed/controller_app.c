// CONTROLLER: Application state and business logic.

#include "socialfeed.h"

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

  app->selected_idx = -1;
  return app;
}

// ============================================================
// app_shutdown
// ============================================================

void app_shutdown(app_state_t *app) {
  if (!app) return;
  // Database owns post storage - it cleans up on destroy
  if (app->accel)
    free_accelerators(app->accel);
  free(app);
}

// ============================================================
// app_add_post — append a post, grow the array if needed
// ============================================================

bool app_add_post(post_t *post) {
  if (!g_app || !g_app->db || !post) return false;
  
  // Convert application post_t to database db_post_t
  db_post_t db_post = {
    .id = 0,  // Database auto-increments
    .author_id = 1,  // TODO: lookup author by name
    .like_count = post->like_count,
    .comment_count = post->comment_count
  };
  strncpy(db_post.title, post->title ? post->title : "", sizeof(db_post.title) - 1);
  strncpy(db_post.body, post->body ? post->body : "", sizeof(db_post.body) - 1);
  
  // Insert into database
  db_post_t *inserted = (db_post_t *)send_db_message(g_app->db, dbInsert, TABLE_POSTS, &db_post);
  if (!inserted) return false;
  
  // Update application model with assigned ID
  post->id = inserted->id;
  return true;
}

// ============================================================
// app_delete_post — remove post at index and free it
// ============================================================

bool app_delete_post(int index) {
  if (!g_app || !g_app->db || index < 0) return false;
  
  // Fetch all posts to get the post at index
  result_node_t *posts = (result_node_t *)send_db_message(g_app->db, dbFetch,
    MAKEDWORD(TABLE_POSTS, 0), (void *)(intptr_t)0);
  if (!posts) return false;
  
  // Navigate to the post at index
  result_node_t *node = posts;
  for (int i = 0; i < index && node; i++)
    node = node->next;
  
  if (!node) {
    free_result_list(posts);
    return false;
  }
  
  db_post_t *post = *(db_post_t **)node->data;
  int post_id = post->id;
  free_result_list(posts);
  
  // Delete from database
  bool success = send_db_message(g_app->db, dbDelete, TABLE_POSTS,
                                 (void *)(intptr_t)post_id) != 0;
  
  if (success) {
    // Update selected index
    int post_count = count_result_list(
      (result_node_t *)send_db_message(g_app->db, dbFetch,
        MAKEDWORD(TABLE_POSTS, 0), (void *)(intptr_t)0));
    if (g_app->selected_idx >= post_count)
      g_app->selected_idx = post_count - 1;
  }
  
  return success;
}

// ============================================================
// app_get_post — bounds-checked accessor
// ============================================================

// ============================================================
// app_get_post — fetch post from database by index
// ============================================================
//
// NOTE: This temporarily returns NULL because of type mismatch.
// The database has db_post_t (flat records with fixed char arrays)
// but the view code expects post_t (with char* and nested comments).
//
// TODO: Either:
//   1. Convert db_post_t -> post_t when fetching (build rich model on-demand)
//   2. Refactor views to work with db_post_t directly (simpler, flatter data)
//   3. Keep posts cached in memory and just use database for persistence
//
// For now, view_dlg_post.c and other code using app_get_post will break.
//
post_t *app_get_post(int index) {
  if (!g_app || !g_app->db || index < 0) return NULL;
  
  // Fetch all posts and navigate to index
  result_node_t *posts = (result_node_t *)send_db_message(g_app->db, dbFetch,
    MAKEDWORD(TABLE_POSTS, 0), (void *)(intptr_t)0);
  if (!posts) return NULL;
  
  result_node_t *node = posts;
  for (int i = 0; i < index && node; i++)
    node = node->next;
  
  if (!node) {
    free_result_list(posts);
    return NULL;
  }
  
  // Type mismatch: database has db_post_t, caller expects post_t
  db_post_t *db_post = *(db_post_t **)node->data;
  (void)db_post;
  free_result_list(posts);
  
  return NULL;  // Temporarily disabled - needs type conversion
}

// ============================================================
// app_update_status — refresh main window status bar
// ============================================================

void app_update_status(void) {
  if (!g_app || !g_app->main_win || !g_app->db) return;
  
  // Fetch post count from database
  result_node_t *posts = (result_node_t *)send_db_message(g_app->db, dbFetch,
    MAKEDWORD(TABLE_POSTS, 0), (void *)(intptr_t)0);
  int post_count = count_result_list(posts);
  free_result_list(posts);
  
  char buf[64];
  snprintf(buf, sizeof(buf), "%d post%s",
           post_count, post_count == 1 ? "" : "s");
  send_message(g_app->main_win, evStatusBar, 0, buf);
}

// ============================================================
// app_add_comment — assign an ID then add to the post
// ============================================================

bool app_add_comment(post_t *post, comment_t *c) {
  if (!g_app || !g_app->db || !post || !c) return false;
  
  // Database auto-assigns comment ID during insert
  // For now, keep using application-level comment management
  // TODO: Store comments in database and use dbInsert(TABLE_COMMENTS)
  return post_add_comment(post, c);
}

// ============================================================
// app_add_reply — assign an ID then add to the parent comment
// ============================================================

bool app_add_reply(comment_t *parent, comment_t *reply) {
  if (!g_app || !g_app->db || !parent || !reply) return false;
  
  // Database auto-assigns comment ID during insert
  // For now, keep using application-level comment management
  // TODO: Store comments in database and use dbInsert(TABLE_COMMENTS)
  return comment_add_reply(parent, reply);
}
