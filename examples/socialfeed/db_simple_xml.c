// SimpleXMLDatabase implementation (message-based proc pattern)
#include "../../ui.h"
#include "socialfeed.h"  // Includes generated types: db_author_t, db_post_t, db_comment_t + field metadata
#include "../../platform/platform.h"  // for LOWORD/HIWORD
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

// ═══════════════════════════════════════════════════════════════════════════
// Internal Context (stored in database_t->userdata)
// Uses generated types from orionc: db_author_t, db_post_t, db_comment_t
// Field metadata arrays: authors_fields[], posts_fields[], comments_fields[]
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  // In-memory tables (dynamic arrays) - using generated types
  db_author_t *authors;
  int author_count;
  int author_capacity;
  
  db_post_t *posts;
  int post_count;
  int post_capacity;
  
  db_comment_t *comments;
  int comment_count;
  int comment_capacity;
  
  // Auto-increment counters
  int next_author_id;
  int next_post_id;
  int next_comment_id;
} simple_xml_context_t;

// ═══════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ═══════════════════════════════════════════════════════════════════════════

static void ensure_capacity_authors(simple_xml_context_t *ctx) {
  if (ctx->author_count >= ctx->author_capacity) {
    ctx->author_capacity = (ctx->author_capacity == 0) ? 16 : ctx->author_capacity * 2;
    ctx->authors = realloc(ctx->authors, ctx->author_capacity * sizeof(db_author_t));
  }
}

static void ensure_capacity_posts(simple_xml_context_t *ctx) {
  if (ctx->post_count >= ctx->post_capacity) {
    ctx->post_capacity = (ctx->post_capacity == 0) ? 32 : ctx->post_capacity * 2;
    ctx->posts = realloc(ctx->posts, ctx->post_capacity * sizeof(db_post_t));
  }
}

static void ensure_capacity_comments(simple_xml_context_t *ctx) {
  if (ctx->comment_count >= ctx->comment_capacity) {
    ctx->comment_capacity = (ctx->comment_capacity == 0) ? 64 : ctx->comment_capacity * 2;
    ctx->comments = realloc(ctx->comments, ctx->comment_capacity * sizeof(db_comment_t));
  }
}

static void array_ensure_capacity(void **rows, int *capacity, int count, size_t row_size, int initial_capacity) {
  if (count < *capacity)
    return;
  int next_capacity = (*capacity == 0) ? initial_capacity : (*capacity * 2);
  *rows = realloc(*rows, (size_t)next_capacity * row_size);
  *capacity = next_capacity;
}

static void *array_append_with_auto_id(void **rows, int *count, int *capacity, size_t row_size, size_t id_offset, int *next_id, int initial_capacity) {
  array_ensure_capacity(rows, capacity, *count, row_size, initial_capacity);
  char *row = (char *)(*rows) + ((size_t)(*count) * row_size);
  *count = *count + 1;
  *(int *)(row + id_offset) = (*next_id)++;
  return row;
}

static void *array_find_by_id(void *rows, int count, size_t row_size, size_t id_offset, int id) {
  char *base = (char *)rows;
  for (int i = 0; i < count; i++) {
    char *row = base + ((size_t)i * row_size);
    if (*(int *)(row + id_offset) == id)
      return row;
  }
  return NULL;
}

static void *array_find_by_string(void *rows, int count, size_t row_size, size_t str_offset, const char *value) {
  char *base = (char *)rows;
  for (int i = 0; i < count; i++) {
    char *row = base + ((size_t)i * row_size);
    if (strcmp(row + str_offset, value) == 0)
      return row;
  }
  return NULL;
}

static bool array_delete_by_id(void *rows, int *count, size_t row_size, size_t id_offset, int id) {
  char *base = (char *)rows;
  for (int i = 0; i < *count; i++) {
    char *row = base + ((size_t)i * row_size);
    if (*(int *)(row + id_offset) == id) {
      memmove(row, row + row_size, ((size_t)(*count - i - 1) * row_size));
      *count = *count - 1;
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// CRUD Helpers - Authors
// ═══════════════════════════════════════════════════════════════════════════

static db_author_t *author_insert(simple_xml_context_t *ctx, const char *name, const char *avatar) {
  db_author_t *author = (db_author_t *)array_append_with_auto_id(
      (void **)&ctx->authors,
      &ctx->author_count,
      &ctx->author_capacity,
      sizeof(db_author_t),
      offsetof(db_author_t, id),
      &ctx->next_author_id,
      16);
  strncpy(author->name, name, sizeof(author->name) - 1);
  strncpy(author->avatar, avatar ? avatar : "", sizeof(author->avatar) - 1);
  
  return author;
}

static db_author_t *author_find_by_id(simple_xml_context_t *ctx, int id) {
  return (db_author_t *)array_find_by_id(ctx->authors, ctx->author_count,
                                         sizeof(db_author_t), offsetof(db_author_t, id), id);
}

static db_author_t *author_find_by_name(simple_xml_context_t *ctx, const char *name) {
  return (db_author_t *)array_find_by_string(ctx->authors, ctx->author_count,
                                             sizeof(db_author_t), offsetof(db_author_t, name), name);
}

static bool author_delete(simple_xml_context_t *ctx, int id) {
  return array_delete_by_id(ctx->authors, &ctx->author_count,
                            sizeof(db_author_t), offsetof(db_author_t, id), id);
}

// ═══════════════════════════════════════════════════════════════════════════
// CRUD Helpers - Posts
// ═══════════════════════════════════════════════════════════════════════════

static db_post_t *post_insert(simple_xml_context_t *ctx, int author_id, 
                           const char *title, const char *body) {
  db_post_t *post = (db_post_t *)array_append_with_auto_id(
      (void **)&ctx->posts,
      &ctx->post_count,
      &ctx->post_capacity,
      sizeof(db_post_t),
      offsetof(db_post_t, id),
      &ctx->next_post_id,
      32);
  post->author_id = author_id;
  strncpy(post->title, title, sizeof(post->title) - 1);
  strncpy(post->body, body, sizeof(post->body) - 1);
  post->like_count = 0;
  post->comment_count = 0;
  
  return post;
}

static db_post_t *post_find_by_id(simple_xml_context_t *ctx, int id) {
  return (db_post_t *)array_find_by_id(ctx->posts, ctx->post_count,
                                       sizeof(db_post_t), offsetof(db_post_t, id), id);
}

static bool post_delete(simple_xml_context_t *ctx, int id) {
  // First delete all comments for this post (cascading delete)
  for (int i = ctx->comment_count - 1; i >= 0; i--) {
    if (ctx->comments[i].post_id == id) {
      memmove(&ctx->comments[i], &ctx->comments[i + 1],
              (ctx->comment_count - i - 1) * sizeof(db_comment_t));
      ctx->comment_count--;
    }
  }
  
  // Then delete the post
  return array_delete_by_id(ctx->posts, &ctx->post_count,
                            sizeof(db_post_t), offsetof(db_post_t, id), id);
}

// ═══════════════════════════════════════════════════════════════════════════
// CRUD Helpers - Comments
// ═══════════════════════════════════════════════════════════════════════════

static db_comment_t *comment_insert(simple_xml_context_t *ctx, int post_id, 
                                 int author_id, const char *text) {
  db_comment_t *comment = (db_comment_t *)array_append_with_auto_id(
      (void **)&ctx->comments,
      &ctx->comment_count,
      &ctx->comment_capacity,
      sizeof(db_comment_t),
      offsetof(db_comment_t, id),
      &ctx->next_comment_id,
      64);
  comment->post_id = post_id;
  comment->author_id = author_id;
  strncpy(comment->text, text, sizeof(comment->text) - 1);
  comment->like_count = 0;
  
  // Update post comment count
  db_post_t *post = post_find_by_id(ctx, post_id);
  if (post) post->comment_count++;
  
  return comment;
}

static db_comment_t *comment_find_by_id(simple_xml_context_t *ctx, int id) {
  return (db_comment_t *)array_find_by_id(ctx->comments, ctx->comment_count,
                                          sizeof(db_comment_t), offsetof(db_comment_t, id), id);
}

static bool comment_delete(simple_xml_context_t *ctx, int id) {
  db_comment_t *comment = comment_find_by_id(ctx, id);
  if (!comment)
    return false;

  int post_id = comment->post_id;
  if (!array_delete_by_id(ctx->comments, &ctx->comment_count,
                          sizeof(db_comment_t), offsetof(db_comment_t, id), id)) {
    return false;
  }

  // Update post comment count
  db_post_t *post = post_find_by_id(ctx, post_id);
  if (post && post->comment_count > 0)
    post->comment_count--;

  return true;
}

static const db_source_def_t db_simple_xml_sources[] = {
  { ID_DB_SOURCE_AUTHORS, ID_DB_MODEL_AUTHOR },
  { ID_DB_SOURCE_POSTS, ID_DB_MODEL_POST },
  { ID_DB_SOURCE_COMMENTS, ID_DB_MODEL_COMMENT },
};

static const db_action_def_t db_simple_xml_actions[] = {
  { ID_DB_ACTION_FETCH_POSTS, DB_ACTION_FETCH, ID_DB_SOURCE_POSTS, ID_MAIN_WINDOW_FEED },
  { ID_DB_ACTION_FETCH_COMMENTS, DB_ACTION_FETCH, ID_DB_SOURCE_COMMENTS, ID_POST_DETAIL_COMMENTS },
  { ID_DB_ACTION_INSERT_POSTS, DB_ACTION_INSERT, ID_DB_SOURCE_POSTS, 0 },
  { ID_DB_ACTION_INSERT_COMMENTS, DB_ACTION_INSERT, ID_DB_SOURCE_COMMENTS, 0 },
};

static const db_api_def_t db_simple_xml_api = {
  STATIC_ARRAY(db_simple_xml_sources),
  NULL,
  0,
  STATIC_ARRAY(db_simple_xml_actions),
  NULL,
  0,
};

// ═══════════════════════════════════════════════════════════════════════════
// Database Procedure (analogous to winproc_t)
// ═══════════════════════════════════════════════════════════════════════════

lresult_t db_simple_xml(database_t *db, uint32_t msg, uint32_t wparam, void *lparam) {
  simple_xml_context_t *ctx = (simple_xml_context_t *)db->userdata;
  
  switch (msg) {
    case dbCreate: {
      // Allocate context (analogous to window userdata allocation)
      ctx = calloc(1, sizeof(simple_xml_context_t));
      if (!ctx) return 0;
      
      ctx->next_author_id = 1;
      ctx->next_post_id = 1;
      ctx->next_comment_id = 1;
      
      db->userdata = ctx;
      return 1;
    }
    
    case dbDestroy: {
      if (!ctx) return 0;
      
      free(ctx->authors);
      free(ctx->posts);
      free(ctx->comments);
      free(ctx);
      db->userdata = NULL;
      return 1;
    }
    
    case dbLoad: {
      if (!ctx) return 0;
      
      // Parse XML file from db->source_path using reflection
      xmlDoc *doc = xmlReadFile(db->source_path, NULL, 0);
      if (!doc) {
        printf("db_simple_xml: Failed to parse %s\n", db->source_path);
        return 0;
      }
      
      xmlNode *root = xmlDocGetRootElement(doc);
      if (!root) {
        xmlFreeDoc(doc);
        return 0;
      }
      
      // Parse each table using generated field metadata (no hardcoded field names!)
      for (xmlNode *table = root->children; table; table = table->next) {
        if (table->type != XML_ELEMENT_NODE) continue;
        
        if (xmlStrcmp(table->name, (const xmlChar *)"authors") == 0) {
          // Load authors using reflection
          for (xmlNode *row = table->children; row; row = row->next) {
            if (row->type != XML_ELEMENT_NODE) continue;
            if (xmlStrcmp(row->name, (const xmlChar *)"author") != 0) continue;
            
            db_author_t author;
            if (db_load_record_from_xml(row, &author, STATIC_ARRAY(authors_fields))) {
              ensure_capacity_authors(ctx);
              ctx->authors[ctx->author_count++] = author;
              if (author.id >= ctx->next_author_id)
                ctx->next_author_id = author.id + 1;
            }
          }
        }
        else if (xmlStrcmp(table->name, (const xmlChar *)"posts") == 0) {
          // Load posts using reflection
          for (xmlNode *row = table->children; row; row = row->next) {
            if (row->type != XML_ELEMENT_NODE) continue;
            if (xmlStrcmp(row->name, (const xmlChar *)"post") != 0) continue;
            
            db_post_t post;
            if (db_load_record_from_xml(row, &post, STATIC_ARRAY(posts_fields))) {
              ensure_capacity_posts(ctx);
              ctx->posts[ctx->post_count++] = post;
              if (post.id >= ctx->next_post_id)
                ctx->next_post_id = post.id + 1;
            }
          }
        }
        else if (xmlStrcmp(table->name, (const xmlChar *)"comments") == 0) {
          // Load comments using reflection
          for (xmlNode *row = table->children; row; row = row->next) {
            if (row->type != XML_ELEMENT_NODE) continue;
            if (xmlStrcmp(row->name, (const xmlChar *)"comment") != 0) continue;
            
            db_comment_t comment;
            if (db_load_record_from_xml(row, &comment, STATIC_ARRAY(comments_fields))) {
              ensure_capacity_comments(ctx);
              ctx->comments[ctx->comment_count++] = comment;
              if (comment.id >= ctx->next_comment_id)
                ctx->next_comment_id = comment.id + 1;
            }
          }
        }
      }
      
      xmlFreeDoc(doc);
      
      printf("db_simple_xml: Loaded from %s (reflection-based)\n", db->source_path);
      printf("  - %d authors\n", ctx->author_count);
      printf("  - %d posts\n", ctx->post_count);
      printf("  - %d comments\n", ctx->comment_count);
      
      db->dirty = false;
      return 1;
    }
    
    case dbSave: {
      if (!ctx || !db->dirty) return 0;
      
      // Create XML document
      xmlDocPtr doc = xmlNewDoc((const xmlChar *)"1.0");
      if (!doc) {
        printf("db_simple_xml: Failed to create XML document\n");
        return 0;
      }
      
      // Create root element
      xmlNodePtr root = xmlNewNode(NULL, (const xmlChar *)"database");
      xmlDocSetRootElement(doc, root);
      
      // Serialize authors table
      xmlNodePtr authors_table = xmlNewChild(root, NULL, (const xmlChar *)"authors", NULL);
      for (int i = 0; i < ctx->author_count; i++) {
        db_save_record_to_xml(authors_table, "author", &ctx->authors[i],
                              STATIC_ARRAY(authors_fields));
      }
      
      // Serialize posts table
      xmlNodePtr posts_table = xmlNewChild(root, NULL, (const xmlChar *)"posts", NULL);
      for (int i = 0; i < ctx->post_count; i++) {
        db_save_record_to_xml(posts_table, "post", &ctx->posts[i],
                              STATIC_ARRAY(posts_fields));
      }
      
      // Serialize comments table
      xmlNodePtr comments_table = xmlNewChild(root, NULL, (const xmlChar *)"comments", NULL);
      for (int i = 0; i < ctx->comment_count; i++) {
        db_save_record_to_xml(comments_table, "comment", &ctx->comments[i],
                              STATIC_ARRAY(comments_fields));
      }
      
      // Save to file with formatting
      int result = xmlSaveFormatFileEnc(db->source_path, doc, "UTF-8", 1);
      xmlFreeDoc(doc);
      
      if (result == -1) {
        printf("db_simple_xml: Failed to write %s\n", db->source_path);
        return 0;
      }
      
      printf("db_simple_xml: Saved to %s\n", db->source_path);
      printf("  - %d authors\n", ctx->author_count);
      printf("  - %d posts\n", ctx->post_count);
      printf("  - %d comments\n", ctx->comment_count);
      
      db->dirty = false;
      return 1;
    }
    
    case dbInsert: {
      if (!ctx) return (lresult_t)NULL;
      
      void *record_data = lparam;
      if (!record_data) return (lresult_t)NULL;
      
      int table_id = wparam;
      
      switch (table_id) {
        case ID_DB_AUTHORS: {
          db_author_t *data = (db_author_t *)record_data;
          db_author_t *rec = author_insert(ctx, data->name, data->avatar);
          db->dirty = true;
          return (lresult_t)rec;
        }
        
        case ID_DB_POSTS: {
          db_post_t *data = (db_post_t *)record_data;
          db_post_t *rec = post_insert(ctx, data->author_id, data->title, data->body);
          db->dirty = true;
          return (lresult_t)rec;
        }
        
        case ID_DB_COMMENTS: {
          db_comment_t *data = (db_comment_t *)record_data;
          db_comment_t *rec = comment_insert(ctx, data->post_id, data->author_id, data->text);
          db->dirty = true;
          return (lresult_t)rec;
        }
      }
      return (lresult_t)NULL;
    }
    
    case dbUpdate: {
      if (!ctx) return 0;
      
      // Record is already updated in-place, just mark dirty
      db->dirty = true;
      return 1;
    }
    
    case dbDelete: {
      if (!ctx) return 0;
      
      int table_id = wparam;
      int record_id = (int)(intptr_t)lparam;
      bool success = false;
      
      switch (table_id) {
        case ID_DB_AUTHORS:
          success = author_delete(ctx, record_id);
          break;
        case ID_DB_POSTS:
          success = post_delete(ctx, record_id);  // cascades to comments
          break;
        case ID_DB_COMMENTS:
          success = comment_delete(ctx, record_id);
          break;
      }
      
      if (success) db->dirty = true;
      return success ? 1 : 0;
    }
    
    case dbFetch: {
      if (!ctx) return (lresult_t)NULL;
      
      int table_id = LOWORD(wparam);
      int filter_field = HIWORD(wparam);
      int filter_value = (int)(intptr_t)lparam;
      
      switch (table_id) {
        case ID_DB_AUTHORS: {
          // Build linked list of all authors
          result_node_t *head = NULL, *tail = NULL;
          for (int i = 0; i < ctx->author_count; i++) {
            result_node_t *node = malloc(sizeof(result_node_t) + sizeof(db_author_t *));
            node->next = NULL;
            *(db_author_t **)node->data = &ctx->authors[i];
            if (tail) tail->next = node;
            else head = node;
            tail = node;
          }
          return (lresult_t)head;
        }
        
        case ID_DB_POSTS: {
          if (filter_field == 0) {
            // Fetch all posts
            result_node_t *head = NULL, *tail = NULL;
            for (int i = 0; i < ctx->post_count; i++) {
              result_node_t *node = malloc(sizeof(result_node_t) + sizeof(db_post_t *));
              node->next = NULL;
              *(db_post_t **)node->data = &ctx->posts[i];
              if (tail) tail->next = node;
              else head = node;
              tail = node;
            }
            return (lresult_t)head;
          }
          break;
        }
        
        case ID_DB_COMMENTS: {
          if (filter_field == ID_DB_COMMENTS_POST_ID) {
            result_node_t *head = NULL, *tail = NULL;
            for (int i = 0; i < ctx->comment_count; i++) {
              if (ctx->comments[i].post_id == filter_value) {
                result_node_t *node = malloc(sizeof(result_node_t) + sizeof(db_comment_t *));
                node->next = NULL;
                *(db_comment_t **)node->data = &ctx->comments[i];
                if (tail) tail->next = node;
                else head = node;
                tail = node;
              }
            }
            return (lresult_t)head;
          } else if (filter_field == 0) {
            // Fetch all comments
            result_node_t *head = NULL, *tail = NULL;
            for (int i = 0; i < ctx->comment_count; i++) {
              result_node_t *node = malloc(sizeof(result_node_t) + sizeof(db_comment_t *));
              node->next = NULL;
              *(db_comment_t **)node->data = &ctx->comments[i];
              if (tail) tail->next = node;
              else head = node;
              tail = node;
            }
            return (lresult_t)head;
          }
          break;
        }
      }
      return (lresult_t)NULL;
    }
    
    case dbFind: {
      if (!ctx) return (lresult_t)NULL;
      
      int table_id = LOWORD(wparam);
      int search_field = HIWORD(wparam);
      uintptr_t search_value = (uintptr_t)lparam;
      
      switch (table_id) {
        case ID_DB_AUTHORS: {
          /* Preserve legacy ordinal lookups (0=id, 1=name) alongside the
           * generated field IDs (id=1, name=2). A legacy ordinal 1 collides
           * with ID_DB_AUTHORS_ID, so treat large pointer-like payloads as
           * string name lookups and small integer payloads as id lookups. */
          if (search_field == 0 ||
              (search_field == ID_DB_AUTHORS_ID && search_value <= INT32_MAX)) {
            // Find by id
            int id = (int)(intptr_t)lparam;
            db_author_t *found = author_find_by_id(ctx, id);
            return (lresult_t)found;
          } else if (search_field == 1 || search_field == ID_DB_AUTHORS_NAME ||
                     (search_field == ID_DB_AUTHORS_ID && search_value > INT32_MAX)) {
            // Find by name
            const char *name = (const char *)lparam;
            db_author_t *found = author_find_by_name(ctx, name);
            return (lresult_t)found;
          }
          break;
        }
        
        case ID_DB_POSTS: {
          if (search_field == 0 || search_field == ID_DB_POSTS_ID) {
            // Find by id
            int id = (int)(intptr_t)lparam;
            db_post_t *found = post_find_by_id(ctx, id);
            return (lresult_t)found;
          }
          break;
        }
        
        case ID_DB_COMMENTS: {
          if (search_field == 0 || search_field == ID_DB_COMMENTS_ID) {
            // Find by id
            int id = (int)(intptr_t)lparam;
            db_comment_t *found = comment_find_by_id(ctx, id);
            return (lresult_t)found;
          }
          break;
        }
      }
      
      return (lresult_t)NULL;
    }
    
    case dbGetDirty:
      return db->dirty ? 1 : 0;

    case dbGetSchema:
      socialfeed_database_schema.name = db->name;
      socialfeed_database_schema.class_name = db->class_name;
      socialfeed_database_schema.source_path = db->source_path;
      return (lresult_t)&socialfeed_database_schema;

    case dbGetFieldMeta: {
      int *count_out = (int *)lparam;
      switch (wparam) {
        case ID_DB_AUTHORS:
          if (count_out) *count_out = ARRAY_LEN(authors_fields);
          return (lresult_t)authors_fields;
        case ID_DB_POSTS:
          if (count_out) *count_out = ARRAY_LEN(posts_fields);
          return (lresult_t)posts_fields;
        case ID_DB_COMMENTS:
          if (count_out) *count_out = ARRAY_LEN(comments_fields);
          return (lresult_t)comments_fields;
        default:
          if (count_out) *count_out = 0;
          return (lresult_t)NULL;
      }
    }

    case dbGetApi:
      return (lresult_t)&db_simple_xml_api;
  }
  
  return 0;
}
