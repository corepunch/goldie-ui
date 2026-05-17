// db_test.c - Test program for SimpleXMLDatabase (message-based API)
#include "db_simple_xml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_authors(database_t *db) {
  printf("\n=== Testing Authors ===\n");
  
  // Find existing author by name
  find_params_t find_alice = {
    .table_id = TABLE_AUTHORS,
    .search_field = 1,  // by name
    .search_value.str_value = "alice"
  };
  author_t *alice_ptr = NULL;
  find_alice.result_out = (void **)&alice_ptr;
  send_db_message(db, dbFind, TABLE_AUTHORS, &find_alice);
  assert(alice_ptr != NULL);
  printf("Found author: %s (id=%d)\n", alice_ptr->name, alice_ptr->id);
  
  // Insert new author
  author_t new_author_data = { .id = 0 };
  strcpy(new_author_data.name, "mallory");
  strcpy(new_author_data.avatar, "avatar_mallory.png");
  
  insert_params_t insert = {
    .table_id = TABLE_AUTHORS,
    .record_data = &new_author_data,
    .out_record = NULL
  };
  author_t *new_author = NULL;
  insert.out_record = (void **)&new_author;
  send_db_message(db, dbInsert, 0, &insert);
  printf("Inserted author: %s (id=%d)\n", new_author->name, new_author->id);
  
  // Update author
  strcpy(new_author->name, "mallory_updated");
  send_db_message(db, dbUpdate, TABLE_AUTHORS, new_author);
  printf("Updated author: %s\n", new_author->name);
  
  // Delete author
  send_db_message(db, dbDelete, TABLE_AUTHORS, (void *)(intptr_t)new_author->id);
  printf("Deleted author id=%d\n", new_author->id);
}

static void test_posts(database_t *db) {
  printf("\n=== Testing Posts ===\n");
  
  // Get alice's record
  find_params_t find_alice = {
    .table_id = TABLE_AUTHORS,
    .search_field = 1,
    .search_value.str_value = "alice"
  };
  author_t *alice_ptr = NULL;
  find_alice.result_out = (void **)&alice_ptr;
  send_db_message(db, dbFind, TABLE_AUTHORS, &find_alice);
  assert(alice_ptr != NULL);
  
  // Insert new post
  post_t post_data = { .id = 0, .author_id = alice_ptr->id };
  strcpy(post_data.title, "Testing SimpleXMLDatabase");
  strcpy(post_data.body, "This is a test post created from C code!");
  
  insert_params_t insert = {
    .table_id = TABLE_POSTS,
    .record_data = &post_data,
    .out_record = NULL
  };
  post_t *post = NULL;
  insert.out_record = (void **)&post;
  send_db_message(db, dbInsert, 0, &insert);
  
  printf("Inserted post: '%s' by author_id=%d (post id=%d)\n", 
         post->title, post->author_id, post->id);
  
  // Fetch all posts
  fetch_params_t fetch_all = { .table_id = TABLE_POSTS, .filter_field = 0 };
  void **results = NULL;
  int count = 0;
  fetch_all.results_out = &results;  // Note: pass address of pointer
  fetch_all.count_out = &count;
  send_db_message(db, dbFetch, TABLE_POSTS, &fetch_all);
  
  printf("Total posts: %d\n", count);
  for (int i = 0; i < count; i++) {
    post_t *p = (post_t *)results[i];
    printf("  - Post %d: '%s' (author_id=%d, comments=%d)\n",
           p->id, p->title, p->author_id, p->comment_count);
  }
  free(results);
  
  // Update post
  post->like_count = 5;
  send_db_message(db, dbUpdate, TABLE_POSTS, post);
  printf("Updated post likes to %d\n", post->like_count);
}

static void test_comments(database_t *db) {
  printf("\n=== Testing Comments ===\n");
  
  // Get first post
  find_params_t find_post = {
    .table_id = TABLE_POSTS,
    .search_field = 0,
    .search_value.int_value = 1
  };
  post_t *post_ptr = NULL;
  find_post.result_out = (void **)&post_ptr;
  send_db_message(db, dbFind, TABLE_POSTS, &find_post);
  
  // Get bob
  find_params_t find_bob = {
    .table_id = TABLE_AUTHORS,
    .search_field = 1,
    .search_value.str_value = "bob"
  };
  author_t *bob_ptr = NULL;
  find_bob.result_out = (void **)&bob_ptr;
  send_db_message(db, dbFind, TABLE_AUTHORS, &find_bob);
  
  if (post_ptr && bob_ptr) {
    // Insert comment
    comment_t comment_data = { .id = 0, .post_id = post_ptr->id, .author_id = bob_ptr->id };
    strcpy(comment_data.text, "Great post! This database API is clean.");
    
    insert_params_t insert = {
      .table_id = TABLE_COMMENTS,
      .record_data = &comment_data,
      .out_record = NULL
    };
    comment_t *comment = NULL;
    insert.out_record = (void **)&comment;
    send_db_message(db, dbInsert, 0, &insert);
    
    printf("Inserted comment: '%s' by author_id=%d on post_id=%d\n",
           comment->text, comment->author_id, comment->post_id);
    
    // Fetch comments for post
    fetch_params_t fetch_comments = { 
      .table_id = TABLE_COMMENTS, 
      .filter_field = 2,  // filter by post_id
      .filter_value = post_ptr->id 
    };
    void **results = NULL;
    int count = 0;
    fetch_comments.results_out = &results;
    fetch_comments.count_out = &count;
    send_db_message(db, dbFetch, TABLE_COMMENTS, &fetch_comments);
    
    printf("Post '%s' has %d comments:\n", post_ptr->title, count);
    for (int i = 0; i < count; i++) {
      comment_t *c = (comment_t *)results[i];
      printf("  - Comment %d: '%s' (likes=%d)\n", c->id, c->text, c->like_count);
    }
    free(results);
    
    // Verify post comment count was updated
    printf("Post comment_count field: %d\n", post_ptr->comment_count);
    
    // Delete comment
    send_db_message(db, dbDelete, TABLE_COMMENTS, (void *)(intptr_t)comment->id);
    printf("Deleted comment id=%d\n", comment->id);
    printf("Post comment_count after delete: %d\n", post_ptr->comment_count);
  }
}

static void test_cascading_delete(database_t *db) {
  printf("\n=== Testing Cascading Delete ===\n");
  
  // Get alice
  find_params_t find_alice = {
    .table_id = TABLE_AUTHORS,
    .search_field = 1,
    .search_value.str_value = "alice"
  };
  author_t *alice_ptr = NULL;
  find_alice.result_out = (void **)&alice_ptr;
  send_db_message(db, dbFind, TABLE_AUTHORS, &find_alice);
  
  // Create a post
  post_t post_data = { .id = 0, .author_id = alice_ptr->id };
  strcpy(post_data.title, "Temporary Post");
  strcpy(post_data.body, "This will be deleted");
  
  insert_params_t insert_post = {
    .table_id = TABLE_POSTS,
    .record_data = &post_data,
    .out_record = NULL
  };
  post_t *post = NULL;
  insert_post.out_record = (void **)&post;
  send_db_message(db, dbInsert, 0, &insert_post);
  printf("Created post id=%d\n", post->id);
  
  // Add comments
  comment_t comment1 = { .id = 0, .post_id = post->id, .author_id = alice_ptr->id };
  strcpy(comment1.text, "Comment 1");
  insert_params_t ins1 = { .table_id = TABLE_COMMENTS, .record_data = &comment1, .out_record = NULL };
  send_db_message(db, dbInsert, 0, &ins1);
  
  comment_t comment2 = { .id = 0, .post_id = post->id, .author_id = alice_ptr->id };
  strcpy(comment2.text, "Comment 2");
  insert_params_t ins2 = { .table_id = TABLE_COMMENTS, .record_data = &comment2, .out_record = NULL };
  send_db_message(db, dbInsert, 0, &ins2);
  printf("Added 2 comments to post\n");
  
  // Get comment count before delete
  fetch_params_t fetch_before = { .table_id = TABLE_COMMENTS, .filter_field = 0 };
  void **res_before = NULL;
  int count_before = 0;
  fetch_before.results_out = &res_before;
  fetch_before.count_out = &count_before;
  send_db_message(db, dbFetch, TABLE_COMMENTS, &fetch_before);
  free(res_before);
  
  // Delete post (should cascade delete comments)
  send_db_message(db, dbDelete, TABLE_POSTS, (void *)(intptr_t)post->id);
  printf("Deleted post id=%d\n", post->id);
  
  // Get comment count after delete
  fetch_params_t fetch_after = { .table_id = TABLE_COMMENTS, .filter_field = 0 };
  void **res_after = NULL;
  int count_after = 0;
  fetch_after.results_out = &res_after;
  fetch_after.count_out = &count_after;
  send_db_message(db, dbFetch, TABLE_COMMENTS, &fetch_after);
  free(res_after);
  
  printf("Comments before: %d, after: %d (should be -2)\n", count_before, count_after);
}

int main(int argc, char **argv) {
  printf("SimpleXMLDatabase Test Program (Message-Based API)\n");
  printf("==================================================\n");
  
  // Register database class
  DB_CLASS(db_simple_xml);
  
  // Create database (sends dbCreate and dbLoad)
  database_t *db = create_database("socialfeed", "db_simple_xml", "socialfeed_seed.xml");
  if (!db) {
    fprintf(stderr, "Failed to create database\n");
    return 1;
  }
  
  // Run tests
  test_authors(db);
  test_posts(db);
  test_comments(db);
  test_cascading_delete(db);
  
  // Cleanup (sends dbSave if dirty, then dbDestroy)
  printf("\n=== Cleanup ===\n");
  destroy_database(db);
  
  printf("\n=== All Tests Passed ===\n");
  return 0;
}
