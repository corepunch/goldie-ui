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
// app_like_post — increment like count in database
// ============================================================

bool app_like_post(int post_id) {
  if (!g_app || !g_app->db || post_id <= 0) return false;
  
  // Fetch the post from database
  db_post_t *post = (db_post_t *)send_db_message(g_app->db, dbFind,
    MAKEDWORD(TABLE_POSTS, 0), (void *)(intptr_t)post_id);
  
  if (!post) return false;
  
  // Increment like count
  post->like_count++;
  
  // Update in database
  bool success = send_db_message(g_app->db, dbUpdate, TABLE_POSTS, post) != 0;
  
  return success;
}

// ============================================================
// app_like_comment — increment like count in database
// ============================================================

bool app_like_comment(int comment_id) {
  if (!g_app || !g_app->db || comment_id <= 0) return false;
  
  // Fetch the comment from database
  db_comment_t *comment = (db_comment_t *)send_db_message(g_app->db, dbFind,
    MAKEDWORD(TABLE_COMMENTS, 0), (void *)(intptr_t)comment_id);
  
  if (!comment) return false;
  
  // Increment like count
  comment->like_count++;
  
  // Update in database
  bool success = send_db_message(g_app->db, dbUpdate, TABLE_COMMENTS, comment) != 0;
  
  return success;
}

// ============================================================
// app_get_post — bounds-checked accessor
// ============================================================

// ============================================================
// app_get_post — fetch post from database by index
// ============================================================

post_t *app_get_post(int index) {
  if (!g_app || !g_app->db || index < 0) return NULL;
  
  // Fetch all posts from database
  result_node_t *posts = (result_node_t *)send_db_message(g_app->db, dbFetch,
    MAKEDWORD(TABLE_POSTS, 0), (void *)(intptr_t)0);
  if (!posts) return NULL;
  
  // Navigate to the requested index
  result_node_t *node = posts;
  for (int i = 0; i < index && node; i++)
    node = node->next;
  
  if (!node) {
    free_result_list(posts);
    return NULL;
  }
  
  // Convert db_post_t to post_t
  db_post_t *db_post = *(db_post_t **)node->data;
  if (!db_post) {
    free_result_list(posts);
    return NULL;
  }
  
  post_t *post = (post_t *)calloc(1, sizeof(post_t));
  if (!post) {
    free_result_list(posts);
    return NULL;
  }
  
  // Copy fields
  post->id = db_post->id;
  post->like_count = db_post->like_count;
  post->comment_count = db_post->comment_count;
  post->created_at = 0; // Not stored in database yet
  
  // Convert fixed arrays to allocated strings
  post->title = strdup(db_post->title);
  post->body = strdup(db_post->body);
  
  // Fetch author name from database
  db_author_t *author = (db_author_t *)send_db_message(g_app->db, dbFind,
    MAKEDWORD(TABLE_AUTHORS, 0), (void *)(intptr_t)db_post->author_id);
  post->author = author ? strdup(author->name) : strdup("Unknown");
  
  // Initialize comment arrays (empty for now - comments would need separate fetch)
  post->comments = NULL;
  post->comment_cap = 0;
  
  free_result_list(posts);
  return post;
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
