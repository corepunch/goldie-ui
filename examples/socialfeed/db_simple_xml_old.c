#include "db_simple_xml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ═══════════════════════════════════════════════════════════════════════════

static void ensure_capacity_authors(simple_xml_db_t *db) {
  if (db->author_count >= db->author_capacity) {
    db->author_capacity = (db->author_capacity == 0) ? 16 : db->author_capacity * 2;
    db->authors = realloc(db->authors, db->author_capacity * sizeof(author_t));
  }
}

static void ensure_capacity_posts(simple_xml_db_t *db) {
  if (db->post_count >= db->post_capacity) {
    db->post_capacity = (db->post_capacity == 0) ? 32 : db->post_capacity * 2;
    db->posts = realloc(db->posts, db->post_capacity * sizeof(post_t));
  }
}

static void ensure_capacity_comments(simple_xml_db_t *db) {
  if (db->comment_count >= db->comment_capacity) {
    db->comment_capacity = (db->comment_capacity == 0) ? 64 : db->comment_capacity * 2;
    db->comments = realloc(db->comments, db->comment_capacity * sizeof(comment_t));
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════

simple_xml_db_t *db_create(const char *source_path) {
  simple_xml_db_t *db = calloc(1, sizeof(simple_xml_db_t));
  if (!db) return NULL;
  
  strncpy(db->source_path, source_path, sizeof(db->source_path) - 1);
  
  db->next_author_id = 1;
  db->next_post_id = 1;
  db->next_comment_id = 1;
  
  return db;
}

void db_free(simple_xml_db_t *db) {
  if (!db) return;
  
  free(db->authors);
  free(db->posts);
  free(db->comments);
  free(db);
}

bool db_load(simple_xml_db_t *db) {
  // TODO: Parse XML file and populate tables
  // For now, this is a stub - full XML parsing would use libxml2 or similar
  printf("db_load: Loading from %s\n", db->source_path);
  
  // Seed with example data (normally parsed from XML)
  db_author_insert(db, "alice", "avatar_alice.png");
  db_author_insert(db, "bob", "avatar_bob.png");
  db_author_insert(db, "carol", "avatar_carol.png");
  db_author_insert(db, "dave", "avatar_dave.png");
  db_author_insert(db, "eve", "avatar_eve.png");
  db_author_insert(db, "frank", "avatar_frank.png");
  
  db->dirty = false;
  return true;
}

bool db_save(simple_xml_db_t *db) {
  if (!db->dirty) return true;
  
  // TODO: Write tables back to XML file
  printf("db_save: Saving to %s\n", db->source_path);
  printf("  - %d authors\n", db->author_count);
  printf("  - %d posts\n", db->post_count);
  printf("  - %d comments\n", db->comment_count);
  
  db->dirty = false;
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// CRUD - Authors
// ═══════════════════════════════════════════════════════════════════════════

author_t *db_author_insert(simple_xml_db_t *db, const char *name, const char *avatar) {
  ensure_capacity_authors(db);
  
  author_t *author = &db->authors[db->author_count++];
  author->id = db->next_author_id++;
  strncpy(author->name, name, sizeof(author->name) - 1);
  strncpy(author->avatar, avatar ? avatar : "", sizeof(author->avatar) - 1);
  
  db->dirty = true;
  return author;
}

author_t *db_author_find_by_id(simple_xml_db_t *db, int id) {
  for (int i = 0; i < db->author_count; i++) {
    if (db->authors[i].id == id)
      return &db->authors[i];
  }
  return NULL;
}

author_t *db_author_find_by_name(simple_xml_db_t *db, const char *name) {
  for (int i = 0; i < db->author_count; i++) {
    if (strcmp(db->authors[i].name, name) == 0)
      return &db->authors[i];
  }
  return NULL;
}

bool db_author_update(simple_xml_db_t *db, author_t *author) {
  // Author is already in-place, just mark dirty
  db->dirty = true;
  return true;
}

bool db_author_delete(simple_xml_db_t *db, int id) {
  for (int i = 0; i < db->author_count; i++) {
    if (db->authors[i].id == id) {
      // Shift remaining authors down
      memmove(&db->authors[i], &db->authors[i + 1], 
              (db->author_count - i - 1) * sizeof(author_t));
      db->author_count--;
      db->dirty = true;
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// CRUD - Posts
// ═══════════════════════════════════════════════════════════════════════════

post_t *db_post_insert(simple_xml_db_t *db, int author_id, const char *title, const char *body) {
  ensure_capacity_posts(db);
  
  post_t *post = &db->posts[db->post_count++];
  post->id = db->next_post_id++;
  post->author_id = author_id;
  strncpy(post->title, title, sizeof(post->title) - 1);
  strncpy(post->body, body, sizeof(post->body) - 1);
  post->like_count = 0;
  post->comment_count = 0;
  
  db->dirty = true;
  return post;
}

post_t *db_post_find_by_id(simple_xml_db_t *db, int id) {
  for (int i = 0; i < db->post_count; i++) {
    if (db->posts[i].id == id)
      return &db->posts[i];
  }
  return NULL;
}

post_t **db_post_fetch_all(simple_xml_db_t *db, int *out_count) {
  *out_count = db->post_count;
  
  // Return array of pointers to posts
  post_t **result = malloc(db->post_count * sizeof(post_t *));
  for (int i = 0; i < db->post_count; i++) {
    result[i] = &db->posts[i];
  }
  
  return result;
}

bool db_post_update(simple_xml_db_t *db, post_t *post) {
  db->dirty = true;
  return true;
}

bool db_post_delete(simple_xml_db_t *db, int id) {
  for (int i = 0; i < db->post_count; i++) {
    if (db->posts[i].id == id) {
      // Delete all comments for this post first
      for (int j = db->comment_count - 1; j >= 0; j--) {
        if (db->comments[j].post_id == id) {
          db_comment_delete(db, db->comments[j].id);
        }
      }
      
      // Shift remaining posts down
      memmove(&db->posts[i], &db->posts[i + 1],
              (db->post_count - i - 1) * sizeof(post_t));
      db->post_count--;
      db->dirty = true;
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// CRUD - Comments
// ═══════════════════════════════════════════════════════════════════════════

comment_t *db_comment_insert(simple_xml_db_t *db, int post_id, int author_id, const char *text) {
  ensure_capacity_comments(db);
  
  comment_t *comment = &db->comments[db->comment_count++];
  comment->id = db->next_comment_id++;
  comment->post_id = post_id;
  comment->author_id = author_id;
  strncpy(comment->text, text, sizeof(comment->text) - 1);
  comment->like_count = 0;
  
  // Update post comment count
  db_post_update_comment_count(db, post_id);
  
  db->dirty = true;
  return comment;
}

comment_t *db_comment_find_by_id(simple_xml_db_t *db, int id) {
  for (int i = 0; i < db->comment_count; i++) {
    if (db->comments[i].id == id)
      return &db->comments[i];
  }
  return NULL;
}

comment_t **db_comment_fetch_by_post(simple_xml_db_t *db, int post_id, int *out_count) {
  // Count matching comments
  int count = 0;
  for (int i = 0; i < db->comment_count; i++) {
    if (db->comments[i].post_id == post_id)
      count++;
  }
  
  *out_count = count;
  if (count == 0) return NULL;
  
  // Collect pointers
  comment_t **result = malloc(count * sizeof(comment_t *));
  int idx = 0;
  for (int i = 0; i < db->comment_count; i++) {
    if (db->comments[i].post_id == post_id) {
      result[idx++] = &db->comments[i];
    }
  }
  
  return result;
}

bool db_comment_update(simple_xml_db_t *db, comment_t *comment) {
  db->dirty = true;
  return true;
}

bool db_comment_delete(simple_xml_db_t *db, int id) {
  for (int i = 0; i < db->comment_count; i++) {
    if (db->comments[i].id == id) {
      int post_id = db->comments[i].post_id;
      
      // Shift remaining comments down
      memmove(&db->comments[i], &db->comments[i + 1],
              (db->comment_count - i - 1) * sizeof(comment_t));
      db->comment_count--;
      
      // Update post comment count
      db_post_update_comment_count(db, post_id);
      
      db->dirty = true;
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Utility
// ═══════════════════════════════════════════════════════════════════════════

void db_post_update_comment_count(simple_xml_db_t *db, int post_id) {
  post_t *post = db_post_find_by_id(db, post_id);
  if (!post) return;
  
  int count = 0;
  for (int i = 0; i < db->comment_count; i++) {
    if (db->comments[i].post_id == post_id)
      count++;
  }
  
  post->comment_count = count;
}
