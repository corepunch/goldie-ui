// Seed-data loader for Social Feed.
//
// The seed data lives in socialfeed_seed.xml.  On Windows we parse it with
// xmllite; on other platforms we use a tiny fallback parser for the same
// constrained file format so the example still builds here.

#include "socialfeed.h"

#include <ctype.h>

#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <xmllite.h>
#endif

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} text_buf_t;

static bool read_file_bytes(const char *path, unsigned char **out_buf, size_t *out_size) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  long n = ftell(f);
  if (n < 0) {
    fclose(f);
    return false;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return false;
  }

  unsigned char *buf = (unsigned char *)malloc((size_t)n + 1);
  if (!buf) {
    fclose(f);
    return false;
  }
  size_t got = fread(buf, 1, (size_t)n, f);
  fclose(f);
  if (got != (size_t)n) {
    free(buf);
    return false;
  }
  buf[n] = 0;
  *out_buf = buf;
  *out_size = (size_t)n;
  return true;
}

static bool read_seed_file(const char *path, unsigned char **out_buf, size_t *out_size) {
  static const char *kFallbacks[] = {
    "examples/socialfeed/socialfeed_seed.xml",
    "build/share/socialfeed/socialfeed_seed.xml",
    "share/socialfeed_seed.xml",
    "share/socialfeed/socialfeed_seed.xml",
  };

  if (read_file_bytes(path, out_buf, out_size)) return true;
  for (size_t i = 0; i < sizeof(kFallbacks) / sizeof(kFallbacks[0]); i++) {
    if (read_file_bytes(kFallbacks[i], out_buf, out_size)) return true;
  }
  return false;
}

#ifdef _WIN32
static void text_buf_reset(text_buf_t *tb) {
  if (!tb) return;
  tb->len = 0;
  if (tb->buf) tb->buf[0] = '\0';
}

static bool text_buf_append(text_buf_t *tb, const char *s, size_t n) {
  if (!tb || !s) return false;
  if (n == 0) return true;
  if (tb->len + n + 1 > tb->cap) {
    size_t new_cap = tb->cap ? tb->cap * 2 : 256;
    while (new_cap < tb->len + n + 1) new_cap *= 2;
    char *p = (char *)realloc(tb->buf, new_cap);
    if (!p) return false;
    tb->buf = p;
    tb->cap = new_cap;
  }
  memcpy(tb->buf + tb->len, s, n);
  tb->len += n;
  tb->buf[tb->len] = '\0';
  return true;
}

static bool text_buf_append_str(text_buf_t *tb, const char *s) {
  return text_buf_append(tb, s, s ? strlen(s) : 0);
}
#endif

static char *trim_and_unescape_xml(const char *s) {
  if (!s) return sf_strdup("");

  size_t len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[0])) {
    s++;
    len--;
  }
  while (len > 0 && isspace((unsigned char)s[len - 1])) {
    len--;
  }

  char *out = (char *)malloc(len + 1);
  if (!out) return NULL;

  size_t oi = 0;
  for (size_t i = 0; i < len; i++) {
    if (s[i] != '&') {
      out[oi++] = s[i];
      continue;
    }
    if (len - i >= 5 && strncmp(s + i, "&amp;", 5) == 0) {
      out[oi++] = '&';
      i += 4;
    } else if (len - i >= 4 && strncmp(s + i, "&lt;", 4) == 0) {
      out[oi++] = '<';
      i += 3;
    } else if (len - i >= 4 && strncmp(s + i, "&gt;", 4) == 0) {
      out[oi++] = '>';
      i += 3;
    } else if (len - i >= 6 && strncmp(s + i, "&quot;", 6) == 0) {
      out[oi++] = '"';
      i += 5;
    } else if (len - i >= 6 && strncmp(s + i, "&apos;", 6) == 0) {
      out[oi++] = '\'';
      i += 5;
    } else {
      out[oi++] = s[i];
    }
  }
  out[oi] = '\0';
  return out;
}

static bool parse_fixed_attrs(const char *attrs, char *author, size_t author_sz,
                              char *title, size_t title_sz, int *likes) {
  (void)author_sz;
  (void)title_sz;
  if (title) title[0] = '\0';
  if (author) author[0] = '\0';
  if (likes) *likes = 0;
  if (!attrs) return false;

  if (title) {
    if (sscanf(attrs, "author=\"%127[^\"]\" title=\"%255[^\"]\" likes=\"%d\"",
               author, title, likes) == 3)
      return true;
  } else {
    if (sscanf(attrs, "author=\"%127[^\"]\" likes=\"%d\"",
               author, likes) == 2)
      return true;
  }
  return false;
}

static bool read_tag_header(const char **p, char *name, size_t name_sz,
                            char *attrs, size_t attrs_sz, bool *closing) {
  const char *s = *p;
  if (!s || *s != '<') return false;
  s++;
  if (*s == '/') {
    if (closing) *closing = true;
    s++;
  } else {
    if (closing) *closing = false;
  }

  size_t ni = 0;
  while (*s && !isspace((unsigned char)*s) && *s != '>' && *s != '/') {
    if (ni + 1 < name_sz) name[ni++] = *s;
    s++;
  }
  name[ni] = '\0';

  while (*s && isspace((unsigned char)*s)) s++;
  size_t ai = 0;
  while (*s && *s != '>' && !(*s == '/' && s[1] == '>')) {
    if (ai + 1 < attrs_sz) attrs[ai++] = *s;
    s++;
  }
  attrs[ai] = '\0';

  if (*s == '/' && s[1] == '>') {
    s += 2;
  } else if (*s == '>') {
    s++;
  } else {
    return false;
  }
  *p = s;
  return true;
}

static bool parse_text_element(const char **p, char **out_text) {
  char tag[16];
  char attrs[8];
  bool closing = false;
  const char *s = *p;
  if (!read_tag_header(&s, tag, sizeof(tag), attrs, sizeof(attrs), &closing))
    return false;
  if (closing || strcmp(tag, "text") != 0) return false;

  const char *end = strstr(s, "</text>");
  if (!end) return false;

  char *raw = (char *)malloc((size_t)(end - s) + 1);
  if (!raw) return false;
  memcpy(raw, s, (size_t)(end - s));
  raw[end - s] = '\0';

  char *txt = trim_and_unescape_xml(raw);
  free(raw);
  if (!txt) return false;

  *out_text = txt;
  *p = end + 7;
  return true;
}

static bool parse_reply_manual(const char **p, comment_t *parent);

static bool parse_comment_manual(const char **p, post_t *post) {
  char tag[16];
  char attrs[512];
  bool closing = false;
  const char *s = *p;
  if (!read_tag_header(&s, tag, sizeof(tag), attrs, sizeof(attrs), &closing))
    return false;
  if (closing || strcmp(tag, "comment") != 0) return false;

  char author[128] = {0};
  int likes = 0;
  if (!parse_fixed_attrs(attrs, author, sizeof(author), NULL, 0, &likes))
    return false;

  comment_t *comment = comment_create(author, "");
  if (!comment) return false;
  comment->like_count = likes;

  while (1) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (strncmp(s, "</comment>", 10) == 0) {
      s += 10;
      break;
    }
    if (strncmp(s, "<text>", 6) == 0) {
      char *txt = NULL;
      if (!parse_text_element(&s, &txt)) {
        comment_free(comment);
        return false;
      }
      free(comment->text);
      comment->text = txt;
      continue;
    }
    if (strncmp(s, "<reply", 6) == 0) {
      if (!parse_reply_manual(&s, comment)) {
        comment_free(comment);
        return false;
      }
      continue;
    }
    if (*s == '<' && strncmp(s, "<comment", 8) != 0) {
      comment_free(comment);
      return false;
    }
    if (*s == '\0') {
      comment_free(comment);
      return false;
    }
    s++;
  }

  if (!app_add_comment(post, comment)) {
    comment_free(comment);
    return false;
  }

  *p = s;
  return true;
}

static bool parse_reply_manual(const char **p, comment_t *parent) {
  char tag[16];
  char attrs[512];
  bool closing = false;
  const char *s = *p;
  if (!read_tag_header(&s, tag, sizeof(tag), attrs, sizeof(attrs), &closing))
    return false;
  if (closing || strcmp(tag, "reply") != 0) return false;

  char author[128] = {0};
  int likes = 0;
  if (!parse_fixed_attrs(attrs, author, sizeof(author), NULL, 0, &likes))
    return false;

  comment_t *reply = comment_create(author, "");
  if (!reply) return false;
  reply->like_count = likes;

  while (1) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (strncmp(s, "</reply>", 8) == 0) {
      s += 8;
      break;
    }
    if (strncmp(s, "<text>", 6) == 0) {
      char *txt = NULL;
      if (!parse_text_element(&s, &txt)) {
        comment_free(reply);
        return false;
      }
      free(reply->text);
      reply->text = txt;
      continue;
    }
    if (*s == '<') {
      comment_free(reply);
      return false;
    }
    s++;
  }

  if (!app_add_reply(parent, reply)) {
    comment_free(reply);
    return false;
  }

  *p = s;
  return true;
}

static bool parse_body_manual(const char **p, post_t *post) {
  const char *s = *p;
  if (strncmp(s, "<body>", 6) != 0) return false;
  s += 6;
  while (*s && isspace((unsigned char)*s)) s++;
  if (strncmp(s, "<text>", 6) != 0) return false;

  char *txt = NULL;
  if (!parse_text_element(&s, &txt)) return false;
  free(post->body);
  post->body = txt;

  while (*s && isspace((unsigned char)*s)) s++;
  if (strncmp(s, "</body>", 7) != 0) return false;
  s += 7;
  *p = s;
  return true;
}

static bool parse_post_manual(const char **p) {
  char tag[16];
  char attrs[512];
  bool closing = false;
  const char *s = *p;
  if (!read_tag_header(&s, tag, sizeof(tag), attrs, sizeof(attrs), &closing))
    return false;
  if (closing || strcmp(tag, "post") != 0) return false;

  char author[128] = {0};
  char title[256] = {0};
  int likes = 0;
  if (!parse_fixed_attrs(attrs, author, sizeof(author), title, sizeof(title), &likes))
    return false;

  post_t *post = post_create(author, title, "");
  if (!post) return false;
  post->like_count = likes;

  while (1) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (strncmp(s, "</post>", 7) == 0) {
      s += 7;
      break;
    }
    if (strncmp(s, "<body>", 6) == 0) {
      if (!parse_body_manual(&s, post)) {
        post_free(post);
        return false;
      }
      continue;
    }
    if (strncmp(s, "<comments>", 10) == 0) {
      s += 10;
      continue;
    }
    if (strncmp(s, "</comments>", 11) == 0) {
      s += 11;
      continue;
    }
    if (strncmp(s, "<comment", 8) == 0) {
      if (!parse_comment_manual(&s, post)) {
        post_free(post);
        return false;
      }
      continue;
    }
    if (*s == '<') {
      post_free(post);
      return false;
    }
    s++;
  }

  if (!app_add_post(post)) {
    post_free(post);
    return false;
  }
  *p = s;
  return true;
}

static bool parse_socialfeed_manual(const char *buf) {
  const char *s = buf;
  while (*s && isspace((unsigned char)*s)) s++;
  if (strncmp(s, "<?xml", 5) == 0) {
    const char *end = strstr(s, "?>");
    if (!end) return false;
    s = end + 2;
  }
  while (*s && isspace((unsigned char)*s)) s++;
  if (strncmp(s, "<socialfeed>", 12) != 0) return false;
  s += 12;

  while (1) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (strncmp(s, "</socialfeed>", 13) == 0) {
      s += 13;
      break;
    }
    if (strncmp(s, "<post", 5) == 0) {
      if (!parse_post_manual(&s)) return false;
      continue;
    }
    if (*s == '\0') return false;
    s++;
  }
  return true;
}

#ifdef _WIN32
static bool append_wide_text(text_buf_t *tb, const wchar_t *wstr) {
  if (!wstr) return true;
  int need = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
  if (need <= 0) return false;
  char *tmp = (char *)malloc((size_t)need);
  if (!tmp) return false;
  WideCharToMultiByte(CP_UTF8, 0, wstr, -1, tmp, need, NULL, NULL);
  bool ok = text_buf_append_str(tb, tmp);
  free(tmp);
  return ok;
}

static bool socialfeed_load_seed_data_xmllite(const char *path) {
  unsigned char *bytes = NULL;
  size_t size = 0;
  if (!read_seed_file(path, &bytes, &size)) return false;

  bool ok = false;
  IStream *stream = NULL;
  IXmlReader *reader = NULL;
  HGLOBAL hglobal = GlobalAlloc(GMEM_MOVEABLE, size);
  if (!hglobal) goto done;

  void *dst = GlobalLock(hglobal);
  if (!dst) goto done;
  memcpy(dst, bytes, size);
  GlobalUnlock(hglobal);

  if (FAILED(CreateStreamOnHGlobal(hglobal, TRUE, &stream))) goto done;
  hglobal = NULL;
  if (FAILED(CreateXmlReader(&IID_IXmlReader, (void **)&reader, NULL))) goto done;
  if (FAILED(IXmlReader_SetInput(reader, (IUnknown *)stream))) goto done;

  typedef enum { CTX_NONE, CTX_POST, CTX_BODY, CTX_COMMENT, CTX_REPLY, CTX_TEXT } ctx_kind_t;
  typedef struct {
    ctx_kind_t kind;
    post_t *post;
    comment_t *comment;
    comment_t *reply;
  } ctx_frame_t;

  ctx_frame_t stack[32];
  int depth = 0;
  text_buf_t tb = {0};
  XmlNodeType node_type;
  HRESULT hr;

  while ((hr = IXmlReader_Read(reader, &node_type)) == S_OK) {
    if (node_type == XmlNodeType_Whitespace)
      continue;

    if (node_type == XmlNodeType_Element) {
      const WCHAR *name = NULL;
      UINT len = 0;
      if (FAILED(IXmlReader_GetLocalName(reader, &name, &len))) goto fail;

      if (len == 4 && wcsncmp(name, L"post", 4) == 0) {
        WCHAR const *aval = NULL;
        UINT alen = 0;
        char author[128] = {0}, title[256] = {0}, num[32] = {0};
        int likes = 0;
        if (FAILED(IXmlReader_MoveToFirstAttribute(reader))) goto fail;
        while (TRUE) {
          if (FAILED(IXmlReader_GetLocalName(reader, &aval, &alen))) goto fail;
          WCHAR const *v = NULL;
          UINT vl = 0;
          if (FAILED(IXmlReader_GetValue(reader, &v, &vl))) goto fail;
          char tmp[512];
          int n = WideCharToMultiByte(CP_UTF8, 0, v, (int)vl, tmp, sizeof(tmp) - 1, NULL, NULL);
          if (n < 0) goto fail;
          tmp[n] = '\0';
          if (alen == 6 && wcsncmp(aval, L"author", 6) == 0) snprintf(author, sizeof(author), "%s", tmp);
          else if (alen == 5 && wcsncmp(aval, L"title", 5) == 0) snprintf(title, sizeof(title), "%s", tmp);
          else if (alen == 5 && wcsncmp(aval, L"likes", 5) == 0) likes = atoi(tmp);
          if (FAILED(IXmlReader_MoveToNextAttribute(reader))) break;
        }
        if (FAILED(IXmlReader_MoveToElement(reader))) goto fail;
        post_t *post = post_create(author, title, "");
        if (!post) goto fail;
        post->like_count = likes;
        stack[depth++] = (ctx_frame_t){ .kind = CTX_POST, .post = post };
        continue;
      }

      if (len == 4 && wcsncmp(name, L"body", 4) == 0) {
        if (depth < 1 || stack[depth - 1].kind != CTX_POST) goto fail;
        stack[depth++] = (ctx_frame_t){ .kind = CTX_BODY, .post = stack[depth - 1].post };
        continue;
      }

      if (len == 7 && wcsncmp(name, L"comment", 7) == 0) {
        if (depth < 1 || stack[depth - 1].kind != CTX_POST) goto fail;
        WCHAR const *aval = NULL;
        UINT alen = 0;
        char author[128] = {0}, num[32] = {0};
        int likes = 0;
        if (FAILED(IXmlReader_MoveToFirstAttribute(reader))) goto fail;
        while (TRUE) {
          if (FAILED(IXmlReader_GetLocalName(reader, &aval, &alen))) goto fail;
          WCHAR const *v = NULL;
          UINT vl = 0;
          if (FAILED(IXmlReader_GetValue(reader, &v, &vl))) goto fail;
          char tmp[256];
          int n = WideCharToMultiByte(CP_UTF8, 0, v, (int)vl, tmp, sizeof(tmp) - 1, NULL, NULL);
          if (n < 0) goto fail;
          tmp[n] = '\0';
          if (alen == 6 && wcsncmp(aval, L"author", 6) == 0) snprintf(author, sizeof(author), "%s", tmp);
          else if (alen == 5 && wcsncmp(aval, L"likes", 5) == 0) likes = atoi(tmp);
          if (FAILED(IXmlReader_MoveToNextAttribute(reader))) break;
        }
        if (FAILED(IXmlReader_MoveToElement(reader))) goto fail;
        comment_t *comment = comment_create(author, "");
        if (!comment) goto fail;
        comment->like_count = likes;
        stack[depth++] = (ctx_frame_t){ .kind = CTX_COMMENT, .comment = comment };
        continue;
      }

      if (len == 5 && wcsncmp(name, L"reply", 5) == 0) {
        if (depth < 1 || stack[depth - 1].kind != CTX_COMMENT) goto fail;
        WCHAR const *aval = NULL;
        UINT alen = 0;
        char author[128] = {0};
        int likes = 0;
        if (FAILED(IXmlReader_MoveToFirstAttribute(reader))) goto fail;
        while (TRUE) {
          if (FAILED(IXmlReader_GetLocalName(reader, &aval, &alen))) goto fail;
          WCHAR const *v = NULL;
          UINT vl = 0;
          if (FAILED(IXmlReader_GetValue(reader, &v, &vl))) goto fail;
          char tmp[256];
          int n = WideCharToMultiByte(CP_UTF8, 0, v, (int)vl, tmp, sizeof(tmp) - 1, NULL, NULL);
          if (n < 0) goto fail;
          tmp[n] = '\0';
          if (alen == 6 && wcsncmp(aval, L"author", 6) == 0) snprintf(author, sizeof(author), "%s", tmp);
          else if (alen == 5 && wcsncmp(aval, L"likes", 5) == 0) likes = atoi(tmp);
          if (FAILED(IXmlReader_MoveToNextAttribute(reader))) break;
        }
        if (FAILED(IXmlReader_MoveToElement(reader))) goto fail;
        comment_t *reply = comment_create(author, "");
        if (!reply) goto fail;
        reply->like_count = likes;
        stack[depth++] = (ctx_frame_t){ .kind = CTX_REPLY, .reply = reply };
        continue;
      }

      if (len == 4 && wcsncmp(name, L"text", 4) == 0) {
        text_buf_reset(&tb);
        stack[depth++] = (ctx_frame_t){ .kind = CTX_TEXT };
        continue;
      }
    }

    if (node_type == XmlNodeType_Text || node_type == XmlNodeType_CDATA) {
      if (depth > 0 && stack[depth - 1].kind == CTX_TEXT) {
        const WCHAR *v = NULL;
        UINT vl = 0;
        if (FAILED(IXmlReader_GetValue(reader, &v, &vl))) goto fail;
        char tmp[512];
        int n = WideCharToMultiByte(CP_UTF8, 0, v, (int)vl, tmp, sizeof(tmp) - 1, NULL, NULL);
        if (n < 0) goto fail;
        tmp[n] = '\0';
        if (!text_buf_append_str(&tb, tmp)) goto fail;
      }
      continue;
    }

    if (node_type == XmlNodeType_EndElement) {
      const WCHAR *name = NULL;
      UINT len = 0;
      if (FAILED(IXmlReader_GetLocalName(reader, &name, &len))) goto fail;

      if (len == 4 && wcsncmp(name, L"text", 4) == 0) {
        if (depth < 1 || stack[depth - 1].kind != CTX_TEXT) goto fail;
        text_buf_t *src = &tb;
        char *txt = trim_and_unescape_xml(src->buf ? src->buf : "");
        if (!txt) goto fail;
        if (depth < 2) {
          free(txt);
          goto fail;
        }
        ctx_kind_t owner = stack[depth - 2].kind;
        switch (owner) {
          case CTX_BODY:
            free(stack[depth - 2].post->body);
            stack[depth - 2].post->body = txt;
            break;
          case CTX_COMMENT:
            free(stack[depth - 2].comment->text);
            stack[depth - 2].comment->text = txt;
            break;
          case CTX_REPLY:
            free(stack[depth - 2].reply->text);
            stack[depth - 2].reply->text = txt;
            break;
          default:
            free(txt);
            goto fail;
        }
        depth--;
        continue;
      }

      if (len == 4 && wcsncmp(name, L"body", 4) == 0) {
        if (depth < 1 || stack[depth - 1].kind != CTX_BODY) goto fail;
        depth--;
        continue;
      }

      if (len == 7 && wcsncmp(name, L"comment", 7) == 0) {
        if (depth < 1 || stack[depth - 1].kind != CTX_COMMENT) goto fail;
        comment_t *c = stack[depth - 1].comment;
        if (!app_add_comment(stack[depth - 2].post, c)) {
          comment_free(c);
          goto fail;
        }
        depth--;
        continue;
      }

      if (len == 5 && wcsncmp(name, L"reply", 5) == 0) {
        if (depth < 1 || stack[depth - 1].kind != CTX_REPLY) goto fail;
        comment_t *r = stack[depth - 1].reply;
        if (!app_add_reply(stack[depth - 2].comment, r)) {
          comment_free(r);
          goto fail;
        }
        depth--;
        continue;
      }

      if (len == 4 && wcsncmp(name, L"post", 4) == 0) {
        if (depth < 1 || stack[depth - 1].kind != CTX_POST) goto fail;
        post_t *post = stack[depth - 1].post;
        if (!app_add_post(post)) {
          post_free(post);
          goto fail;
        }
        depth--;
        continue;
      }
    }
  }

  if (hr != S_OK && hr != S_FALSE) goto fail;
  ok = true;

fail:
  while (depth > 0) {
    switch (stack[depth - 1].kind) {
      case CTX_TEXT: break;
      case CTX_REPLY: comment_free(stack[depth - 1].reply); break;
      case CTX_COMMENT: comment_free(stack[depth - 1].comment); break;
      case CTX_BODY: break;
      case CTX_POST: post_free(stack[depth - 1].post); break;
      default: break;
    }
    depth--;
  }

done:
  if (reader) IXmlReader_Release(reader);
  if (stream) IStream_Release(stream);
  if (hglobal) GlobalFree(hglobal);
  free(tb.buf);
  free(bytes);
  return ok;
}
#endif

bool socialfeed_load_seed_data(const char *path) {
  if (!path) return false;

#ifdef _WIN32
  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  bool co_init = SUCCEEDED(hr);
  bool ok = socialfeed_load_seed_data_xmllite(path);
  if (co_init) CoUninitialize();
  return ok;
#else
  unsigned char *bytes = NULL;
  size_t size = 0;
  if (!read_seed_file(path, &bytes, &size)) return false;
  bool ok = parse_socialfeed_manual((const char *)bytes);
  free(bytes);
  return ok;
#endif
}
