#include "examples/socialfeed/socialfeed.h"
#include "test_framework.h"

static void setup_feed(void) {
  DB_CLASS(db_simple_xml);
  g_app = app_init();
  ASSERT_NOT_NULL(g_app);
  g_app->db = create_database("test_db", "db_simple_xml", ":memory:");
  ASSERT_NOT_NULL(g_app->db);
}

static void teardown_feed(void) {
  if (g_app) {
    if (g_app->db)
      destroy_database(g_app->db);
    g_app->db = NULL;
    app_shutdown(g_app);
    g_app = NULL;
  }
}

static db_post_t *insert_post(const char *title, int likes, int comments) {
  db_post_t post = {
    .id = 0,
    .author_id = 1,
    .like_count = likes,
    .comment_count = comments
  };
  snprintf(post.title, sizeof(post.title), "%s", title);
  snprintf(post.body, sizeof(post.body), "%s body", title);
  return (db_post_t *)send_db_message(g_app->db, dbInsert, TABLE_POSTS, &post);
}

static void test_row_index_maps_to_post_id(void) {
  TEST("socialfeed: row index maps to inserted post id");
  setup_feed();

  db_post_t *first = insert_post("First Post", 5, 3);
  db_post_t *second = insert_post("Second Post", 10, 7);
  ASSERT_NOT_NULL(first);
  ASSERT_NOT_NULL(second);

  g_app->selected_idx = 1;
  int row0 = app_get_post_id_from_index(0);
  int row1 = app_get_post_id_from_index(g_app->selected_idx);
  ASSERT_TRUE(row0 > 0);
  ASSERT_TRUE(row1 > 0);
  ASSERT_NOT_EQUAL(row0, row1);

  teardown_feed();
  PASS();
}

int main(void) {
  TEST_START("SocialFeed double-click");
  test_row_index_maps_to_post_id();
  TEST_END();
}
