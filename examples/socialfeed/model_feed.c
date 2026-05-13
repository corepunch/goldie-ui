// MODEL: Post and comment data structures and CRUD operations.

#include "socialfeed.h"

// ============================================================
// Portable string-duplicate helper
// ============================================================

char *sf_strdup(const char *s) {
  if (!s) s = "";
  size_t len = strlen(s);
  char *copy = (char *)malloc(len + 1);
  if (copy) memcpy(copy, s, len + 1);
  return copy;
}

// ============================================================
// comment_create / comment_free
// ============================================================

comment_t *comment_create(const char *author, const char *text) {
  comment_t *c = (comment_t *)calloc(1, sizeof(comment_t));
  if (!c) return NULL;
  c->author = sf_strdup(author);
  c->text   = sf_strdup(text);
  if (!c->author || !c->text) {
    comment_free(c);
    return NULL;
  }
  c->created_at = (uint32_t)time(NULL);
  return c;
}

void comment_free(comment_t *c) {
  if (!c) return;
  for (int i = 0; i < c->reply_count; i++)
    comment_free(c->replies[i]);
  free(c->replies);
  free(c->author);
  free(c->text);
  free(c);
}

// ============================================================
// comment_add_reply — append a reply to a comment
// ============================================================

bool comment_add_reply(comment_t *c, comment_t *reply) {
  if (!c || !reply) return false;
  if (c->reply_count >= c->reply_cap) {
    int newcap = c->reply_cap ? c->reply_cap * 2 : REPLIES_INIT_CAP;
    comment_t **newbuf = (comment_t **)realloc(c->replies,
                                               (size_t)newcap * sizeof(comment_t *));
    if (!newbuf) return false;
    c->replies  = newbuf;
    c->reply_cap = newcap;
  }
  c->replies[c->reply_count++] = reply;
  return true;
}

// ============================================================
// comment_like — increment the like counter
// ============================================================

void comment_like(comment_t *c) {
  if (c) c->like_count++;
}

// ============================================================
// post_create / post_free
// ============================================================

post_t *post_create(const char *author, const char *title, const char *body) {
  post_t *p = (post_t *)calloc(1, sizeof(post_t));
  if (!p) return NULL;
  p->author = sf_strdup(author);
  p->title  = sf_strdup(title);
  p->body   = sf_strdup(body);
  if (!p->author || !p->title || !p->body) {
    post_free(p);
    return NULL;
  }
  p->created_at = (uint32_t)time(NULL);
  return p;
}

void post_free(post_t *p) {
  if (!p) return;
  for (int i = 0; i < p->comment_count; i++)
    comment_free(p->comments[i]);
  free(p->comments);
  free(p->author);
  free(p->title);
  free(p->body);
  free(p);
}

// ============================================================
// post_add_comment — append a comment, grow the array if needed
// ============================================================

bool post_add_comment(post_t *p, comment_t *c) {
  if (!p || !c) return false;
  if (p->comment_count >= p->comment_cap) {
    int newcap = p->comment_cap ? p->comment_cap * 2 : COMMENTS_INIT_CAP;
    comment_t **newbuf = (comment_t **)realloc(p->comments,
                                               (size_t)newcap * sizeof(comment_t *));
    if (!newbuf) return false;
    p->comments   = newbuf;
    p->comment_cap = newcap;
  }
  p->comments[p->comment_count++] = c;
  return true;
}

// ============================================================
// post_like — increment the like counter
// ============================================================

void post_like(post_t *p) {
  if (p) p->like_count++;
}

enum {
  sfPostFieldTitle = 1,
  sfPostFieldAuthor,
  sfPostFieldLikeCount,
  sfPostFieldCommentCount,
};

static result_t socialfeed_post_proc(const void *object, uint32_t msg,
                                     uint32_t wparam, void *lparam) {
  const post_t *p = (const post_t *)object;
  char *buf = (char *)lparam;
  // db_object_get_field_text passes buffer size through uint32_t wparam.
  size_t buf_sz = (size_t)wparam;
  if (!p || !buf || buf_sz == 0) return false;

  switch (msg) {
    case sfPostFieldTitle:
      snprintf(buf, buf_sz, "%s", p->title ? p->title : "");
      return true;
    case sfPostFieldAuthor:
      snprintf(buf, buf_sz, "%s", p->author ? p->author : "");
      return true;
    case sfPostFieldLikeCount:
      snprintf(buf, buf_sz, "%d", p->like_count);
      return true;
    case sfPostFieldCommentCount:
      snprintf(buf, buf_sz, "%d", p->comment_count);
      return true;
    default:
      return false;
  }
}

static const db_field_msg_binding_t kPostFieldBindings[] = {
  { "title", sfPostFieldTitle },
  { "author", sfPostFieldAuthor },
  { "like_count", sfPostFieldLikeCount },
  { "comment_count", sfPostFieldCommentCount },
};

bool socialfeed_post_field_text(const post_t *p, const char *field,
                                char *buf, size_t buf_sz) {
  return db_object_get_field_text(kPostFieldBindings, ARRAY_LEN(kPostFieldBindings),
                                  socialfeed_post_proc, p, field, buf, buf_sz);
}

enum {
  sfCommentFieldAuthor = 1,
  sfCommentFieldText,
  sfCommentFieldLikeCount,
};

static result_t socialfeed_comment_proc(const void *object, uint32_t msg,
                                        uint32_t wparam, void *lparam) {
  const comment_t *c = (const comment_t *)object;
  char *buf = (char *)lparam;
  // db_object_get_field_text passes buffer size through uint32_t wparam.
  size_t buf_sz = (size_t)wparam;
  if (!c || !buf || buf_sz == 0) return false;

  switch (msg) {
    case sfCommentFieldAuthor:
      snprintf(buf, buf_sz, "%s", c->author ? c->author : "");
      return true;
    case sfCommentFieldText:
      snprintf(buf, buf_sz, "%s", c->text ? c->text : "");
      return true;
    case sfCommentFieldLikeCount:
      snprintf(buf, buf_sz, "%d", c->like_count);
      return true;
    default:
      return false;
  }
}

static const db_field_msg_binding_t kCommentFieldBindings[] = {
  { "author", sfCommentFieldAuthor },
  { "text", sfCommentFieldText },
  { "like_count", sfCommentFieldLikeCount },
};

bool socialfeed_comment_field_text(const comment_t *c, const char *field,
                                   char *buf, size_t buf_sz) {
  return db_object_get_field_text(kCommentFieldBindings, ARRAY_LEN(kCommentFieldBindings),
                                  socialfeed_comment_proc, c, field, buf, buf_sz);
}

bool socialfeed_comment_has_field(const char *field) {
  if (!field || !*field) return false;
  for (int i = 0; i < ARRAY_LEN(kCommentFieldBindings); i++) {
    if (kCommentFieldBindings[i].field &&
        !strcmp(kCommentFieldBindings[i].field, field))
      return true;
  }
  return false;
}
