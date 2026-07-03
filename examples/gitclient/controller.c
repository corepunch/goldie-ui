// Controller — business logic and git-to-database population.

#include "gitclient.h"

// ═══════════════════════════════════════════════════════════════════════════
// gc_load_from_git — populate database tables from git output
// ═══════════════════════════════════════════════════════════════════════════

void gc_load_from_git(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !gc->db) return;

  int branch_count = 0;
  git_branch_t branches[128];
  int branch_ids[128] = {0};

  send_db_message(gc->db, dbDelete, ID_DB_BRANCHES, (void *)(intptr_t)0);
  send_db_message(gc->db, dbDelete, ID_DB_COMMITS,  (void *)(intptr_t)0);
  send_db_message(gc->db, dbDelete, ID_DB_FILES,    (void *)(intptr_t)0);
  send_db_message(gc->db, dbDelete, ID_DB_DIFF,     (void *)(intptr_t)0);

  {
    int count = git_get_branches(gc->repo, branches, 128);
    branch_count = count;
    for (int i = 0; i < count; i++) {
      db_branche_t rec = {0};
      strncpy(rec.name, branches[i].name, sizeof(rec.name) - 1);
      rec.is_current = branches[i].is_current;
      rec.is_remote  = branches[i].is_remote;
      db_branche_t *inserted = (db_branche_t *)send_db_message(gc->db, dbInsert, ID_DB_BRANCHES, &rec);
      if (inserted) branch_ids[i] = inserted->id;
    }
  }

  {
    for (int branch = 0; branch < branch_count; branch++) {
      git_commit_t raw[500];
      int count = git_get_log_ref(gc->repo, branches[branch].name, raw, 500);
      for (int i = 0; i < count; i++) {
        db_commit_t rec = {0};
        rec.branch_id = branch_ids[branch];
        strncpy(rec.hash,    raw[i].hash,    sizeof(rec.hash) - 1);
        strncpy(rec.author,  raw[i].author,  sizeof(rec.author) - 1);
        strncpy(rec.date,    raw[i].date,    sizeof(rec.date) - 1);
        strncpy(rec.subject, raw[i].subject, sizeof(rec.subject) - 1);
        send_db_message(gc->db, dbInsert, ID_DB_COMMITS, &rec);
      }
    }
  }

  {
    git_file_status_t raw[256];
    int count = git_get_status(gc->repo, raw, 256);
    for (int i = 0; i < count; i++) {
      db_file_t rec = {0};
      rec.commit_id = 0;
      strncpy(rec.path, raw[i].path, sizeof(rec.path) - 1);
      rec.status[0] = raw[i].status;
      rec.status[1] = '\0';
      rec.staged = raw[i].staged;
      send_db_message(gc->db, dbInsert, ID_DB_FILES, &rec);
    }
  }

  GC_LOG("database populated: branches=%d commits=%d files=%d",
         branch_count, commit_count, file_count);
}

void gc_load_commit_files(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !gc->db) return;
  if (gc->selected_commit < 0) {
    gc_load_from_git();
    return;
  }

  send_db_message(gc->db, dbDelete, ID_DB_FILES, (void *)(intptr_t)0);

  db_commit_t *c = (db_commit_t *)(intptr_t)send_message(
    gc->log_win, tvGetSelectedRecord, 0, NULL);
  if (!c || !c->hash[0]) {
    return;
  }

  char buf[64 * 1024] = {0};
  const char *args[] = {
    "git", "show", "--name-only", "--pretty=format:", c->hash, NULL
  };
  if (!git_run_sync(gc->repo, args, buf, sizeof(buf))) {
    return;
  }

  char *line = buf;
  while (*line) {
    char *nl = strchr(line, '\n');
    if (nl) *nl = '\0';
    if (line[0] && strcmp(line, c->hash) != 0) {
      db_file_t rec = {0};
      rec.commit_id = c->id;
      rec.status[0] = 'M';
      rec.status[1] = '\0';
      rec.staged = false;
      strncpy(rec.path, line, sizeof(rec.path) - 1);
      send_db_message(gc->db, dbInsert, ID_DB_FILES, &rec);
    }
    if (!nl) break;
    line = nl + 1;
  }
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
// Checkout / delete / merge / rebase / rename
// ═══════════════════════════════════════════════════════════════════════════

bool gc_checkout_branch(const char *name) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !name || !name[0]) return false;
  char buf[1024] = {0};
  const char *args[] = { "git", "checkout", name, NULL };
  if (!git_run_sync(gc->repo, args, buf, sizeof(buf))) {
    GC_LOG("gc_checkout_branch failed: %s", buf);
    return false;
  }
  return true;
}

bool gc_delete_branch(const char *name, bool remote) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !name || !name[0]) return false;
  char buf[1024] = {0};
  if (remote) {
    const char *args[] = { "git", "push", "origin", "--delete", name, NULL };
    if (!git_run_sync(gc->repo, args, buf, sizeof(buf))) {
      GC_LOG("gc_delete_branch remote failed: %s", buf);
      return false;
    }
  } else {
    const char *args[] = { "git", "branch", "-d", name, NULL };
    if (!git_run_sync(gc->repo, args, buf, sizeof(buf))) {
      // Try force delete if -d fails (unmerged)
      const char *args_f[] = { "git", "branch", "-D", name, NULL };
      if (!git_run_sync(gc->repo, args_f, buf, sizeof(buf))) {
        GC_LOG("gc_delete_branch failed: %s", buf);
        return false;
      }
    }
  }
  return true;
}

bool gc_merge_branch(const char *name) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !name || !name[0]) return false;
  char buf[4096] = {0};
  const char *args[] = { "git", "merge", name, NULL };
  if (!git_run_sync(gc->repo, args, buf, sizeof(buf))) {
    GC_LOG("gc_merge_branch failed: %s", buf);
    return false;
  }
  return true;
}

bool gc_rebase_onto(const char *branch) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !branch || !branch[0]) return false;
  char buf[4096] = {0};
  const char *args[] = { "git", "rebase", branch, NULL };
  if (!git_run_sync(gc->repo, args, buf, sizeof(buf))) {
    GC_LOG("gc_rebase_onto failed: %s", buf);
    return false;
  }
  return true;
}

bool gc_rename_branch(const char *old_name, const char *new_name) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !old_name || !old_name[0] || !new_name || !new_name[0])
    return false;
  char buf[1024] = {0};
  const char *args[] = { "git", "branch", "-m", old_name, new_name, NULL };
  if (!git_run_sync(gc->repo, args, buf, sizeof(buf))) {
    GC_LOG("gc_rename_branch failed: %s", buf);
    return false;
  }
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Discard changes
// ═══════════════════════════════════════════════════════════════════════════

bool gc_discard_file(const char *path) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo || !path) return false;
  char buf[1024] = {0};
  // Try checkout first (tracked files), then clean (untracked)
  const char *args_co[] = { "git", "checkout", "--", path, NULL };
  if (git_run_sync(gc->repo, args_co, buf, sizeof(buf)))
    return true;
  const char *args_cl[] = { "git", "clean", "-f", path, NULL };
  return git_run_sync(gc->repo, args_cl, buf, sizeof(buf));
}

bool gc_discard_all(void) {
  gc_state_t *gc = g_gc;
  if (!gc || !gc->repo) return false;
  char buf[4096] = {0};
  const char *args_co[] = { "git", "checkout", "--", ".", NULL };
  git_run_sync(gc->repo, args_co, buf, sizeof(buf));
  const char *args_cl[] = { "git", "clean", "-fd", NULL };
  return git_run_sync(gc->repo, args_cl, buf, sizeof(buf));
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
