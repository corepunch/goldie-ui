// SimpleXMLDatabase implementation (message-based proc pattern)
#include "db_simple_xml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// Internal Context (stored in database_t->userdata)
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  // In-memory tables (dynamic arrays)
  author_t *authors;
  int author_count;
  int author_capacity;
  
  post_t *posts;
  int post_count;
  int post_capacity;
  
  comment_t *comments;
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
    ctx->authors = realloc(ctx->authors, ctx->author_capacity * sizeof(author_t));
  }
}

static void ensure_capacity_posts(simple_xml_context_t *ctx) {
  if (ctx->post_count >= ctx->post_capacity) {
    ctx->post_capacity = (ctx->post_capacity == 0) ? 32 : ctx->post_capacity * 2;
    ctx->posts = realloc(ctx->posts, ctx->post_capacity * sizeof(post_t));
  }
}

static void ensure_capacity_comments(simple_xml_context_t *ctx) {
  if (ctx->comment_count >= ctx->comment_capacity) {
    ctx->comment_capacity = (ctx->comment_capacity == 0) ? 64 : ctx->comment_capacity * 2;
    ctx->comments = realloc(ctx->comments, ctx->comment_capacity * sizeof(comment_t));
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// CRUD Helpers - Authors
// ═══════════════════════════════════════════════════════════════════════════

static author_t *author_insert(simple_xml_context_t *ctx, const char *name, const char *avatar) {
  ensure_capacity_authors(ctx);
  
  author_t *author = &ctx->authors[ctx->author_count++];
  author->id = ctx->next_author_id++;
  strncpy(author->name, name, sizeof(author->name) - 1);
  strncpy(author->avatar, avatar ? avatar : "", sizeof(author->avatar) - 1);
  
  return author;
}

static author_t *author_find_by_id(simple_xml_context_t *ctx, int id) {
  for (int i = 0; i < ctx->author_count; i++) {
    if (ctx->authors[i].id == id)
      return &ctx->authors[i];
  }
  return NULL;
}

static author_t *author_find_by_name(simple_xml_context_t *ctx, const char *name) {
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
              (ctx->author_count - i - 1) * sizeof(author_t));
      ctx->author_count--;
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// CRUD Helpers - Posts
// ═══════════════════════════════════════════════════════════════════════════

static post_t *post_insert(simple_xml_context_t *ctx, int author_id, 
                           const char *title, const char *body) {
  ensure_capacity_posts(ctx);
  
  post_t *post = &ctx->posts[ctx->post_count++];
  post->id = ctx->next_post_id++;
  post->author_id = author_id;
  strncpy(post->title, title, sizeof(post->title) - 1);
  strncpy(post->body, body, sizeof(post->body) - 1);
  post->like_count = 0;
  post->comment_count = 0;
  
  return post;
}

static post_t *post_find_by_id(simple_xml_context_t *ctx, int id) {
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
              (ctx->comment_count - i - 1) * sizeof(comment_t));
      ctx->comment_count--;
    }
  }
  
  // Then delete the post
  for (int i = 0; i < ctx->post_count; i++) {
    if (ctx->posts[i].id == id) {
      memmove(&ctx->posts[i], &ctx->posts[i + 1],
              (ctx->post_count - i - 1) * sizeof(post_t));
      ctx->post_count--;
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// CRUD Helpers - Comments
// ═══════════════════════════════════════════════════════════════════════════

static comment_t *comment_insert(simple_xml_context_t *ctx, int post_id, 
                                 int author_id, const char *text) {
  ensure_capacity_comments(ctx);
  
  comment_t *comment = &ctx->comments[ctx->comment_count++];
  comment->id = ctx->next_comment_id++;
  comment->post_id = post_id;
  comment->author_id = author_id;
  strncpy(comment->text, text, sizeof(comment->text) - 1);
  comment->like_count = 0;
  
  // Update post comment count
  post_t *post = post_find_by_id(ctx, post_id);
  if (post) post->comment_count++;
  
  return comment;
}

static comment_t *comment_find_by_id(simple_xml_context_t *ctx, int id) {
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
              (ctx->comment_count - i - 1) * sizeof(comment_t));
      ctx->comment_count--;
      
      // Update post comment count
      post_t *post = post_find_by_id(ctx, post_id);
      if (post && post->comment_count > 0) post->comment_count--;
      
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Database Procedure (analogous to winproc_t)
// ═══════════════════════════════════════════════════════════════════════════

result_t db_simple_xml(database_t *db, uint32_t msg, uint32_t wparam, void *lparam) {
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
      
      // TODO: Parse XML file from db->source_path
      printf("db_simple_xml: Loading from %s\n", db->source_path);
      
      // Seed with example data (normally parsed from XML)
      author_insert(ctx, "alice", "avatar_alice.png");
      author_insert(ctx, "bob", "avatar_bob.png");
      author_insert(ctx, "carol", "avatar_carol.png");
      author_insert(ctx, "dave", "avatar_dave.png");
      author_insert(ctx, "eve", "avatar_eve.png");
      author_insert(ctx, "frank", "avatar_frank.png");
      
      db->dirty = false;
      return 1;
    }
    
    case dbSave: {
      if (!ctx || !db->dirty) return 0;
      
      // TODO: Serialize to XML file
      printf("db_simple_xml: Saving to %s\n", db->source_path);
      printf("  - %d authors\n", ctx->author_count);
      printf("  - %d posts\n", ctx->post_count);
      printf("  - %d comments\n", ctx->comment_count);
      
      db->dirty = false;
      return 1;
    }
    
    case dbInsert: {
      if (!ctx) return 0;
      
      insert_params_t *params = (insert_params_t *)lparam;
      if (!params) return 0;
      
      switch (params->table_id) {
        case TABLE_AUTHORS: {
          author_t *data = (author_t *)params->record_data;
          author_t *rec = author_insert(ctx, data->name, data->avatar);
          if (params->out_record) *params->out_record = rec;
          db->dirty = true;
          return 1;
        }
        
        case TABLE_POSTS: {
          post_t *data = (post_t *)params->record_data;
          post_t *rec = post_insert(ctx, data->author_id, data->title, data->body);
          if (params->out_record) *params->out_record = rec;
          db->dirty = true;
          return 1;
        }
        
        case TABLE_COMMENTS: {
          comment_t *data = (comment_t *)params->record_data;
          comment_t *rec = comment_insert(ctx, data->post_id, data->author_id, data->text);
          if (params->out_record) *params->out_record = rec;
          db->dirty = true;
          return 1;
        }
      }
      return 0;
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
      if (!ctx) return 0;
      
      fetch_params_t *params = (fetch_params_t *)lparam;
      if (!params || !params->results_out || !params->count_out) return 0;
      
      switch (params->table_id) {
        case TABLE_AUTHORS: {
          int count = ctx->author_count;
          void **results = malloc(count * sizeof(void *));
          for (int i = 0; i < count; i++) {
            results[i] = &ctx->authors[i];
          }
          *params->results_out = results;  // Write to the pointer variable
          *params->count_out = count;
          return count;
        }
        
        case TABLE_POSTS: {
          if (params->filter_field == 0) {
            // Fetch all
            int count = ctx->post_count;
            void **results = malloc(count * sizeof(void *));
            for (int i = 0; i < count; i++) {
              results[i] = &ctx->posts[i];
            }
            *params->results_out = results;
            *params->count_out = count;
            return count;
          }
          break;
        }
        
        case TABLE_COMMENTS: {
          if (params->filter_field == 2) {  // filter by post_id
            int count = 0;
            for (int i = 0; i < ctx->comment_count; i++) {
              if (ctx->comments[i].post_id == params->filter_value)
                count++;
            }
            
            void **results = malloc(count * sizeof(void *));
            int idx = 0;
            for (int i = 0; i < ctx->comment_count; i++) {
              if (ctx->comments[i].post_id == params->filter_value)
                results[idx++] = &ctx->comments[i];
            }
            *params->results_out = results;
            *params->count_out = count;
            return count;
          } else if (params->filter_field == 0) {
            // Fetch all
            int count = ctx->comment_count;
            void **results = malloc(count * sizeof(void *));
            for (int i = 0; i < count; i++) {
              results[i] = &ctx->comments[i];
            }
            *params->results_out = results;
            *params->count_out = count;
            return count;
          }
          break;
        }
      }
      return 0;
    }
    
    case dbFind: {
      if (!ctx) return 0;
      
      find_params_t *params = (find_params_t *)lparam;
      if (!params || !params->result_out) return 0;
      
      void *result = NULL;
      
      switch (params->table_id) {
        case TABLE_AUTHORS: {
          if (params->search_field == 0) {  // by id
            result = author_find_by_id(ctx, params->search_value.int_value);
          } else if (params->search_field == 1) {  // by name
            result = author_find_by_name(ctx, params->search_value.str_value);
          }
          break;
        }
        
        case TABLE_POSTS: {
          if (params->search_field == 0) {  // by id
            result = post_find_by_id(ctx, params->search_value.int_value);
          }
          break;
        }
        
        case TABLE_COMMENTS: {
          if (params->search_field == 0) {  // by id
            result = comment_find_by_id(ctx, params->search_value.int_value);
          }
          break;
        }
      }
      
      *params->result_out = result;
      return result ? 1 : 0;
    }
    
    case dbGetDirty:
      return db->dirty ? 1 : 0;
  }
  
  return 0;
}
