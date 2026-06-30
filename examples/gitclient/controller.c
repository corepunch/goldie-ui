// Controller — business logic and git-to-database population.

#include "gitclient.h"

// ═══════════════════════════════════════════════════════════════════════════
// gc_load_from_git — populate database tables from git output
// ═══════════════════════════════════════════════════════════════════════════

void gc_load_from_git(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !gc->db) return;

  send_db_message(gc->db, dbDelete, ID_DB_BRANCHES, (void *)(intptr_t)0);
  send_db_message(gc->db, dbDelete, ID_DB_COMMITS,  (void *)(intptr_t)0);
  send_db_message(gc->db, dbDelete, ID_DB_FILES,    (void *)(intptr_t)0);
  send_db_message(gc->db, dbDelete, ID_DB_DIFF,     (void *)(intptr_t)0);

  {
    git_branch_t raw[128];
    int count = git_get_branches(gc->repo, raw, 128);
    for (int i = 0; i < count; i++) {
      db_branche_t rec = {0};
      strncpy(rec.name, raw[i].name, sizeof(rec.name) - 1);
      rec.is_current = raw[i].is_current;
      rec.is_remote  = raw[i].is_remote;
      send_db_message(gc->db, dbInsert, ID_DB_BRANCHES, &rec);
    }
  }

  {
    git_commit_t raw[500];
    int count = git_get_log(gc->repo, raw, 500);
    for (int i = 0; i < count; i++) {
      db_commit_t rec = {0};
      strncpy(rec.hash,    raw[i].hash,    sizeof(rec.hash) - 1);
      strncpy(rec.author,  raw[i].author,  sizeof(rec.author) - 1);
      strncpy(rec.date,    raw[i].date,    sizeof(rec.date) - 1);
      strncpy(rec.subject, raw[i].subject, sizeof(rec.subject) - 1);
      send_db_message(gc->db, dbInsert, ID_DB_COMMITS, &rec);
    }
  }

  {
    git_file_status_t raw[256];
    int count = git_get_status(gc->repo, raw, 256);
    for (int i = 0; i < count; i++) {
      db_file_t rec = {0};
      strncpy(rec.path, raw[i].path, sizeof(rec.path) - 1);
      rec.status[0] = raw[i].status;
      rec.status[1] = '\0';
      rec.staged = raw[i].staged;
      send_db_message(gc->db, dbInsert, ID_DB_FILES, &rec);
    }
  }

  GC_LOG("gc_load_from_git: loaded branches, commits, files");
}

// ═══════════════════════════════════════════════════════════════════════════
// Stage / unstage files
// ═══════════════════════════════════════════════════════════════════════════

bool gc_stage_file(const char *path) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !path) return false;
  const char *args[] = { "git", "add", path, NULL };
  char buf[512] = {0};
  return git_run_sync(gc->repo, args, buf, sizeof(buf));
}

bool gc_unstage_file(const char *path) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !path) return false;
  const char *args[] = { "git", "restore", "--staged", path, NULL };
  char buf[512] = {0};
  return git_run_sync(gc->repo, args, buf, sizeof(buf));
}

// ═══════════════════════════════════════════════════════════════════════════
// Commit
// ═══════════════════════════════════════════════════════════════════════════

bool gc_commit(const char *message, bool amend) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !message || !message[0]) return false;

  char buf[4096] = {0};
  bool ok;
  if (amend) {
    const char *args[] = { "git", "commit", "--amend", "-m", message, NULL };
    ok = git_run_sync(gc->repo, args, buf, sizeof(buf));
  } else {
    const char *args[] = { "git", "commit", "-m", message, NULL };
    ok = git_run_sync(gc->repo, args, buf, sizeof(buf));
  }
  if (!ok) {
    GC_LOG("gc_commit failed: %s", buf);
    return false;
  }
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Branch operations
// ═══════════════════════════════════════════════════════════════════════════

bool gc_create_branch(const char *name, const char *from, bool checkout) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !name || !name[0]) return false;

  char buf[1024] = {0};
  const char *args[] = { "git", "branch", name, from && from[0] ? from : NULL, NULL };
  if (!git_run_sync(gc->repo, args, buf, sizeof(buf))) {
    GC_LOG("gc_create_branch failed: %s", buf);
    return false;
  }

  if (checkout) {
    const char *args_co[] = { "git", "checkout", name, NULL };
    git_run_sync(gc->repo, args_co, buf, sizeof(buf));
  }
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Stash
// ═══════════════════════════════════════════════════════════════════════════

void gc_stash(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo) return;
  const char *args[] = { "git", "stash", NULL };
  char buf[256] = {0};
  git_run_sync(gc->repo, args, buf, sizeof(buf));
}

void gc_stash_pop(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo) return;
  const char *args[] = { "git", "stash", "pop", NULL };
  char buf[256] = {0};
  git_run_sync(gc->repo, args, buf, sizeof(buf));
}
