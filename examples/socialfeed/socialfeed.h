#ifndef __SOCIALFEED_H__
#define __SOCIALFEED_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "../../ui.h"
#include "../../commctl/columnview.h"
#include "../../commctl/menubar.h"
#include "../../user/accel.h"
#include "../../user/icons.h"

// All definitions generated from socialfeed.orion:
//   - Menu IDs, toolbar IDs, form definitions
//   - Database schema (table enums, structs, field bindings)
#include "build/generated/examples/socialfeed/socialfeed.h"

// ============================================================
// Database Implementation (db_simple_xml.c)
// ============================================================
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

#ifndef SOCIALFEED_DEBUG
#define SOCIALFEED_DEBUG 1
#endif

#if SOCIALFEED_DEBUG
#define SF_DEBUG(...) do { axLog("[socialfeed] " __VA_ARGS__); } while (0)
#else
#define SF_DEBUG(...) ((void)0)
#endif

// ============================================================
// Layout constants
// ============================================================

#define SCREEN_W  640
#define SCREEN_H  480

#define FEED_AUTHOR_W    80
#define FEED_LIKES_W     50
#define FEED_COMMENTS_W  55

// Post detail dialog dimensions (client area)
#define POST_DLG_W  520
#define POST_DLG_H  336

// ============================================================
// Data capacity constants
// ============================================================

#define POSTS_INIT_CAP    16
#define COMMENTS_INIT_CAP  8
#define REPLIES_INIT_CAP   4

// ============================================================
// Data model
// ============================================================

typedef struct comment_s {
  int                id;
  char              *author;
  char              *text;
  int                like_count;
  uint32_t           created_at;
  struct comment_s **replies;
  int                reply_count;
  int                reply_cap;
} comment_t;

typedef struct {
  int        id;
  char      *author;
  char      *title;
  char      *body;
  int        like_count;
  uint32_t   created_at;
  comment_t **comments;
  int        comment_count;
  int        comment_cap;
} post_t;

// ============================================================
// Application state
// ============================================================

typedef struct {
  database_t  *db;           // Database for automatic view population
  post_t     **posts;
  int          post_count;
  int          post_cap;
  int          next_id;          // next post ID (Appwrite document ID)
  int          next_comment_id;  // next comment / reply ID
  int          selected_idx;
  window_t    *menubar_win;
  window_t    *main_win;
  window_t    *content_win;
  window_t    *feed_win;
  hinstance_t  hinstance;
  accel_table_t *accel;
} app_state_t;

extern app_state_t *g_app;

// ============================================================
// Model functions (model_feed.c)
// ============================================================

char      *sf_strdup(const char *s);

comment_t *comment_create(const char *author, const char *text);
void       comment_free(comment_t *c);
bool       comment_add_reply(comment_t *c, comment_t *reply);
void       comment_like(comment_t *c);

post_t    *post_create(const char *author, const char *title, const char *body);
void       post_free(post_t *p);
bool       post_add_comment(post_t *p, comment_t *c);
void       post_like(post_t *p);
bool       socialfeed_post_field_text(const post_t *p, const char *field,
                                      char *buf, size_t buf_sz);
bool       socialfeed_comment_field_text(const comment_t *c, const char *field,
                                         char *buf, size_t buf_sz);
bool       socialfeed_comment_has_field(const char *field);

// ============================================================
// Controller functions (controller_app.c)
// ============================================================

app_state_t *app_init(void);
void         app_shutdown(app_state_t *app);
bool         app_add_post(post_t *post);
bool         app_delete_post(int index);
post_t      *app_get_post(int index);
void         app_update_status(void);

// Append a comment to a post, assigning it a unique document ID.
// Mirrors app_add_post — callers must use this instead of post_add_comment()
// directly so that all comments are assigned monotonically increasing IDs.
bool         app_add_comment(post_t *post, comment_t *c);

// Append a reply to a comment, assigning it a unique document ID.
bool         app_add_reply(comment_t *parent, comment_t *reply);

// ============================================================
// View — menu bar (view_menubar.c)
// ============================================================

void     handle_menu_command(uint16_t id);
result_t app_menubar_proc(window_t *win, uint32_t msg,
                          uint32_t wparam, void *lparam);
void     create_menubar(void);

// ============================================================
// View — main window (view_main.c)
// ============================================================

result_t main_win_proc(window_t *win, uint32_t msg,
                       uint32_t wparam, void *lparam);
void     feed_refresh(void);
void     create_main_window(void);

// ============================================================
// View — post detail dialog (view_dlg_post.c)
// ============================================================

void show_post_detail(window_t *parent, int post_idx);

// ============================================================
// View — new post / comment dialogs (view_dlg_forms.c)
// ============================================================

bool show_new_post_dialog(window_t *parent);
bool show_new_comment_dialog(window_t *parent, const char *prompt_title,
                             char *author_buf, size_t author_sz,
                             char *text_buf,   size_t text_sz);

// ============================================================
// Seed data loading
// ============================================================

bool socialfeed_load_seed_data(const char *path);

#endif // __SOCIALFEED_H__
