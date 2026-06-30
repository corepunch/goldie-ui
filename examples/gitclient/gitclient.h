#ifndef __GITCLIENT_H__
#define __GITCLIENT_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "../../ui.h"
#include "../../commctl/columnview.h"
#include "../../commctl/menubar.h"
#include "../../user/accel.h"
#include "../../user/icons.h"

// Generated from gitclient.orion — IDs, structs, field metadata, forms
#include "build/generated/examples/gitclient/gitclient.h"

// ============================================================
// Debug logging
// ============================================================

#ifndef GITCLIENT_DEBUG
#define GITCLIENT_DEBUG 1
#endif

#if GITCLIENT_DEBUG
#define GC_LOG(...) do { axLog("[gitclient] " __VA_ARGS__); } while (0)
#else
#define GC_LOG(...) ((void)0)
#endif

// ============================================================
// Layout constants
// ============================================================

#define SCREEN_W         800
#define SCREEN_H         480

#define PANEL_SPLITTER   4
#define PANEL_LEFT_W_DEFAULT   180
#define PANEL_RIGHT_W_DEFAULT  260
#define PANEL_VSPLIT_FRAC      60

// ============================================================
// Custom event messages
// ============================================================

#define evGitOpDone     (evUser + 500)
#define evOpenRepo      (evUser + 501)

// ============================================================
// Git data types (kept for git_backend.c compatibility)
// ============================================================

typedef struct {
  char hash[41];
  char author[64];
  char date[20];
  char subject[256];
} git_commit_t;

typedef struct {
  char path[512];
  char status;
  bool staged;
} git_file_status_t;

typedef struct {
  char name[256];
  bool is_current;
  bool is_remote;
} git_branch_t;

typedef struct git_repo_s git_repo_t;

typedef enum {
  GIT_OP_FETCH,
  GIT_OP_PULL,
  GIT_OP_PUSH,
  GIT_OP_CLONE,
  GIT_OP_GENERIC,
} git_op_t;

typedef struct {
  git_op_t  op;
  bool      success;
  char      output[4096];
} git_async_result_t;

// ============================================================
// Application state
// ============================================================

typedef struct {
  git_repo_t  *repo;
  char         repo_path[512];
  database_t  *db;

  int          selected_commit;
  int          selected_file;

  // UI windows
  window_t    *main_win;
  window_t    *menubar_win;
  window_t    *branches_win;
  window_t    *log_win;
  window_t    *files_win;
  window_t    *diff_win;

  // Splitter state
  int          right_w;
  int          vsplit_y;
  window_t    *vsplitter_win;
  window_t    *hsplitter_win;
  window_t    *dragging_splitter;
  int          drag_start_mouse;
  int          drag_start_val;

  accel_table_t *accel;
  hinstance_t    hinstance;
} gc_state_t;

extern gc_state_t *g_gc;

// ============================================================
// Database procedure (gitclient_db.c)
// ============================================================

lresult_t gitclient_db(database_t *db, uint32_t msg, uint32_t wparam, void *lparam);

// ============================================================
// Controller (controller.c)
// ============================================================

void gc_load_from_git(void);
bool gc_stage_file(const char *path);
bool gc_unstage_file(const char *path);
bool gc_commit(const char *message, bool amend);
bool gc_create_branch(const char *name, const char *from, bool checkout);
void gc_stash(void);
void gc_stash_pop(void);

// ============================================================
// Git backend (git_backend.c)
// ============================================================

git_repo_t *git_repo_open(const char *path);
void        git_repo_close(git_repo_t *repo);
bool        git_repo_valid(git_repo_t *repo);
const char *git_repo_path(git_repo_t *repo);

int  git_get_log(git_repo_t *repo, git_commit_t *out, int max);
int  git_get_status(git_repo_t *repo, git_file_status_t *out, int max);
bool git_get_diff(git_repo_t *repo, const char *path,
                  bool staged, char *buf, int buf_sz);
int  git_get_branches(git_repo_t *repo, git_branch_t *out, int max);
bool git_current_branch(git_repo_t *repo, char *buf, int buf_sz);
int  git_get_remotes(git_repo_t *repo, char (*out)[256], int max);

bool git_run_async(git_repo_t *repo, git_op_t op,
                   const char *args[],
                   window_t *notify_win);

bool git_run_sync(git_repo_t *repo, const char *args[],
                  char *buf, int buf_sz);

void git_async_result_free(git_async_result_t *r);

// ============================================================
// Menus (view_menubar.c)
// ============================================================

void gc_create_menubar(void);
void gc_handle_command(uint16_t id);
result_t gc_menubar_proc(window_t *win, uint32_t msg,
                         uint32_t wparam, void *lparam);

// ============================================================
// View — main window (view_main.c)
// ============================================================

result_t gc_main_proc(window_t *win, uint32_t msg,
                      uint32_t wparam, void *lparam);
void gc_open_repo(const char *path);
void gc_refresh_all(void);
void gc_update_status(void);
void gc_layout_panels(window_t *win);

// ============================================================
// View — diff viewer (view_diff.c)
// ============================================================

result_t gc_diff_proc(window_t *win, uint32_t msg,
                      uint32_t wparam, void *lparam);
void gc_diff_refresh(void);

// ============================================================
// View — dialogs
// ============================================================

bool gc_show_commit_dialog(window_t *parent, bool amend);
bool gc_show_new_branch_dialog(window_t *parent);
void gc_show_push_pull_dialog(window_t *parent, git_op_t op);
void gc_show_about_dialog(window_t *parent);

#endif // __GITCLIENT_H__
