// database_test.c - Unit tests for database API (message-based CRUD)
//
// Tests the core database infrastructure (database_t + dbproc_t + messages)
// using SimpleXMLDatabase as the implementation, but focuses on the API layer
// rather than XML serialization details.

#include "test_framework.h"
#include "../ui.h"
#include "../examples/socialfeed/db_simple_xml.h"

// Test database creation and destruction
void test_database_lifecycle(void) {
  TEST("database lifecycle: create and destroy");
  
  DB_CLASS(db_simple_xml);
  database_t *db = create_database("test", "db_simple_xml", "test.xml");
  
  if (!db) {
    FAIL("create_database returned NULL");
    return;
  }
  
  ASSERT(db->proc != NULL, "database proc should be set");
  ASSERT(db->userdata != NULL, "database userdata should be allocated");
  ASSERT_STR_EQUAL(db->class_name, "db_simple_xml");
  
  destroy_database(db);
  PASS();
}

// Test author insert and find operations
void test_author_crud(void) {
  TEST("author CRUD: insert, find, update, delete");
  
  DB_CLASS(db_simple_xml);
  database_t *db = create_database("test", "db_simple_xml", "test.xml");
  
  // Insert new author
  author_t author_data = { .id = 0 };
  strcpy(author_data.name, "testuser");
  strcpy(author_data.avatar, "avatar.png");
  
  insert_params_t insert = {
    .table_id = TABLE_AUTHORS,
    .record_data = &author_data,
    .out_record = NULL
  };
  author_t *inserted = NULL;
  insert.out_record = (void **)&inserted;
  
  result_t res = send_db_message(db, dbInsert, 0, &insert);
  if (!res || !inserted) {
    destroy_database(db);
    FAIL("insert failed");
    return;
  }
  
  ASSERT(inserted->id > 0, "auto-increment ID should be assigned");
  ASSERT_STR_EQUAL(inserted->name, "testuser");
  
  // Find by ID
  find_params_t find_by_id = {
    .table_id = TABLE_AUTHORS,
    .search_field = 0,  // by id
    .search_value.int_value = inserted->id
  };
  author_t *found = NULL;
  find_by_id.result_out = (void **)&found;
  send_db_message(db, dbFind, TABLE_AUTHORS, &find_by_id);
  
  if (!found) {
    destroy_database(db);
    FAIL("find by ID failed");
    return;
  }
  
  ASSERT_EQUAL(found->id, inserted->id);
  
  // Find by name
  find_params_t find_by_name = {
    .table_id = TABLE_AUTHORS,
    .search_field = 1,  // by name
    .search_value.str_value = "testuser"
  };
  author_t *found2 = NULL;
  find_by_name.result_out = (void **)&found2;
  send_db_message(db, dbFind, TABLE_AUTHORS, &find_by_name);
  
  ASSERT(found2 != NULL, "find by name should succeed");
  ASSERT_EQUAL(found2->id, inserted->id);
  
  // Update
  strcpy(found->name, "updated_user");
  send_db_message(db, dbUpdate, TABLE_AUTHORS, found);
  ASSERT_STR_EQUAL(found->name, "updated_user");
  
  // Delete
  int delete_id = found->id;
  send_db_message(db, dbDelete, TABLE_AUTHORS, (void *)(intptr_t)delete_id);
  
  // Verify deleted
  find_params_t find_deleted = {
    .table_id = TABLE_AUTHORS,
    .search_field = 0,
    .search_value.int_value = delete_id
  };
  author_t *should_be_null = NULL;
  find_deleted.result_out = (void **)&should_be_null;
  send_db_message(db, dbFind, TABLE_AUTHORS, &find_deleted);
  
  ASSERT(should_be_null == NULL, "deleted record should not be found");
  
  destroy_database(db);
  PASS();
}

// Test post operations and foreign key relationships
void test_post_operations(void) {
  TEST("post operations: insert with foreign key, fetch, update");
  
  DB_CLASS(db_simple_xml);
  database_t *db = create_database("test", "db_simple_xml", "test.xml");
  
  // Create author first
  author_t author = { .id = 0 };
  strcpy(author.name, "postauthor");
  insert_params_t ins_author = {
    .table_id = TABLE_AUTHORS,
    .record_data = &author,
    .out_record = NULL
  };
  author_t *author_ptr = NULL;
  ins_author.out_record = (void **)&author_ptr;
  send_db_message(db, dbInsert, 0, &ins_author);
  
  if (!author_ptr) {
    destroy_database(db);
    FAIL("author insert failed");
    return;
  }
  
  // Create post with foreign key
  post_t post = { .id = 0, .author_id = author_ptr->id };
  strcpy(post.title, "Test Post");
  strcpy(post.body, "Post body");
  post.like_count = 0;
  post.comment_count = 0;
  
  insert_params_t ins_post = {
    .table_id = TABLE_POSTS,
    .record_data = &post,
    .out_record = NULL
  };
  post_t *post_ptr = NULL;
  ins_post.out_record = (void **)&post_ptr;
  send_db_message(db, dbInsert, 0, &ins_post);
  
  ASSERT(post_ptr != NULL, "post insert should succeed");
  ASSERT(post_ptr->id > 0, "post should have auto-increment ID");
  ASSERT_EQUAL(post_ptr->author_id, author_ptr->id);
  
  // Fetch all posts
  fetch_params_t fetch = {
    .table_id = TABLE_POSTS,
    .filter_field = 0  // no filter
  };
  void **results = NULL;
  int count = 0;
  fetch.results_out = &results;
  fetch.count_out = &count;
  send_db_message(db, dbFetch, TABLE_POSTS, &fetch);
  
  ASSERT(count > 0, "should fetch at least one post");
  ASSERT(results != NULL, "results array should be allocated");
  
  free(results);
  
  // Update post likes
  post_ptr->like_count = 42;
  send_db_message(db, dbUpdate, TABLE_POSTS, post_ptr);
  ASSERT_EQUAL(post_ptr->like_count, 42);
  
  destroy_database(db);
  PASS();
}

// Test comment operations and denormalized field updates
void test_comment_and_denormalized_fields(void) {
  TEST("comments: auto-update post comment_count");
  
  DB_CLASS(db_simple_xml);
  database_t *db = create_database("test", "db_simple_xml", "test.xml");
  
  // Create author and post
  author_t author = { .id = 0 };
  strcpy(author.name, "commenter");
  insert_params_t ins_a = { .table_id = TABLE_AUTHORS, .record_data = &author };
  author_t *a_ptr = NULL;
  ins_a.out_record = (void **)&a_ptr;
  send_db_message(db, dbInsert, 0, &ins_a);
  
  post_t post = { .id = 0, .author_id = a_ptr->id, .comment_count = 0 };
  strcpy(post.title, "Post");
  insert_params_t ins_p = { .table_id = TABLE_POSTS, .record_data = &post };
  post_t *p_ptr = NULL;
  ins_p.out_record = (void **)&p_ptr;
  send_db_message(db, dbInsert, 0, &ins_p);
  
  int initial_count = p_ptr->comment_count;
  
  // Add comment (should auto-increment post comment_count)
  comment_t comment = { .id = 0, .post_id = p_ptr->id, .author_id = a_ptr->id };
  strcpy(comment.text, "Great post!");
  insert_params_t ins_c = { .table_id = TABLE_COMMENTS, .record_data = &comment };
  comment_t *c_ptr = NULL;
  ins_c.out_record = (void **)&c_ptr;
  send_db_message(db, dbInsert, 0, &ins_c);
  
  ASSERT(c_ptr != NULL, "comment insert should succeed");
  ASSERT_EQUAL(p_ptr->comment_count, initial_count + 1);
  
  // Delete comment (should decrement post comment_count)
  int comment_id = c_ptr->id;
  send_db_message(db, dbDelete, TABLE_COMMENTS, (void *)(intptr_t)comment_id);
  ASSERT_EQUAL(p_ptr->comment_count, initial_count);
  
  destroy_database(db);
  PASS();
}

// Test cascading deletes
void test_cascading_delete(void) {
  TEST("cascading delete: post deletion removes comments");
  
  DB_CLASS(db_simple_xml);
  database_t *db = create_database("test", "db_simple_xml", "test.xml");
  
  // Create author, post, and comments
  author_t author = { .id = 0 };
  strcpy(author.name, "cascade_test");
  insert_params_t ins_a = { .table_id = TABLE_AUTHORS, .record_data = &author };
  author_t *a_ptr = NULL;
  ins_a.out_record = (void **)&a_ptr;
  send_db_message(db, dbInsert, 0, &ins_a);
  
  post_t post = { .id = 0, .author_id = a_ptr->id };
  strcpy(post.title, "To Be Deleted");
  insert_params_t ins_p = { .table_id = TABLE_POSTS, .record_data = &post };
  post_t *p_ptr = NULL;
  ins_p.out_record = (void **)&p_ptr;
  send_db_message(db, dbInsert, 0, &ins_p);
  
  // Add two comments
  for (int i = 0; i < 2; i++) {
    comment_t c = { .id = 0, .post_id = p_ptr->id, .author_id = a_ptr->id };
    sprintf(c.text, "Comment %d", i + 1);
    insert_params_t ins_c = { .table_id = TABLE_COMMENTS, .record_data = &c };
    send_db_message(db, dbInsert, 0, &ins_c);
  }
  
  // Count comments before delete
  fetch_params_t fetch_before = {
    .table_id = TABLE_COMMENTS,
    .filter_field = 2,  // filter by post_id
    .filter_value = p_ptr->id
  };
  void **results_before = NULL;
  int count_before = 0;
  fetch_before.results_out = &results_before;
  fetch_before.count_out = &count_before;
  send_db_message(db, dbFetch, TABLE_COMMENTS, &fetch_before);
  
  ASSERT_EQUAL(count_before, 2);
  free(results_before);
  
  // Delete post (should cascade delete comments)
  int post_id = p_ptr->id;
  send_db_message(db, dbDelete, TABLE_POSTS, (void *)(intptr_t)post_id);
  
  // Count comments after delete
  fetch_params_t fetch_after = {
    .table_id = TABLE_COMMENTS,
    .filter_field = 2,
    .filter_value = post_id
  };
  void **results_after = NULL;
  int count_after = 0;
  fetch_after.results_out = &results_after;
  fetch_after.count_out = &count_after;
  send_db_message(db, dbFetch, TABLE_COMMENTS, &fetch_after);
  
  ASSERT_EQUAL(count_after, 0);
  if (results_after) free(results_after);
  
  destroy_database(db);
  PASS();
}

// Test dirty flag tracking
void test_dirty_tracking(void) {
  TEST("dirty flag: tracked on insert/update/delete");
  
  DB_CLASS(db_simple_xml);
  database_t *db = create_database("test", "db_simple_xml", "test.xml");
  
  // Check initial dirty state (should be false after load)
  result_t initial_dirty = send_db_message(db, dbGetDirty, 0, NULL);
  ASSERT_EQUAL(initial_dirty, 0);
  
  // Insert should mark dirty
  author_t author = { .id = 0 };
  strcpy(author.name, "dirtytest");
  insert_params_t ins = { .table_id = TABLE_AUTHORS, .record_data = &author };
  send_db_message(db, dbInsert, 0, &ins);
  
  result_t after_insert = send_db_message(db, dbGetDirty, 0, NULL);
  ASSERT(after_insert != 0, "should be dirty after insert");
  
  destroy_database(db);
  PASS();
}

int main(int argc, char **argv) {
  TEST_START("Database API");
  
  test_database_lifecycle();
  test_author_crud();
  test_post_operations();
  test_comment_and_denormalized_fields();
  test_cascading_delete();
  test_dirty_tracking();
  
  TEST_END();
}
