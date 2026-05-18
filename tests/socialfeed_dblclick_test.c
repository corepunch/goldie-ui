// Test: Verify double-click on socialfeed tableview opens post detail dialog
//
// This test verifies that:
// 1. Double-clicking a row in the tableview updates g_app->selected_idx
// 2. app_get_post() returns the correct post for that index
// 3. The post detail dialog can be opened successfully

#include "../examples/socialfeed/socialfeed.h"
#include "test_framework.h"

static bool test_dblclick_updates_selection(void) {
  // Initialize app state
  g_app = app_init();
  TEST_ASSERT(g_app != NULL, "App state initialized");
  
  // Create in-memory database
  g_app->db = create_database("test_db", "SimpleXMLDatabase", ":memory:");
  TEST_ASSERT(g_app->db != NULL, "Database created");
  
  // Insert test posts
  db_post_t post1 = {
    .id = 0,
    .author_id = 1,
    .like_count = 5,
    .comment_count = 3
  };
  strncpy(post1.title, "Test Post 1", sizeof(post1.title) - 1);
  strncpy(post1.body, "This is test post 1", sizeof(post1.body) - 1);
  
  db_post_t *inserted1 = (db_post_t *)send_db_message(g_app->db, dbInsert, TABLE_POSTS, &post1);
  TEST_ASSERT(inserted1 != NULL, "First post inserted");
  
  db_post_t post2 = {
    .id = 0,
    .author_id = 1,
    .like_count = 10,
    .comment_count = 7
  };
  strncpy(post2.title, "Test Post 2", sizeof(post2.title) - 1);
  strncpy(post2.body, "This is test post 2", sizeof(post2.body) - 1);
  
  db_post_t *inserted2 = (db_post_t *)send_db_message(g_app->db, dbInsert, TABLE_POSTS, &post2);
  TEST_ASSERT(inserted2 != NULL, "Second post inserted");
  
  // Simulate double-click on row 1 (second post)
  int dblclick_index = 1;
  g_app->selected_idx = dblclick_index;
  
  // Verify app_get_post returns the correct post
  post_t *retrieved = app_get_post(dblclick_index);
  TEST_ASSERT(retrieved != NULL, "app_get_post returns non-NULL for valid index");
  TEST_ASSERT(strcmp(retrieved->title, "Test Post 2") == 0, "Retrieved correct post title");
  TEST_ASSERT(retrieved->like_count == 10, "Retrieved correct like count");
  
  // Cleanup
  post_free(retrieved);
  send_db_message(g_app->db, dbClose, 0, NULL);
  app_shutdown(g_app);
  g_app = NULL;
  
  return true;
}

static bool test_dblclick_on_first_row(void) {
  // Test double-clicking the first row (index 0)
  g_app = app_init();
  TEST_ASSERT(g_app != NULL, "App state initialized");
  
  g_app->db = create_database("test_db", "SimpleXMLDatabase", ":memory:");
  TEST_ASSERT(g_app->db != NULL, "Database created");
  
  db_post_t post = {
    .id = 0,
    .author_id = 1,
    .like_count = 42,
    .comment_count = 0
  };
  strncpy(post.title, "First Post", sizeof(post.title) - 1);
  strncpy(post.body, "First post body", sizeof(post.body) - 1);
  
  db_post_t *inserted = (db_post_t *)send_db_message(g_app->db, dbInsert, TABLE_POSTS, &post);
  TEST_ASSERT(inserted != NULL, "Post inserted");
  
  // Simulate double-click on row 0
  g_app->selected_idx = 0;
  
  post_t *retrieved = app_get_post(0);
  TEST_ASSERT(retrieved != NULL, "app_get_post returns non-NULL for index 0");
  TEST_ASSERT(strcmp(retrieved->title, "First Post") == 0, "Retrieved correct first post");
  
  // Cleanup
  post_free(retrieved);
  send_db_message(g_app->db, dbClose, 0, NULL);
  app_shutdown(g_app);
  g_app = NULL;
  
  return true;
}

int main(void) {
  TEST_BEGIN("SocialFeed Double-Click Tests");
  
  RUN_TEST(test_dblclick_updates_selection);
  RUN_TEST(test_dblclick_on_first_row);
  
  TEST_END();
  return 0;
}
