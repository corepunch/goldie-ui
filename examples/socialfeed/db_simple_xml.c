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

// Global context pointer for object procs to access (set during database operations)
static simple_xml_context_t *g_db_ctx = NULL;

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

// ═══════════════════════════════════════════════════════════════════════════
// CRUD Helpers - Authors
// ═══════════════════════════════════════════════════════════════════════════

static db_author_t *author_insert(simple_xml_context_t *ctx, const char *name, const char *avatar) {
  ensure_capacity_authors(ctx);
  
  db_author_t *author = &ctx->authors[ctx->author_count++];
  author->id = ctx->next_author_id++;
  strncpy(author->name, name, sizeof(author->name) - 1);
  strncpy(author->avatar, avatar ? avatar : "", sizeof(author->avatar) - 1);
  
  return author;
}

static db_author_t *author_find_by_id(simple_xml_context_t *ctx, int id) {
  for (int i = 0; i < ctx->author_count; i++) {
    if (ctx->authors[i].id == id)
      return &ctx->authors[i];
  }
  return NULL;
}

static db_author_t *author_find_by_name(simple_xml_context_t *ctx, const char *name) {
  for (int i = 0; i < ctx->author_count; i++) {
    if (strcmp(ctx->authors[i].name, name) == 0)
      return &ctx->authors[i];
  }
  return NULL;
}

static bool author_delete(simple_xml_context_t *ctx, int id) {
  for (int i = 0; i < ctx->author_count; i++) {
    if (ctx->authors[i].id == id) {
      memmove(&ctx->authors[i], &ctx->authors[i + 1], 
              (ctx->author_count - i - 1) * sizeof(db_author_t));
      ctx->author_count--;
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// CRUD Helpers - Posts
// ═══════════════════════════════════════════════════════════════════════════

static db_post_t *post_insert(simple_xml_context_t *ctx, int author_id, 
                           const char *title, const char *body) {
  ensure_capacity_posts(ctx);
  
  db_post_t *post = &ctx->posts[ctx->post_count++];
  post->id = ctx->next_post_id++;
  post->author_id = author_id;
  strncpy(post->title, title, sizeof(post->title) - 1);
  strncpy(post->body, body, sizeof(post->body) - 1);
  post->like_count = 0;
  post->comment_count = 0;
  
  return post;
}

static db_post_t *post_find_by_id(simple_xml_context_t *ctx, int id) {
  for (int i = 0; i < ctx->post_count; i++) {
    if (ctx->posts[i].id == id)
      return &ctx->posts[i];
  }
  return NULL;
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
  for (int i = 0; i < ctx->post_count; i++) {
    if (ctx->posts[i].id == id) {
      memmove(&ctx->posts[i], &ctx->posts[i + 1],
              (ctx->post_count - i - 1) * sizeof(db_post_t));
      ctx->post_count--;
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// CRUD Helpers - Comments
// ═══════════════════════════════════════════════════════════════════════════

static db_comment_t *comment_insert(simple_xml_context_t *ctx, int post_id, 
                                 int author_id, const char *text) {
  ensure_capacity_comments(ctx);
  
  db_comment_t *comment = &ctx->comments[ctx->comment_count++];
  comment->id = ctx->next_comment_id++;
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
  for (int i = 0; i < ctx->comment_count; i++) {
    if (ctx->comments[i].id == id)
      return &ctx->comments[i];
  }
  return NULL;
}

static bool comment_delete(simple_xml_context_t *ctx, int id) {
  for (int i = 0; i < ctx->comment_count; i++) {
    if (ctx->comments[i].id == id) {
      int post_id = ctx->comments[i].post_id;
      
      memmove(&ctx->comments[i], &ctx->comments[i + 1],
              (ctx->comment_count - i - 1) * sizeof(db_comment_t));
      ctx->comment_count--;
      
      // Update post comment count
      db_post_t *post = post_find_by_id(ctx, post_id);
      if (post && post->comment_count > 0) post->comment_count--;
      
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Object Procedures (DDX-style field extraction)
// ═══════════════════════════════════════════════════════════════════════════

// Column IDs for Action-Message DDX
enum {
  COL_AUTHOR_ID = 0,
  COL_AUTHOR_NAME,
  COL_AUTHOR_AVATAR,
  
  COL_POST_ID = 100,
  COL_POST_AUTHOR_ID,
  COL_POST_TITLE,
  COL_POST_BODY,
  COL_POST_LIKE_COUNT,
  COL_POST_COMMENT_COUNT,
  COL_POST_AUTHOR_NAME,  // Join: author.name
  
  COL_COMMENT_ID = 200,
  COL_COMMENT_POST_ID,
  COL_COMMENT_AUTHOR_ID,
  COL_COMMENT_TEXT,
  COL_COMMENT_LIKE_COUNT,
  COL_COMMENT_AUTHOR_NAME,  // Join: author.name
};

// Object proc for db_author_t records
static result_t author_object_proc(const void *object, uint32_t msg,
                                   uint32_t wparam, void *lparam) {
  const db_author_t *a = (const db_author_t *)object;
  char *buf = (char *)lparam;
  uint16_t column_id = LOWORD(wparam);
  size_t buf_sz = (size_t)HIWORD(wparam);
  
  if (!a || !buf || buf_sz == 0) return false;
  
  switch (msg) {
    case dbObjGetFieldText:
      switch (column_id) {
        case COL_AUTHOR_ID:
          snprintf(buf, buf_sz, "%d", a->id);
          return true;
        case COL_AUTHOR_NAME:
          strncpy(buf, a->name, buf_sz - 1);
          buf[buf_sz - 1] = '\0';
          return true;
        case COL_AUTHOR_AVATAR:
          strncpy(buf, a->avatar, buf_sz - 1);
          buf[buf_sz - 1] = '\0';
          return true;
        default:
          return false;
      }
    default:
      return false;
  }
}

// Object proc for db_post_t records
static result_t post_object_proc(const void *object, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  const db_post_t *p = (const db_post_t *)object;
  char *buf = (char *)lparam;
  uint16_t column_id = LOWORD(wparam);
  size_t buf_sz = (size_t)HIWORD(wparam);
  
  if (!p || !buf || buf_sz == 0) return false;
  
  switch (msg) {
    case dbObjGetFieldText:
      switch (column_id) {
        case COL_POST_ID:
          snprintf(buf, buf_sz, "%d", p->id);
          return true;
        case COL_POST_AUTHOR_ID:
          snprintf(buf, buf_sz, "%d", p->author_id);
          return true;
        case COL_POST_TITLE:
          strncpy(buf, p->title, buf_sz - 1);
          buf[buf_sz - 1] = '\0';
          return true;
        case COL_POST_BODY:
          strncpy(buf, p->body, buf_sz - 1);
          buf[buf_sz - 1] = '\0';
          return true;
        case COL_POST_LIKE_COUNT:
          snprintf(buf, buf_sz, "%d", p->like_count);
          return true;
        case COL_POST_COMMENT_COUNT:
          snprintf(buf, buf_sz, "%d", p->comment_count);
          return true;
        case COL_POST_AUTHOR_NAME: {
          // Join: Look up author by author_id and return name
          if (g_db_ctx) {
            db_author_t *author = author_find_by_id(g_db_ctx, p->author_id);
            if (author) {
              strncpy(buf, author->name, buf_sz - 1);
              buf[buf_sz - 1] = '\0';
              return true;
            }
          }
          snprintf(buf, buf_sz, "Unknown");
          return true;
        }
        default:
          return false;
      }
    default:
      return false;
  }
}

// Object proc for db_comment_t records
static result_t comment_object_proc(const void *object, uint32_t msg,
                                    uint32_t wparam, void *lparam) {
  const db_comment_t *c = (const db_comment_t *)object;
  char *buf = (char *)lparam;
  uint16_t column_id = LOWORD(wparam);
  size_t buf_sz = (size_t)HIWORD(wparam);
  
  if (!c || !buf || buf_sz == 0) return false;
  
  switch (msg) {
    case dbObjGetFieldText:
      switch (column_id) {
        case COL_COMMENT_ID:
          snprintf(buf, buf_sz, "%d", c->id);
          return true;
        case COL_COMMENT_POST_ID:
          snprintf(buf, buf_sz, "%d", c->post_id);
          return true;
        case COL_COMMENT_AUTHOR_ID:
          snprintf(buf, buf_sz, "%d", c->author_id);
          return true;
        case COL_COMMENT_TEXT:
          strncpy(buf, c->text, buf_sz - 1);
          buf[buf_sz - 1] = '\0';
          return true;
        case COL_COMMENT_LIKE_COUNT:
          snprintf(buf, buf_sz, "%d", c->like_count);
          return true;
        case COL_COMMENT_AUTHOR_NAME: {
          // Join: Look up author by author_id and return name
          if (g_db_ctx) {
            db_author_t *author = author_find_by_id(g_db_ctx, c->author_id);
            if (author) {
              strncpy(buf, author->name, buf_sz - 1);
              buf[buf_sz - 1] = '\0';
              return true;
            }
          }
          snprintf(buf, buf_sz, "Unknown");
          return true;
        }
        default:
          return false;
      }
    default:
      return false;
  }
}

// Field bindings for each table
static const db_field_msg_binding_t author_field_bindings[] = {
  { "id", COL_AUTHOR_ID },
  { "name", COL_AUTHOR_NAME },
  { "avatar", COL_AUTHOR_AVATAR },
};

static const db_field_msg_binding_t post_field_bindings[] = {
  { "id", COL_POST_ID },
  { "author_id", COL_POST_AUTHOR_ID },
  { "title", COL_POST_TITLE },
  { "body", COL_POST_BODY },
  { "likes", COL_POST_LIKE_COUNT },
  { "like_count", COL_POST_LIKE_COUNT },  // Alias
  { "comment_count", COL_POST_COMMENT_COUNT },
  { "author", COL_POST_AUTHOR_ID },  // Alias for join queries
  { "author.name", COL_POST_AUTHOR_NAME },  // Join field
};

static const db_field_msg_binding_t comment_field_bindings[] = {
  { "id", COL_COMMENT_ID },
  { "post_id", COL_COMMENT_POST_ID },
  { "author_id", COL_COMMENT_AUTHOR_ID },
  { "text", COL_COMMENT_TEXT },
  { "likes", COL_COMMENT_LIKE_COUNT },
  { "like_count", COL_COMMENT_LIKE_COUNT },  // Alias
  { "author", COL_COMMENT_AUTHOR_ID },  // Alias for join queries
  { "author.name", COL_COMMENT_AUTHOR_NAME },  // Join field
};

// ═══════════════════════════════════════════════════════════════════════════
// Database Procedure (analogous to winproc_t)
// ═══════════════════════════════════════════════════════════════════════════

lresult_t db_simple_xml(database_t *db, uint32_t msg, uint32_t wparam, void *lparam) {
  simple_xml_context_t *ctx = (simple_xml_context_t *)db->userdata;
  g_db_ctx = ctx;  // Set global for object procs
  
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
            if (db_load_record_from_xml(row, &author, authors_fields, 
                                        sizeof(authors_fields)/sizeof(authors_fields[0]))) {
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
            if (db_load_record_from_xml(row, &post, posts_fields,
                                        sizeof(posts_fields)/sizeof(posts_fields[0]))) {
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
            if (db_load_record_from_xml(row, &comment, comments_fields,
                                        sizeof(comments_fields)/sizeof(comments_fields[0]))) {
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
                              authors_fields,
                              sizeof(authors_fields)/sizeof(authors_fields[0]));
      }
      
      // Serialize posts table
      xmlNodePtr posts_table = xmlNewChild(root, NULL, (const xmlChar *)"posts", NULL);
      for (int i = 0; i < ctx->post_count; i++) {
        db_save_record_to_xml(posts_table, "post", &ctx->posts[i],
                              posts_fields,
                              sizeof(posts_fields)/sizeof(posts_fields[0]));
      }
      
      // Serialize comments table
      xmlNodePtr comments_table = xmlNewChild(root, NULL, (const xmlChar *)"comments", NULL);
      for (int i = 0; i < ctx->comment_count; i++) {
        db_save_record_to_xml(comments_table, "comment", &ctx->comments[i],
                              comments_fields,
                              sizeof(comments_fields)/sizeof(comments_fields[0]));
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
        case TABLE_AUTHORS: {
          db_author_t *data = (db_author_t *)record_data;
          db_author_t *rec = author_insert(ctx, data->name, data->avatar);
          db->dirty = true;
          return (lresult_t)rec;
        }
        
        case TABLE_POSTS: {
          db_post_t *data = (db_post_t *)record_data;
          db_post_t *rec = post_insert(ctx, data->author_id, data->title, data->body);
          db->dirty = true;
          return (lresult_t)rec;
        }
        
        case TABLE_COMMENTS: {
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
        case TABLE_AUTHORS:
          success = author_delete(ctx, record_id);
          break;
        case TABLE_POSTS:
          success = post_delete(ctx, record_id);  // cascades to comments
          break;
        case TABLE_COMMENTS:
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
        case TABLE_AUTHORS: {
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
        
        case TABLE_POSTS: {
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
        
        case TABLE_COMMENTS: {
          if (filter_field == 1) {  // filter by post_id (field 1)
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
      
      switch (table_id) {
        case TABLE_AUTHORS: {
          if (search_field == 0) {
            // Find by id
            int id = (int)(intptr_t)lparam;
            db_author_t *found = author_find_by_id(ctx, id);
            return (lresult_t)found;
          } else if (search_field == 1) {
            // Find by name
            const char *name = (const char *)lparam;
            db_author_t *found = author_find_by_name(ctx, name);
            return (lresult_t)found;
          }
          break;
        }
        
        case TABLE_POSTS: {
          if (search_field == 0) {
            // Find by id
            int id = (int)(intptr_t)lparam;
            db_post_t *found = post_find_by_id(ctx, id);
            return (lresult_t)found;
          }
          break;
        }
        
        case TABLE_COMMENTS: {
          if (search_field == 0) {
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
    
    case dbGetObjectProc: {
      // wparam = table_id; returns object proc for that table
      int table_id = (int)wparam;
      switch (table_id) {
        case TABLE_AUTHORS:  return (lresult_t)author_object_proc;
        case TABLE_POSTS:    return (lresult_t)post_object_proc;
        case TABLE_COMMENTS: return (lresult_t)comment_object_proc;
        default: return (lresult_t)NULL;
      }
    }
    
    case dbGetFieldBindings: {
      // wparam = table_id; lparam = int* count_out; returns db_field_msg_binding_t*
      int table_id = (int)wparam;
      int *count_out = (int *)lparam;
      
      switch (table_id) {
        case TABLE_AUTHORS:
          if (count_out) *count_out = sizeof(author_field_bindings) / sizeof(author_field_bindings[0]);
          return (lresult_t)author_field_bindings;
        case TABLE_POSTS:
          if (count_out) *count_out = sizeof(post_field_bindings) / sizeof(post_field_bindings[0]);
          return (lresult_t)post_field_bindings;
        case TABLE_COMMENTS:
          if (count_out) *count_out = sizeof(comment_field_bindings) / sizeof(comment_field_bindings[0]);
          return (lresult_t)comment_field_bindings;
        default:
          if (count_out) *count_out = 0;
          return (lresult_t)NULL;
      }
    }
  }
  
  return 0;
}
