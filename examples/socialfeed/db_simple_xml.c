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

static void *array_append_copy(void **rows, int *count, int *capacity, size_t row_size, const void *src, int initial_capacity) {
  array_ensure_capacity(rows, capacity, *count, row_size, initial_capacity);
  char *row = (char *)(*rows) + ((size_t)(*count) * row_size);
  *count = *count + 1;
  memcpy(row, src, row_size);
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

static void load_table_rows(xmlNode *table, const char *row_tag, void **rows, int *count, int *capacity, size_t row_size, size_t id_offset, int *next_id, const db_field_meta_t *fields, int field_count, int initial_capacity) {
  void *record = calloc(1, row_size);
  if (!record)
    return;
  for (xmlNode *row = table->children; row; row = row->next) {
    if (row->type != XML_ELEMENT_NODE) continue;
    if (xmlStrcmp(row->name, (const xmlChar *)row_tag) != 0) continue;
    if (!db_load_record_from_xml(row, record, fields, field_count))
      continue;
    array_append_copy(rows, count, capacity, row_size, record, initial_capacity);
    int id = *(int *)((char *)record + id_offset);
    if (id >= *next_id)
      *next_id = id + 1;
  }
  free(record);
}

static void save_table_rows(xmlNodePtr root, const char *table_tag, const char *row_tag, void *rows, int count, size_t row_size, const db_field_meta_t *fields, int field_count) {
  xmlNodePtr table = xmlNewChild(root, NULL, (const xmlChar *)table_tag, NULL);
  for (int i = 0; i < count; i++) {
    db_save_record_to_xml(table, row_tag, (char *)rows + ((size_t)i * row_size), fields, field_count);
  }
}

static lresult_t fetch_rows(void *rows, int count, size_t row_size, bool use_filter, size_t filter_offset, int filter_value) {
  result_node_t *head = NULL, *tail = NULL;
  for (int i = 0; i < count; i++) {
    char *row = (char *)rows + ((size_t)i * row_size);
    if (use_filter && (*(int *)(row + filter_offset) != filter_value))
      continue;
    result_node_t *node = malloc(sizeof(result_node_t) + sizeof(void *));
    node->next = NULL;
    *(void **)node->data = row;
    if (tail) tail->next = node;
    else head = node;
    tail = node;
  }
  return (lresult_t)head;
}

static void *table_find_record(simple_xml_context_t *ctx, int table_id, int search_field, uintptr_t search_value) {
  switch (table_id) {
    case ID_DB_AUTHORS:
      if (search_field == 0 || (search_field == ID_DB_AUTHORS_ID && search_value <= INT32_MAX))
        return array_find_by_id(ctx->authors, ctx->author_count, sizeof(db_author_t), offsetof(db_author_t, id), (int)(intptr_t)search_value);
      if (search_field == 1 || search_field == ID_DB_AUTHORS_NAME || (search_field == ID_DB_AUTHORS_ID && search_value > INT32_MAX))
        return array_find_by_string(ctx->authors, ctx->author_count, sizeof(db_author_t), offsetof(db_author_t, name), (const char *)search_value);
      break;
    case ID_DB_POSTS:
      if (search_field == 0 || search_field == ID_DB_POSTS_ID)
        return array_find_by_id(ctx->posts, ctx->post_count, sizeof(db_post_t), offsetof(db_post_t, id), (int)(intptr_t)search_value);
      break;
    case ID_DB_COMMENTS:
      if (search_field == 0 || search_field == ID_DB_COMMENTS_ID)
        return array_find_by_id(ctx->comments, ctx->comment_count, sizeof(db_comment_t), offsetof(db_comment_t, id), (int)(intptr_t)search_value);
      break;
  }
  return NULL;
}

static bool table_delete_record(simple_xml_context_t *ctx, int table_id, int record_id) {
  switch (table_id) {
    case ID_DB_AUTHORS:
      return array_delete_by_id(ctx->authors, &ctx->author_count, sizeof(db_author_t), offsetof(db_author_t, id), record_id);
    case ID_DB_POSTS:
      for (int i = ctx->comment_count - 1; i >= 0; i--) {
        if (ctx->comments[i].post_id == record_id) {
          memmove(&ctx->comments[i], &ctx->comments[i + 1], (size_t)(ctx->comment_count - i - 1) * sizeof(db_comment_t));
          ctx->comment_count--;
        }
      }
      return array_delete_by_id(ctx->posts, &ctx->post_count, sizeof(db_post_t), offsetof(db_post_t, id), record_id);
    case ID_DB_COMMENTS: {
      db_comment_t *comment = array_find_by_id(ctx->comments, ctx->comment_count, sizeof(db_comment_t), offsetof(db_comment_t, id), record_id);
      if (!comment)
        return false;
      int post_id = comment->post_id;
      if (!array_delete_by_id(ctx->comments, &ctx->comment_count, sizeof(db_comment_t), offsetof(db_comment_t, id), record_id))
        return false;
      db_post_t *post = array_find_by_id(ctx->posts, ctx->post_count, sizeof(db_post_t), offsetof(db_post_t, id), post_id);
      if (post && post->comment_count > 0)
        post->comment_count--;
      return true;
    }
  }
  return false;
}

static void *table_insert_record(simple_xml_context_t *ctx, int table_id, const void *record_data) {
  switch (table_id) {
    case ID_DB_AUTHORS: {
      db_author_t *rec = array_append_with_auto_id((void **)&ctx->authors, &ctx->author_count, &ctx->author_capacity,
                                                  sizeof(db_author_t), offsetof(db_author_t, id), &ctx->next_author_id, 16);
      memcpy(rec, record_data, sizeof(db_author_t));
      rec->id = ctx->next_author_id - 1;
      return rec;
    }
    case ID_DB_POSTS: {
      db_post_t *rec = array_append_with_auto_id((void **)&ctx->posts, &ctx->post_count, &ctx->post_capacity,
                                                 sizeof(db_post_t), offsetof(db_post_t, id), &ctx->next_post_id, 32);
      memcpy(rec, record_data, sizeof(db_post_t));
      rec->id = ctx->next_post_id - 1;
      rec->like_count = 0;
      rec->comment_count = 0;
      return rec;
    }
    case ID_DB_COMMENTS: {
      db_comment_t *data = (db_comment_t *)record_data;
      db_comment_t *rec = array_append_with_auto_id((void **)&ctx->comments, &ctx->comment_count, &ctx->comment_capacity,
                                                    sizeof(db_comment_t), offsetof(db_comment_t, id), &ctx->next_comment_id, 64);
      memcpy(rec, record_data, sizeof(db_comment_t));
      rec->id = ctx->next_comment_id - 1;
      rec->like_count = 0;
      db_post_t *post = array_find_by_id(ctx->posts, ctx->post_count, sizeof(db_post_t), offsetof(db_post_t, id), data->post_id);
      if (post)
        post->comment_count++;
      return rec;
    }
  }
  return NULL;
}

static lresult_t table_fetch_records(simple_xml_context_t *ctx, int table_id, int filter_field, int filter_value) {
  switch (table_id) {
    case ID_DB_AUTHORS:
      return fetch_rows(ctx->authors, ctx->author_count, sizeof(db_author_t), false, 0, 0);
    case ID_DB_POSTS:
      return filter_field == 0 ? fetch_rows(ctx->posts, ctx->post_count, sizeof(db_post_t), false, 0, 0) : (lresult_t)NULL;
    case ID_DB_COMMENTS:
      if (filter_field == ID_DB_COMMENTS_POST_ID)
        return fetch_rows(ctx->comments, ctx->comment_count, sizeof(db_comment_t), true, offsetof(db_comment_t, post_id), filter_value);
      if (filter_field == 0)
        return fetch_rows(ctx->comments, ctx->comment_count, sizeof(db_comment_t), false, 0, 0);
      break;
  }
  return (lresult_t)NULL;
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
          load_table_rows(table, "author",
                          (void **)&ctx->authors, &ctx->author_count, &ctx->author_capacity,
                          sizeof(db_author_t), offsetof(db_author_t, id), &ctx->next_author_id,
                          STATIC_ARRAY(authors_fields), 16);
        }
        else if (xmlStrcmp(table->name, (const xmlChar *)"posts") == 0) {
          load_table_rows(table, "post",
                          (void **)&ctx->posts, &ctx->post_count, &ctx->post_capacity,
                          sizeof(db_post_t), offsetof(db_post_t, id), &ctx->next_post_id,
                          STATIC_ARRAY(posts_fields), 32);
        }
        else if (xmlStrcmp(table->name, (const xmlChar *)"comments") == 0) {
          load_table_rows(table, "comment",
                          (void **)&ctx->comments, &ctx->comment_count, &ctx->comment_capacity,
                          sizeof(db_comment_t), offsetof(db_comment_t, id), &ctx->next_comment_id,
                          STATIC_ARRAY(comments_fields), 64);
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

      save_table_rows(root, "authors", "author", ctx->authors, ctx->author_count,
                      sizeof(db_author_t), STATIC_ARRAY(authors_fields));
      save_table_rows(root, "posts", "post", ctx->posts, ctx->post_count,
                      sizeof(db_post_t), STATIC_ARRAY(posts_fields));
      save_table_rows(root, "comments", "comment", ctx->comments, ctx->comment_count,
                      sizeof(db_comment_t), STATIC_ARRAY(comments_fields));
      
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
      
      void *rec = table_insert_record(ctx, wparam, lparam);
      if (rec)
        db->dirty = true;
      return (lresult_t)rec;
    }
    
    case dbUpdate: {
      if (!ctx) return 0;
      
      // Record is already updated in-place, just mark dirty
      db->dirty = true;
      return 1;
    }
    
    case dbDelete: {
      if (!ctx) return 0;
      
      bool success = table_delete_record(ctx, wparam, (int)(intptr_t)lparam);
      if (success)
        db->dirty = true;
      return success ? 1 : 0;
    }
    
    case dbFetch: {
      if (!ctx) return (lresult_t)NULL;
      
      return table_fetch_records(ctx, LOWORD(wparam), HIWORD(wparam), (int)(intptr_t)lparam);
    }
    
    case dbFind: {
      if (!ctx) return (lresult_t)NULL;
      
      return (lresult_t)table_find_record(ctx, LOWORD(wparam), HIWORD(wparam), (uintptr_t)lparam);
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
