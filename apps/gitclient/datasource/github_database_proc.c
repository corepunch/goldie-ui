// GitHub database adaptor — issues and pull requests fetched via the gh CLI.

#include "gitclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ──────────────────────────────────────────────────────────────────────────────
// Field schemas — mirror the github_db database definition in gitclient.orion.
// ──────────────────────────────────────────────────────────────────────────────

static const db_field_schema_t github_issues_schema[] = {
  { "id",         DB_TYPE_INT,    0,   true,  NULL, NULL },
  { "number",     DB_TYPE_INT,    0,   false, NULL, NULL },
  { "title",      DB_TYPE_STRING, 256, false, NULL, NULL },
  { "state",      DB_TYPE_STRING, 16,  false, NULL, NULL },
  { "author",     DB_TYPE_STRING, 64,  false, NULL, NULL },
  { "created_at", DB_TYPE_STRING, 32,  false, NULL, NULL },
};

static const db_field_schema_t github_pulls_schema[] = {
  { "id",     DB_TYPE_INT,    0,   true,  NULL, NULL },
  { "number", DB_TYPE_INT,    0,   false, NULL, NULL },
  { "title",  DB_TYPE_STRING, 256, false, NULL, NULL },
  { "state",  DB_TYPE_STRING, 16,  false, NULL, NULL },
  { "author", DB_TYPE_STRING, 64,  false, NULL, NULL },
  { "base",   DB_TYPE_STRING, 64,  false, NULL, NULL },
};

static db_table_schema_t github_db_tables[] = {
  { TABLE_ISSUES, "issues", NULL, github_issues_schema, 6, NULL, 0 },
  { TABLE_PULLS,  "pulls",  NULL, github_pulls_schema,  6, NULL, 0 },
};

static db_schema_def_t github_db_schema = {
  .name        = NULL, .class_name  = NULL, .source_path = NULL,
  .tables      = github_db_tables, .table_count = 2,
};

// ──────────────────────────────────────────────────────────────────────────────
// Field bindings — map field names to global column IDs.
// ──────────────────────────────────────────────────────────────────────────────

static const db_field_msg_binding_t github_issue_bindings[] = {
  { "id",         GC_COL_ISSUE_ID },
  { "number",     GC_COL_ISSUE_NUMBER },
  { "title",      GC_COL_ISSUE_TITLE },
  { "state",      GC_COL_ISSUE_STATE },
  { "author",     GC_COL_ISSUE_AUTHOR },
  { "created_at", GC_COL_ISSUE_CREATED_AT },
};

static const db_field_msg_binding_t github_pull_bindings[] = {
  { "id",     GC_COL_PULL_ID },
  { "number", GC_COL_PULL_NUMBER },
  { "title",  GC_COL_PULL_TITLE },
  { "state",  GC_COL_PULL_STATE },
  { "author", GC_COL_PULL_AUTHOR },
  { "base",   GC_COL_PULL_BASE },
};

// ──────────────────────────────────────────────────────────────────────────────
// Object procs — text serialisation for table views.
// ──────────────────────────────────────────────────────────────────────────────

static result_t github_issue_object_proc(const void *object, uint32_t msg,
                                          uint32_t wparam, void *lparam) {
  if (msg != dbObjGetFieldText || !object || !lparam) return false;
  const db_issue_t *issue = (const db_issue_t *)object;
  int field_index = (int)LOWORD(wparam) - GC_COL_ISSUE_ID;
  size_t buf_sz = (size_t)HIWORD(wparam);
  char *buf = (char *)lparam;
  if (field_index < 0 || field_index >= 6 || buf_sz == 0) return false;
  switch (field_index) {
    case 0: snprintf(buf, buf_sz, "%d",  issue->id);         return true;
    case 1: snprintf(buf, buf_sz, "%d",  issue->number);     return true;
    case 2: snprintf(buf, buf_sz, "%s",  issue->title);      return true;
    case 3: snprintf(buf, buf_sz, "%s",  issue->state);      return true;
    case 4: snprintf(buf, buf_sz, "%s",  issue->author);     return true;
    case 5: snprintf(buf, buf_sz, "%s",  issue->created_at); return true;
  }
  return false;
}

static result_t github_pull_object_proc(const void *object, uint32_t msg,
                                         uint32_t wparam, void *lparam) {
  if (msg != dbObjGetFieldText || !object || !lparam) return false;
  const db_pull_t *pull = (const db_pull_t *)object;
  int field_index = (int)LOWORD(wparam) - GC_COL_PULL_ID;
  size_t buf_sz = (size_t)HIWORD(wparam);
  char *buf = (char *)lparam;
  if (field_index < 0 || field_index >= 6 || buf_sz == 0) return false;
  switch (field_index) {
    case 0: snprintf(buf, buf_sz, "%d", pull->id);     return true;
    case 1: snprintf(buf, buf_sz, "%d", pull->number); return true;
    case 2: snprintf(buf, buf_sz, "%s", pull->title);  return true;
    case 3: snprintf(buf, buf_sz, "%s", pull->state);  return true;
    case 4: snprintf(buf, buf_sz, "%s", pull->author); return true;
    case 5: snprintf(buf, buf_sz, "%s", pull->base);   return true;
  }
  return false;
}

// ──────────────────────────────────────────────────────────────────────────────
// Per-database heap storage.
// ──────────────────────────────────────────────────────────────────────────────

typedef struct {
  db_issue_t *issues;
  int         issue_count, issue_capacity, issue_next_id;
  db_pull_t  *pulls;
  int         pull_count, pull_capacity, pull_next_id;
} github_db_ctx_t;

static void *gh_append_row(void **data, int *count, int *capacity,
                            size_t rec_size, int *next_id, int chunk) {
  if (*count >= *capacity) {
    int new_cap = *capacity + chunk;
    void *grown = realloc(*data, (size_t)new_cap * rec_size);
    if (!grown) return NULL;
    *data = grown; *capacity = new_cap;
  }
  void *row = (char *)(*data) + (size_t)(*count) * rec_size;
  memset(row, 0, rec_size);
  *(int *)row = (*next_id)++;
  (*count)++;
  return row;
}

static result_node_t *gh_fetch_all(void *data, int count, size_t rec_size) {
  if (!data || count == 0) return NULL;
  result_node_t *head = NULL, *tail = NULL;
  for (int i = 0; i < count; i++) {
    char *row = (char *)data + (size_t)i * rec_size;
    result_node_t *node = malloc(sizeof(result_node_t) + sizeof(void *));
    node->next = NULL;
    *(void **)node->data = row;
    if (tail) tail->next = node; else head = node;
    tail = node;
  }
  return head;
}

// ──────────────────────────────────────────────────────────────────────────────
// gh CLI helpers — fetch issues and PRs via tab-separated jq output.
// ──────────────────────────────────────────────────────────────────────────────

static char *gh_strsep(char **sp, char delim) {
  char *start = *sp;
  if (!start) return NULL;
  char *p = strchr(start, delim);
  if (p) { *p = '\0'; *sp = p + 1; }
  else    *sp = NULL;
  return start;
}

static void gh_load_issues(github_db_ctx_t *ctx) {
  static const char *cmd =
    "gh issue list --limit 30 --state open "
    "--json number,title,state,author,createdAt "
    "--jq '.[] | [(.number|tostring),.title,.state,.author.login,.createdAt] | join(\"\\t\")' "
    "2>/dev/null";

  FILE *fp = popen(cmd, "r");
  if (!fp) return;

  char line[700];
  while (fgets(line, sizeof(line), fp)) {
    char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
    char *p = line;
    char *num_s    = gh_strsep(&p, '\t');
    char *title_s  = gh_strsep(&p, '\t');
    char *state_s  = gh_strsep(&p, '\t');
    char *author_s = gh_strsep(&p, '\t');
    char *date_s   = gh_strsep(&p, '\t');
    if (!num_s || !title_s || !state_s || !author_s) continue;

    db_issue_t *rec = gh_append_row((void **)&ctx->issues, &ctx->issue_count,
                                     &ctx->issue_capacity, sizeof(db_issue_t),
                                     &ctx->issue_next_id, 32);
    if (!rec) break;
    rec->number = atoi(num_s);
    strncpy(rec->title,  title_s,  sizeof(rec->title)  - 1);
    strncpy(rec->state,  state_s,  sizeof(rec->state)  - 1);
    strncpy(rec->author, author_s, sizeof(rec->author) - 1);
    if (date_s) strncpy(rec->created_at, date_s, sizeof(rec->created_at) - 1);
  }
  pclose(fp);
}

static void gh_load_pulls(github_db_ctx_t *ctx) {
  static const char *cmd =
    "gh pr list --limit 30 --state open "
    "--json number,title,state,author,baseRefName "
    "--jq '.[] | [(.number|tostring),.title,.state,.author.login,.baseRefName] | join(\"\\t\")' "
    "2>/dev/null";

  FILE *fp = popen(cmd, "r");
  if (!fp) return;

  char line[700];
  while (fgets(line, sizeof(line), fp)) {
    char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
    char *p = line;
    char *num_s    = gh_strsep(&p, '\t');
    char *title_s  = gh_strsep(&p, '\t');
    char *state_s  = gh_strsep(&p, '\t');
    char *author_s = gh_strsep(&p, '\t');
    char *base_s   = gh_strsep(&p, '\t');
    if (!num_s || !title_s || !state_s || !author_s) continue;

    db_pull_t *rec = gh_append_row((void **)&ctx->pulls, &ctx->pull_count,
                                    &ctx->pull_capacity, sizeof(db_pull_t),
                                    &ctx->pull_next_id, 32);
    if (!rec) break;
    rec->number = atoi(num_s);
    strncpy(rec->title,  title_s,  sizeof(rec->title)  - 1);
    strncpy(rec->state,  state_s,  sizeof(rec->state)  - 1);
    strncpy(rec->author, author_s, sizeof(rec->author) - 1);
    if (base_s) strncpy(rec->base, base_s, sizeof(rec->base) - 1);
  }
  pclose(fp);
}

// ──────────────────────────────────────────────────────────────────────────────
// Database procedure.
// ──────────────────────────────────────────────────────────────────────────────

lresult_t github_database_proc(database_t *db, uint32_t msg,
                                uint32_t wparam, void *lparam) {
  github_db_ctx_t *ctx = (github_db_ctx_t *)db->userdata;

  switch (msg) {
    case dbCreate:
      ctx = calloc(1, sizeof(github_db_ctx_t));
      if (!ctx) return 0;
      db->userdata = ctx;
      return 1;

    case dbDestroy:
      if (ctx) {
        free(ctx->issues);
        free(ctx->pulls);
        free(ctx);
        db->userdata = NULL;
      }
      return 1;

    case dbLoad: {
      if (!ctx) return 0;
      free(ctx->issues); ctx->issues = NULL;
      ctx->issue_count = ctx->issue_capacity = ctx->issue_next_id = 0;
      free(ctx->pulls);  ctx->pulls  = NULL;
      ctx->pull_count  = ctx->pull_capacity  = ctx->pull_next_id  = 0;

      gh_load_issues(ctx);
      gh_load_pulls(ctx);
      return 1;
    }

    case dbFetch: {
      if (!ctx) return (lresult_t)NULL;
      int table_id = (int)LOWORD(wparam);
      if (table_id == TABLE_ISSUES)
        return (lresult_t)gh_fetch_all(ctx->issues, ctx->issue_count, sizeof(db_issue_t));
      if (table_id == TABLE_PULLS)
        return (lresult_t)gh_fetch_all(ctx->pulls, ctx->pull_count, sizeof(db_pull_t));
      return (lresult_t)NULL;
    }

    case dbGetObjectProc:
      if ((int)wparam == TABLE_ISSUES) return (lresult_t)github_issue_object_proc;
      if ((int)wparam == TABLE_PULLS)  return (lresult_t)github_pull_object_proc;
      return (lresult_t)NULL;

    case dbGetFieldBindings: {
      int *count_out = (int *)lparam;
      if ((int)wparam == TABLE_ISSUES) {
        if (count_out) *count_out = 6;
        return (lresult_t)github_issue_bindings;
      }
      if ((int)wparam == TABLE_PULLS) {
        if (count_out) *count_out = 6;
        return (lresult_t)github_pull_bindings;
      }
      if (count_out) *count_out = 0;
      return (lresult_t)NULL;
    }

    case dbGetSchema:
      github_db_schema.name        = db->name;
      github_db_schema.class_name  = db->class_name;
      github_db_schema.source_path = db->source_path;
      return (lresult_t)&github_db_schema;

    case dbGetFieldMeta: {
      int *count_out = (int *)lparam;
      if ((int)wparam == TABLE_ISSUES) {
        if (count_out) *count_out = ARRAY_LEN(github_issues_schema);
        return (lresult_t)github_issues_schema;
      }
      if ((int)wparam == TABLE_PULLS) {
        if (count_out) *count_out = ARRAY_LEN(github_pulls_schema);
        return (lresult_t)github_pulls_schema;
      }
      if (count_out) *count_out = 0;
      return (lresult_t)NULL;
    }

    default: return 0;
  }
  return 0;
}
