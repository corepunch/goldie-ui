#ifndef __DB_SIMPLE_XML_H__
#define __DB_SIMPLE_XML_H__

#include "../../user/database.h"

// Table identifiers
enum {
  TABLE_AUTHORS = 0,
  TABLE_POSTS,
  TABLE_COMMENTS
};

// ═══════════════════════════════════════════════════════════════════════════
// Schema Definitions (match socialfeed.orion <table> declarations)
// ═══════════════════════════════════════════════════════════════════════════

// authors table
typedef struct author_s {
  int id;
  char name[64];
  char avatar[256];
} author_t;

// posts table
typedef struct post_s {
  int id;
  int author_id;
  char title[256];
  char body[2048];
  int like_count;
  int comment_count;
} post_t;

// comments table
typedef struct comment_s {
  int id;
  int post_id;
  int author_id;
  char text[1024];
  int like_count;
} comment_t;

// ═══════════════════════════════════════════════════════════════════════════
// Database procedure (analogous to winproc_t pattern)
// ═══════════════════════════════════════════════════════════════════════════
//
// Messages supported (Zero Wrapper Structs API):
//   dbCreate - allocate userdata, parse lparam source path
//   dbLoad   - load from XML file
//   dbSave   - save to XML file (only if dirty)
//   dbInsert - wparam=TABLE_*; lparam=record_data → returns (lresult_t)record_ptr
//   dbUpdate - wparam=TABLE_*; lparam=record_ptr → returns 1 on success
//   dbDelete - wparam=TABLE_*; lparam=(void*)(intptr_t)record_id → returns 1 on success
//   dbFetch  - wparam=MAKEDWORD(TABLE_*,filter_field); lparam=(intptr_t)filter_value → returns (lresult_t)result_node_t*
//   dbFind   - wparam=MAKEDWORD(TABLE_*,search_field); lparam=(intptr_t)value or (void*)str → returns (lresult_t)record_ptr
//
// Example usage:
//   // Insert
//   author_t author = { .name = "alice" };
//   author_t *inserted = (author_t *)send_db_message(db, dbInsert, TABLE_AUTHORS, &author);
//
//   // Fetch all posts
//   result_node_t *posts = (result_node_t *)send_db_message(db, dbFetch,
//     MAKEDWORD(TABLE_POSTS, 0), (void *)(intptr_t)0);
//   int count = count_result_list(posts);
//   free_result_list(posts);
//
lresult_t db_simple_xml(database_t *db, uint32_t msg, uint32_t wparam, void *lparam);

#endif // __DB_SIMPLE_XML_H__
