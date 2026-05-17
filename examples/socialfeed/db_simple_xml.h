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
// Messages supported:
//   dbCreate - allocate userdata, parse lparam source path
//   dbLoad   - load from XML file
//   dbSave   - save to XML file (only if dirty)
//   dbInsert - wparam=TABLE_AUTHORS/POSTS/COMMENTS; lparam=record_data
//   dbUpdate - wparam=TABLE_*; lparam=record_ptr
//   dbDelete - wparam=TABLE_*; lparam=(void*)(intptr_t)record_id
//   dbFetch  - wparam=TABLE_*; lparam=fetch_params_t*
//   dbFind   - wparam=TABLE_*; lparam=find_params_t*
//
lresult_t db_simple_xml(database_t *db, uint32_t msg, uint32_t wparam, void *lparam);

#endif // __DB_SIMPLE_XML_H__
