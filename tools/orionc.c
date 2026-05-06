#include <libxml/parser.h>
#include <libxml/tree.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../user/user.h"
#include "../user/enum_parse.h"

#ifndef ARRAY_LEN
#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#endif

typedef struct {
  int x, y, w, h;
} frame_t;

typedef struct {
  char name[128];
  char value[64];
} define_t;

typedef struct {
  define_t items[512];
  int count;
} define_list_t;

typedef struct {
  char items[512][128];
  int count;
} ident_list_t;

static const char *base_name(const char *path) {
  const char *s = strrchr(path, '/');
  return s ? s + 1 : path;
}

static bool streq(const char *a, const char *b) {
  return a && b && strcmp(a, b) == 0;
}

static bool is_ident_expr(const char *s) {
  if (!s || !*s) return false;
  unsigned char c = (unsigned char)*s++;
  if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'))
    return false;
  while (*s) {
    c = (unsigned char)*s++;
    if (!((c >= 'A' && c <= 'Z') ||
          (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') ||
          c == '_'))
      return false;
  }
  return true;
}

static char *attr_dup(xmlNodePtr node, const char *name) {
  xmlChar *raw = xmlGetProp(node, BAD_CAST name);
  if (!raw) return NULL;
  char *s = strdup((const char *)raw);
  xmlFree(raw);
  return s;
}

static const char *nonempty(const char *s, const char *fallback) {
  return (s && *s) ? s : fallback;
}

static bool parse_frame(xmlNodePtr node, frame_t *out) {
  if (!node || !out) return false;
  char *frame = attr_dup(node, "frame");
  if (frame) {
    bool ok = sscanf(frame, "%d %d %d %d", &out->x, &out->y, &out->w, &out->h) == 4;
    free(frame);
    if (ok) return true;
  }

  char *x = attr_dup(node, "x");
  char *y = attr_dup(node, "y");
  char *w = attr_dup(node, "w");
  char *h = attr_dup(node, "h");
  if (!w) w = attr_dup(node, "width");
  if (!h) h = attr_dup(node, "height");
  if (x && y && w && h) {
    out->x = atoi(x);
    out->y = atoi(y);
    out->w = atoi(w);
    out->h = atoi(h);
    free(x); free(y); free(w); free(h);
    return true;
  }
  free(x); free(y); free(w); free(h);
  return false;
}

static bool parse_rect_attr(xmlNodePtr node, const char *name, frame_t *out) {
  if (!node || !name || !out) return false;
  char *v = attr_dup(node, name);
  if (!v) return false;
  int a = 0, b = 0, c = 0, d = 0;
  int n = sscanf(v, "%d %d %d %d", &a, &b, &c, &d);
  free(v);
  switch (n) {
    case 1:
      *out = (frame_t){a, a, a, a};
      return true;
    case 2:
      *out = (frame_t){a, b, a, b};
      return true;
    case 4:
      *out = (frame_t){a, b, c, d};
      return true;
    default:
      return false;
  }
}

static void fprint_c_string(FILE *f, const char *s) {
  fputc('"', f);
  if (s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
      switch (*p) {
        case '\\': fputs("\\\\", f); break;
        case '"':  fputs("\\\"", f); break;
        case '\n': fputs("\\n", f); break;
        case '\r': fputs("\\r", f); break;
        case '\t': fputs("\\t", f); break;
        default:
          if (*p < 0x20)
            fprintf(f, "\\x%02x", *p);
          else
            fputc(*p, f);
          break;
      }
    }
  }
  fputc('"', f);
}

static void fprint_c_string_with_shortcut(FILE *f, const char *label,
                                          const char *shortcut) {
  fputc('"', f);
  const char *parts[3] = { label, (shortcut && *shortcut) ? "\t" : NULL, shortcut };
  for (int i = 0; i < 3; i++) {
    const char *s = parts[i];
    if (!s) continue;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
      switch (*p) {
        case '\\': fputs("\\\\", f); break;
        case '"':  fputs("\\\"", f); break;
        case '\n': fputs("\\n", f); break;
        case '\r': fputs("\\r", f); break;
        case '\t': fputs("\\t", f); break;
        default:
          if (*p < 0x20)
            fprintf(f, "\\x%02x", *p);
          else
            fputc(*p, f);
          break;
      }
    }
  }
  fputc('"', f);
}

static void make_ident(char *out, size_t out_sz, const char *s) {
  size_t n = 0;
  if (!out || out_sz == 0) return;
  for (const unsigned char *p = (const unsigned char *)nonempty(s, "form");
       *p && n + 1 < out_sz; p++) {
    bool ok = (*p >= 'a' && *p <= 'z') ||
              (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9');
    out[n++] = ok ? (char)*p : '_';
  }
  if (n == 0) out[n++] = '_';
  out[n] = '\0';
}

static void make_upper_ident(char *out, size_t out_sz, const char *s) {
  make_ident(out, out_sz, s);
  for (char *p = out; p && *p; p++) {
    if (*p >= 'a' && *p <= 'z')
      *p = (char)(*p - 'a' + 'A');
  }
}

static void make_control_ident(char *out, size_t out_sz,
                               const char *form_ident,
                               const char *control_name,
                               const char *class_name,
                               int ordinal) {
  char form_buf[128];
  char name_buf[128];
  make_upper_ident(form_buf, sizeof(form_buf), nonempty(form_ident, "form"));
  make_upper_ident(name_buf, sizeof(name_buf), nonempty(control_name, nonempty(class_name, "control")));
  if (ordinal >= 0 && (!control_name || !*control_name)) {
    snprintf(out, out_sz, "ID_%s_%s%d", form_buf, name_buf, ordinal);
  } else {
    snprintf(out, out_sz, "ID_%s_%s", form_buf, name_buf);
  }
}

static void make_scoped_ident(char *out, size_t out_sz,
                              const char *scope_ident,
                              const char *name,
                              const char *fallback,
                              int ordinal) {
  char scope_buf[128];
  char name_buf[128];
  make_upper_ident(scope_buf, sizeof(scope_buf), nonempty(scope_ident, "scope"));
  make_upper_ident(name_buf, sizeof(name_buf), nonempty(name, nonempty(fallback, "item")));
  if (ordinal >= 0 && (!name || !*name)) {
    snprintf(out, out_sz, "ID_%s_%s%d", scope_buf, name_buf, ordinal);
  } else {
    snprintf(out, out_sz, "ID_%s_%s", scope_buf, name_buf);
  }
}

static bool is_element(xmlNodePtr node, const char *name) {
  return node && node->type == XML_ELEMENT_NODE &&
         xmlStrcmp(node->name, BAD_CAST name) == 0;
}

static xmlNodePtr first_child_element(xmlNodePtr node, const char *name) {
  for (xmlNodePtr c = node ? node->children : NULL; c; c = c->next)
    if (is_element(c, name)) return c;
  return NULL;
}

static bool attr_is_true(xmlNodePtr node, const char *name) {
  char *v = attr_dup(node, name);
  bool yes = v && (!strcmp(v, "true") || !strcmp(v, "1") || !strcmp(v, "yes"));
  free(v);
  return yes;
}

static const enum_token_t kAlignHTokens[] = {
  {"stretch", LAYOUT_ALIGN_STRETCH},
  {"left",    LAYOUT_ALIGN_START},
  {"start",   LAYOUT_ALIGN_START},
  {"center",  LAYOUT_ALIGN_CENTER},
  {"right",   LAYOUT_ALIGN_END},
  {"end",     LAYOUT_ALIGN_END},
};

static const enum_token_t kAlignVTokens[] = {
  {"stretch", LAYOUT_ALIGN_STRETCH},
  {"top",     LAYOUT_ALIGN_START},
  {"start",   LAYOUT_ALIGN_START},
  {"center",  LAYOUT_ALIGN_CENTER},
  {"bottom",  LAYOUT_ALIGN_END},
  {"end",     LAYOUT_ALIGN_END},
};

static const enum_token_t kLayoutOrientationTokens[] = {
  {"vertical",   WINDOW_STACK_VERTICAL},
  {"horizontal", WINDOW_STACK_HORIZONTAL},
};

static uint8_t align_h_attr(const char *v, uint8_t fallback) {
  return (uint8_t)enum_parse_token(v, kAlignHTokens, ARRAY_LEN(kAlignHTokens), fallback);
}

static uint8_t align_v_attr(const char *v, uint8_t fallback) {
  return (uint8_t)enum_parse_token(v, kAlignVTokens, ARRAY_LEN(kAlignVTokens), fallback);
}

static const char *align_h_token(uint8_t align) {
  return enum_token_name(align, kAlignHTokens, ARRAY_LEN(kAlignHTokens), "stretch");
}

static const char *align_v_token(uint8_t align) {
  return enum_token_name(align, kAlignVTokens, ARRAY_LEN(kAlignVTokens), "stretch");
}

static flags_t layout_orientation_attr(const char *v, flags_t fallback) {
  return (flags_t)enum_parse_token(v, kLayoutOrientationTokens, ARRAY_LEN(kLayoutOrientationTokens), (int)fallback);
}

static char *attr_dup_first(xmlNodePtr node, const char *a, const char *b) {
  char *v = attr_dup(node, a);
  if (v && *v) return v;
  free(v);
  return b ? attr_dup(node, b) : NULL;
}

static uint8_t layout_columns_attr(const char *v, uint8_t fallback) {
  if (!v || !*v) return fallback;
  char *end = NULL;
  long n = strtol(v, &end, 0);
  if (end && *end == '\0' && n >= 0 && n <= 255) return (uint8_t)n;
  return fallback;
}

static uint8_t layout_spacing_attr(const char *v, uint8_t fallback) {
  if (!v || !*v) return fallback;
  char *end = NULL;
  long n = strtol(v, &end, 0);
  if (end && *end == '\0' && n >= 0 && n <= 255) return (uint8_t)n;
  return fallback;
}

static const char *layout_orientation_c_token(flags_t orientation) {
  return enum_token_name(orientation, (const enum_token_t[]) {
    {"WINDOW_STACK_VERTICAL",   WINDOW_STACK_VERTICAL},
    {"WINDOW_STACK_HORIZONTAL", WINDOW_STACK_HORIZONTAL},
  }, 2, "WINDOW_STACK_VERTICAL");
}

static bool is_control_node(xmlNodePtr node) {
  return node && node->type == XML_ELEMENT_NODE && !is_element(node, "requires");
}

static char *control_class_name(xmlNodePtr node) {
  return strdup((const char *)node->name);
}

static bool has_child_controls(xmlNodePtr node) {
  for (xmlNodePtr c = node ? node->children : NULL; c; c = c->next) {
    if (!is_control_node(c)) continue;
    return true;
  }
  return false;
}

static const char *layout_kind_default_for_class(const char *klass) {
  if (!klass) return "none";
  if (!strcmp(klass, "stack") || !strcmp(klass, "stackview"))
    return "stack";
  if (!strcmp(klass, "grid") || !strcmp(klass, "gridview"))
    return "grid";
  return "none";
}

static const char *layout_kind_attr(const char *v, const char *fallback) {
  if (!v || !*v) return fallback;
  if (!strcmp(v, "none") || !strcmp(v, "stack") || !strcmp(v, "grid"))
    return v;
  return fallback;
}

static uint8_t layout_columns_default_for_class(const char *klass) {
  if (!klass) return 0;
  if (!strcmp(klass, "grid") || !strcmp(klass, "gridview"))
    return 2;
  return 0;
}

static bool emit_control_tree(FILE *f, xmlNodePtr parent, const char *scope,
                              const char *form_ident, const char *parent_expr,
                              bool allow_auto_layout, int *out_count);

static bool emit_control_node(FILE *f, xmlNodePtr c, const char *scope,
                              const char *ident, const char *parent_expr,
                              bool allow_auto_layout) {
  char *klass = control_class_name(c);
  char *name = attr_dup(c, "name");
  char *text = attr_dup(c, "text");
  char *cflags = attr_dup(c, "flags");
  char *h_align = attr_dup(c, "h-align");
  char *v_align = attr_dup(c, "v-align");
  if (!h_align) h_align = attr_dup(c, "h_align");
  if (!v_align) v_align = attr_dup(c, "v_align");
  char *layout_kind = attr_dup(c, "layout_kind");
  char *layout_orientation = attr_dup_first(c, "orientation", "layout_orientation");
  char *layout_columns = attr_dup_first(c, "columns", "layout_columns");
  char *layout_spacing = attr_dup_first(c, "spacing", "layout_spacing");
  char *padding = attr_dup_first(c, "padding", "layout_padding");
  char *margin = attr_dup_first(c, "margin", "layout_margin");
  frame_t cr = {0, 0, 0, 0};
  frame_t pad = {0, 0, 0, 0};
  frame_t mar = {0, 0, 0, 0};
  bool nested = has_child_controls(c);
  const char *emit_class = nonempty(klass, "");

  if (!emit_class || !*emit_class) {
    free(klass); free(name); free(text); free(cflags);
    free(h_align); free(v_align); free(layout_kind);
    free(layout_orientation); free(layout_columns); free(layout_spacing);
    free(padding); free(margin);
    return false;
  }

    if (!parse_frame(c, &cr)) {
      if (!allow_auto_layout && !nested) {
        fprintf(stderr, "orionc: control '%s' in form '%s' has no valid frame\n",
              nonempty(name, ""), nonempty(scope, ""));
      free(klass); free(name); free(text); free(cflags);
      free(h_align); free(v_align); free(layout_kind);
      free(layout_orientation); free(layout_columns); free(layout_spacing);
      free(padding); free(margin);
      return false;
    }
    cr = (frame_t){0, 0, 0, 0};
  }

  if (allow_auto_layout || nested) {
    if (!layout_kind || !*layout_kind) {
      free(layout_kind);
      layout_kind = strdup(layout_kind_default_for_class(emit_class));
    }
    if (!layout_orientation || !*layout_orientation)
      layout_orientation = strdup("vertical");
    if (!layout_columns || !*layout_columns) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%u",
               (unsigned)layout_columns_default_for_class(emit_class));
      free(layout_columns);
      layout_columns = strdup(buf);
    }
    if (!layout_spacing || !*layout_spacing) {
      free(layout_spacing);
      layout_spacing = strdup("4");
    }
  }

  if (!parse_rect_attr(c, "padding", &pad))
    (void)parse_rect_attr(c, "layout_padding", &pad);
  if (!parse_rect_attr(c, "margin", &mar))
    (void)parse_rect_attr(c, "layout_margin", &mar);

  fputs("  { ", f);
  fprint_c_string(f, emit_class);
  fprintf(f, ", %s, ", nonempty(ident, "0"));
  if (nested || !parse_frame(c, &cr)) {
    fprintf(f, "{ 0, 0, 0, 0 }, %s, ", nonempty(cflags, "0"));
  } else {
    fprintf(f, "{ %d, %d, %d, %d }, %s, ",
            cr.x, cr.y, cr.w, cr.h, nonempty(cflags, "0"));
  }
  fprint_c_string(f, nonempty(text, ""));
  fputs(", ", f);
  fprint_c_string(f, nonempty(name, ""));
  fprintf(f, ", %u, %u, ", (unsigned)align_h_attr(h_align, 0), (unsigned)align_v_attr(v_align, 0));
  fputs("NULL, 0, ", f);
  fprint_c_string(f, layout_kind_attr(layout_kind, layout_kind_default_for_class(emit_class)));
  fprintf(f, ", %s, %u, %u, { %d, %d, %d, %d }, { %d, %d, %d, %d }, %s },\n",
          layout_orientation_c_token(layout_orientation_attr(layout_orientation, WINDOW_STACK_VERTICAL)),
          (unsigned)layout_columns_attr(layout_columns, layout_columns_default_for_class(emit_class)),
          (unsigned)layout_spacing_attr(layout_spacing, 4),
          pad.x, pad.y, pad.w, pad.h,
          mar.x, mar.y, mar.w, mar.h,
          nonempty(parent_expr, "0"));

  free(klass); free(name); free(text); free(cflags);
  free(h_align); free(v_align); free(layout_kind);
  free(layout_orientation); free(layout_columns); free(layout_spacing);
  free(padding); free(margin);
  return true;
}

static bool emit_control_tree(FILE *f, xmlNodePtr parent, const char *scope,
                              const char *form_ident, const char *parent_expr,
                              bool allow_auto_layout, int *out_count) {
  int count = 0;
  int ordinal = 0;
  for (xmlNodePtr c = parent ? parent->children : NULL; c; c = c->next) {
    if (!is_control_node(c)) continue;
    char *name = attr_dup(c, "name");
    char *klass = control_class_name(c);
    char ident[256];
    make_control_ident(ident, sizeof(ident), form_ident, name, klass, ordinal);
    free(name);
    free(klass);

    if (!emit_control_node(f, c, scope, ident, parent_expr, allow_auto_layout))
      return false;
    count++;
    ordinal++;
    if (has_child_controls(c)) {
      char *cname = attr_dup(c, "name");
      char *cclass = control_class_name(c);
      char next_parent_buf[256];
      if (is_ident_expr(ident)) {
        snprintf(next_parent_buf, sizeof(next_parent_buf), "%s", ident);
      } else {
        make_control_ident(next_parent_buf, sizeof(next_parent_buf), form_ident,
                           cname, cclass, ordinal - 1);
      }
      const char *next_parent = next_parent_buf;
      int subcount = 0;
      if (!emit_control_tree(f, c, scope, form_ident, next_parent, true, &subcount)) {
        free(cname);
        free(cclass);
        return false;
      }
      count += subcount;
      free(cname);
      free(cclass);
    }
  }
  if (out_count) *out_count = count;
  return true;
}

static bool collect_define(define_list_t *defs, const char *name,
                           const char *value) {
  if (!defs || !is_ident_expr(name) || !value || !*value) return true;
  for (int i = 0; i < defs->count; i++) {
    if (!strcmp(defs->items[i].name, name)) {
      if (!strcmp(defs->items[i].value, value)) return true;
      fprintf(stderr, "orionc: conflicting values for id '%s' (%s vs %s)\n",
              name, defs->items[i].value, value);
      return false;
    }
  }
  if (defs->count >= (int)(sizeof(defs->items) / sizeof(defs->items[0]))) {
    fprintf(stderr, "orionc: too many generated id defines\n");
    return false;
  }
  snprintf(defs->items[defs->count].name, sizeof(defs->items[defs->count].name),
           "%s", name);
  snprintf(defs->items[defs->count].value, sizeof(defs->items[defs->count].value),
           "%s", value);
  defs->count++;
  return true;
}

static bool collect_ident(ident_list_t *ids, const char *name) {
  if (!ids || !is_ident_expr(name)) return true;
  if (!strcmp(name, "ID_OK") || !strcmp(name, "ID_CANCEL"))
    return true;
  for (int i = 0; i < ids->count; i++) {
    if (!strcmp(ids->items[i], name))
      return true;
  }
  if (ids->count >= (int)(sizeof(ids->items) / sizeof(ids->items[0]))) {
    fprintf(stderr, "orionc: too many generated control ids\n");
    return false;
  }
  snprintf(ids->items[ids->count], sizeof(ids->items[ids->count]), "%s", name);
  ids->count++;
  return true;
}

static bool collect_control_tree_idents(ident_list_t *ids, xmlNodePtr node,
                                        const char *form_ident) {
  int ordinal = 0;
  for (xmlNodePtr c = node ? node->children : NULL; c; c = c->next) {
    if (!is_control_node(c)) continue;
    char *cname = attr_dup(c, "name");
    char *cclass = control_class_name(c);
    char ident[256];
    make_control_ident(ident, sizeof(ident), form_ident, cname, cclass, ordinal);
    bool ok = collect_ident(ids, ident);
    free(cname);
    free(cclass);
    if (!ok) return false;
    if (!collect_control_tree_idents(ids, c, form_ident)) return false;
    ordinal++;
  }
  return true;
}

static bool collect_form_idents(ident_list_t *ids, xmlNodePtr form,
                                const char *form_ident) {
  return collect_control_tree_idents(ids, form, form_ident);
}

static void menu_scope_ident(char *out, size_t out_sz, xmlNodePtr node,
                             const char *parent_scope) {
  char *name = attr_dup(node, "name");
  const char *src = nonempty(name, "item");
  if (parent_scope && *parent_scope) {
    char scoped[128];
    make_upper_ident(scoped, sizeof(scoped), src);
    snprintf(out, out_sz, "%s_%s", parent_scope, scoped);
  } else {
    make_upper_ident(out, out_sz, src);
  }
  free(name);
}

static bool collect_menu_node_idents(ident_list_t *ids, xmlNodePtr menu,
                                     const char *scope) {
  for (xmlNodePtr it = menu ? menu->children : NULL; it; it = it->next) {
    if (is_element(it, "submenu")) {
      if (!collect_menu_node_idents(ids, it, scope)) return false;
      continue;
    }
    if (!is_element(it, "item")) continue;
    char *name = attr_dup(it, "name");
    char *label = attr_dup(it, "label");
    char ident[256];
    make_scoped_ident(ident, sizeof(ident), scope, name, label, -1);
    if (!collect_ident(ids, ident)) {
      free(name); free(label);
      return false;
    }
    free(name);
    free(label);
  }
  return true;
}

static bool collect_menu_idents(ident_list_t *ids, xmlNodePtr menus) {
  for (xmlNodePtr m = menus ? menus->children : NULL; m; m = m->next) {
    if (!is_element(m, "menu")) continue;
    char *mid = attr_dup(m, "name");
    char menu_ident[128];
    make_upper_ident(menu_ident, sizeof(menu_ident), nonempty(mid, "menu"));
    free(mid);
    if (!collect_menu_node_idents(ids, m, menu_ident)) return false;
  }
  return true;
}

static const char *toolbar_item_type_for_node(xmlNodePtr node);

static bool collect_toolbar_idents(ident_list_t *ids, xmlNodePtr toolbars) {
  for (xmlNodePtr tb = toolbars ? toolbars->children : NULL; tb; tb = tb->next) {
    if (!is_element(tb, "toolbar")) continue;
    char *tbid = attr_dup(tb, "name");
    char toolbar_ident[128];
    make_upper_ident(toolbar_ident, sizeof(toolbar_ident), nonempty(tbid, "toolbar"));
    free(tbid);
    for (xmlNodePtr it = tb->children; it; it = it->next) {
      if (!toolbar_item_type_for_node(it)) continue;
      if (is_element(it, "separator") || is_element(it, "spacer"))
        continue;
      char *menu = attr_dup(it, "menu");
      char *name = attr_dup(it, "name");
      char *id = NULL;
      char *label = attr_dup(it, "label");
      char ident[256];
      const char *scope = (menu && *menu) ? menu : toolbar_ident;
      if (menu && *menu) {
        char menu_scope[128];
        make_upper_ident(menu_scope, sizeof(menu_scope), menu);
        if (name && *name) {
          make_scoped_ident(ident, sizeof(ident), menu_scope, name, label, -1);
        } else {
          make_scoped_ident(ident, sizeof(ident), menu_scope, label, "item", -1);
        }
      } else {
        if (name && *name) {
          make_scoped_ident(ident, sizeof(ident), scope, name, label, -1);
        } else {
          make_scoped_ident(ident, sizeof(ident), scope, label, "item", -1);
        }
      }
      if (!collect_ident(ids, ident)) {
        free(menu); free(name); free(id); free(label);
        return false;
      }
      free(menu);
      free(name);
      free(id);
      free(label);
    }
  }
  return true;
}

static void emit_command_enums(FILE *f, const ident_list_t *ids) {
  if (!ids || ids->count <= 0) return;
  fputs("/* Menu and toolbar command IDs generated as symbolic enums. */\n", f);
  fputs("enum {\n", f);
  for (int i = 0; i < ids->count; i++) {
    if (i == 0)
      fprintf(f, "  %s = ID_COMMAND_BASE + 1%s\n", ids->items[i],
              (i + 1 < ids->count) ? "," : "");
    else
      fprintf(f, "  %s%s\n", ids->items[i], (i + 1 < ids->count) ? "," : "");
  }
  fputs("};\n\n", f);
}

static void emit_control_idents(FILE *f, const ident_list_t *ids) {
  if (!ids || ids->count <= 0) return;
  fputs("/* Control IDs generated as symbolic enums. */\n", f);
  fputs("enum {\n", f);
  for (int i = 0; i < ids->count; i++) {
    if (i == 0)
      fprintf(f, "  %s = ID_CONTROL_BASE + 1%s\n", ids->items[i],
              (i + 1 < ids->count) ? "," : "");
    else
      fprintf(f, "  %s%s\n", ids->items[i], (i + 1 < ids->count) ? "," : "");
  }
  fputs("};\n\n", f);
}

static int count_menu_items(xmlNodePtr menu) {
  int n = 0;
  for (xmlNodePtr it = menu ? menu->children : NULL; it; it = it->next)
    if (is_element(it, "item") || is_element(it, "separator") ||
        is_element(it, "submenu")) n++;
  return n;
}

static void emit_optional_if(FILE *f, xmlNodePtr node) {
  char *expr = attr_dup(node, "if");
  if (expr && *expr)
    fprintf(f, "#if %s\n", expr);
  free(expr);
}

static void emit_optional_endif(FILE *f, xmlNodePtr node) {
  char *expr = attr_dup(node, "if");
  if (expr && *expr)
    fputs("#endif\n", f);
  free(expr);
}

static void emit_menu_indices(FILE *f, xmlNodePtr menus) {
  if (!menus) return;
  int idx = 0;
  bool emitted = false;
  for (xmlNodePtr m = menus->children; m; m = m->next) {
    if (!is_element(m, "menu")) continue;
    char *id = attr_dup(m, "name");
    if (is_ident_expr(id)) {
      if (!emitted) {
        fputs("/* Top-level menu indices generated from <menu> order. */\n", f);
        fputs("enum {\n", f);
        emitted = true;
      }
      char ident[128];
      make_ident(ident, sizeof(ident), id);
      for (char *p = ident; *p; p++)
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
      fprintf(f, "  MENU_%s_INDEX = %d,\n", ident, idx);
    }
    free(id);
    idx++;
  }
  if (emitted)
    fputs("};\n\n", f);
}

static void menu_child_var(char *out, size_t out_sz,
                           const char *parent_var, xmlNodePtr node) {
  char *var = attr_dup(node, "var");
  if (var && *var) {
    snprintf(out, out_sz, "%s", var);
    free(var);
    return;
  }
  free(var);

  char *id = attr_dup(node, "name");
  char *label = attr_dup(node, "label");
  char ident[128];
  make_ident(ident, sizeof(ident), nonempty(id, nonempty(label, "submenu")));
  snprintf(out, out_sz, "%s_%s", nonempty(parent_var, "kMenu"), ident);
  free(id);
  free(label);
}

static bool emit_menu_item_array(FILE *f, xmlNodePtr menu,
                                 const char *var, const char *command_scope,
                                 bool is_mutable);

static bool emit_submenu_arrays(FILE *f, xmlNodePtr menu,
                                const char *parent_var, const char *command_scope,
                                bool is_mutable) {
  for (xmlNodePtr it = menu ? menu->children : NULL; it; it = it->next) {
    if (!is_element(it, "submenu")) continue;
    char child_var[256];
    menu_child_var(child_var, sizeof(child_var), parent_var, it);
    if (!emit_submenu_arrays(f, it, child_var, command_scope, is_mutable))
      return false;
    if (!emit_menu_item_array(f, it, child_var, command_scope, is_mutable))
      return false;
  }
  return true;
}

static bool emit_menu_item_array(FILE *f, xmlNodePtr menu,
                                 const char *var, const char *command_scope,
                                 bool is_mutable) {
  int item_count = count_menu_items(menu);
  if (item_count <= 0) return true;
  fprintf(f, "static %smenu_item_t %s[] = {\n",
          is_mutable ? "" : "const ", var);
  char *menu_id = attr_dup(menu, "name");
  char menu_scope[128];
  make_upper_ident(menu_scope, sizeof(menu_scope), nonempty(command_scope, nonempty(menu_id, "menu")));
  for (xmlNodePtr it = menu->children; it; it = it->next) {
    emit_optional_if(f, it);
    if (is_element(it, "separator")) {
      fputs("  { NULL, 0, NULL, 0 },\n", f);
      emit_optional_endif(f, it);
      continue;
    }
    if (is_element(it, "submenu")) {
      char child_var[256];
      char *label = attr_dup(it, "label");
      char *count = attr_dup(it, "count");
      bool dynamic = attr_is_true(it, "dynamic");
      int child_count = count_menu_items(it);
      menu_child_var(child_var, sizeof(child_var), var, it);
      fputs("  { ", f);
      fprint_c_string(f, nonempty(label, ""));
      if (dynamic && child_count <= 0) {
        fprintf(f, ", 0, NULL, %s },\n", nonempty(count, "0"));
      } else if (child_count <= 0) {
        fputs(", 0, NULL, 0 },\n", f);
      } else if (count && *count) {
        fprintf(f, ", 0, %s, %s },\n", child_var, count);
      } else {
        fprintf(f, ", 0, %s, (int)(sizeof(%s) / sizeof(%s[0])) },\n",
                child_var, child_var, child_var);
      }
      free(label);
      free(count);
      emit_optional_endif(f, it);
      continue;
    }
    if (!is_element(it, "item")) {
      emit_optional_endif(f, it);
      continue;
    }
    char *name = attr_dup(it, "name");
    char *id = NULL;
    char *label = attr_dup(it, "label");
    char *shortcut = attr_dup(it, "shortcut");
    char item_ident[256];
    if (name && *name) {
      make_scoped_ident(item_ident, sizeof(item_ident), menu_scope, name, label, -1);
    } else {
      make_scoped_ident(item_ident, sizeof(item_ident), menu_scope, label, "item", -1);
    }
    fputs("  { ", f);
    fprint_c_string_with_shortcut(f, nonempty(label, ""), shortcut);
    fprintf(f, ", %s, NULL, 0 },\n", item_ident);
    free(name);
    free(id);
    free(label);
    free(shortcut);
    emit_optional_endif(f, it);
  }
  fputs("};\n\n", f);
  free(menu_id);
  return true;
}

static bool emit_menu_resources(FILE *f, xmlNodePtr menus) {
  if (!menus) return true;
  char *menus_var = attr_dup(menus, "var");
  char *count_var = attr_dup(menus, "count");
  const char *menu_array = nonempty(menus_var, "kMenus");
  const char *menu_count = nonempty(count_var, "kNumMenus");

  for (xmlNodePtr m = menus->children; m; m = m->next) {
    if (!is_element(m, "menu")) continue;
    char *mid = attr_dup(m, "name");
    char menu_scope[128];
    make_upper_ident(menu_scope, sizeof(menu_scope), nonempty(mid, "menu"));
    free(mid);
    char *var = attr_dup(m, "var");
    int item_count = count_menu_items(m);
    if (item_count <= 0) {
      free(var);
      continue;
    }
    if (!var || !*var) {
      fprintf(stderr, "orionc: menu with items has no var\n");
      free(var); free(menus_var); free(count_var);
      return false;
    }
    bool is_mutable = attr_is_true(m, "mutable");
    if (!emit_submenu_arrays(f, m, var, menu_scope, is_mutable) ||
        !emit_menu_item_array(f, m, var, menu_scope, is_mutable)) {
      free(var); free(menus_var); free(count_var);
      return false;
    }
    free(var);
  }

  fprintf(f, "static menu_def_t %s[] = {\n", menu_array);
  for (xmlNodePtr m = menus->children; m; m = m->next) {
    if (!is_element(m, "menu")) continue;
    char *label = attr_dup(m, "label");
    char *var = attr_dup(m, "var");
    char *count = attr_dup(m, "count");
    bool dynamic = attr_is_true(m, "dynamic");
    int item_count = count_menu_items(m);

    fputs("  { ", f);
    fprint_c_string(f, nonempty(label, ""));
    if (dynamic && item_count <= 0) {
      fprintf(f, ", NULL, %s },\n", nonempty(count, "0"));
    } else if (count && *count) {
      fprintf(f, ", %s, %s },\n", nonempty(var, "NULL"), count);
    } else {
      fprintf(f, ", %s, (int)(sizeof(%s) / sizeof(%s[0])) },\n",
              nonempty(var, "NULL"), nonempty(var, "NULL"), nonempty(var, "NULL"));
    }
    free(label);
    free(var);
    free(count);
  }
  fputs("};\n", f);
  fprintf(f, "static const int %s = (int)(sizeof(%s) / sizeof(%s[0]));\n\n",
          menu_count, menu_array, menu_array);

  free(menus_var);
  free(count_var);
  return true;
}

static const char *toolbar_item_type_for_node(xmlNodePtr node) {
  if (is_element(node, "button")) return "TOOLBAR_ITEM_BUTTON";
  if (is_element(node, "label")) return "TOOLBAR_ITEM_LABEL";
  if (is_element(node, "combobox")) return "TOOLBAR_ITEM_COMBOBOX";
  if (is_element(node, "textedit")) return "TOOLBAR_ITEM_TEXTEDIT";
  if (is_element(node, "separator")) return "TOOLBAR_ITEM_SEPARATOR";
  if (is_element(node, "spacer")) return "TOOLBAR_ITEM_SPACER";
  return NULL;
}

static int count_toolbar_items(xmlNodePtr toolbar) {
  int n = 0;
  for (xmlNodePtr it = toolbar ? toolbar->children : NULL; it; it = it->next)
    if (toolbar_item_type_for_node(it)) n++;
  return n;
}

static bool emit_toolbar_resources(FILE *f, xmlNodePtr toolbars) {
  if (!toolbars) return true;

  for (xmlNodePtr tb = toolbars->children; tb; tb = tb->next) {
    if (!is_element(tb, "toolbar")) continue;
    char *var = attr_dup(tb, "var");
    char *count = attr_dup(tb, "count");
    int item_count = count_toolbar_items(tb);
    if (item_count <= 0) {
      free(var);
      free(count);
      continue;
    }
    if (!var || !*var) {
      fprintf(stderr, "orionc: toolbar with items has no var\n");
      free(var);
      free(count);
      return false;
    }

    fprintf(f, "static const toolbar_item_t %s[] = {\n", var);
    char *tbid = attr_dup(tb, "name");
    char toolbar_scope[128];
    make_upper_ident(toolbar_scope, sizeof(toolbar_scope), nonempty(tbid, "toolbar"));
    for (xmlNodePtr it = tb->children; it; it = it->next) {
      const char *type = toolbar_item_type_for_node(it);
      if (!type) continue;

      char *menu = attr_dup(it, "menu");
      char *name = attr_dup(it, "name");
      char *id = NULL;
      char *icon = attr_dup(it, "icon");
      char *w = attr_dup(it, "w");
      char *flags = attr_dup(it, "flags");
      char *text = attr_dup(it, "text");
      char ident[256];
      if (menu && *menu) {
        char menu_scope[128];
        make_upper_ident(menu_scope, sizeof(menu_scope), menu);
        if (name && *name) {
          make_scoped_ident(ident, sizeof(ident), menu_scope, name, text, -1);
        } else {
          make_scoped_ident(ident, sizeof(ident), menu_scope, text, "item", -1);
        }
      } else if (is_element(it, "separator") || is_element(it, "spacer")) {
        snprintf(ident, sizeof(ident), "0");
      } else {
        if (name && *name) {
          make_scoped_ident(ident, sizeof(ident), toolbar_scope, name, text, -1);
        } else {
          make_scoped_ident(ident, sizeof(ident), toolbar_scope, text, "item", -1);
        }
      }
      fprintf(f, "  { %s, %s, %s, %s, %s, ",
              type,
              nonempty(ident, "0"),
              nonempty(icon, "-1"),
              nonempty(w, "0"),
              nonempty(flags, "0"));
      if (text && *text)
        fprint_c_string(f, text);
      else
        fputs("NULL", f);
      fputs(" },\n", f);
      free(menu);
      free(name);
      free(id);
      free(icon);
      free(w);
      free(flags);
      free(text);
    }
    fputs("};\n", f);
    if (count && *count)
      fprintf(f, "static const int %s = (int)(sizeof(%s) / sizeof(%s[0]));\n",
              count, var, var);
    fputc('\n', f);
    free(tbid);
    free(var);
    free(count);
  }

  return true;
}

static bool emit_form(FILE *f, xmlNodePtr form, const char *prefix) {
  char *id = attr_dup(form, "name");
  char *title = attr_dup(form, "title");
  char *flags = attr_dup(form, "flags");
  char *layout_kind = attr_dup(form, "layout_kind");
  char *layout_orientation = attr_dup(form, "layout_orientation");
  char *layout_columns = attr_dup(form, "layout_columns");
  char *layout_spacing = attr_dup_first(form, "spacing", "layout_spacing");
  char *padding = attr_dup_first(form, "padding", "layout_padding");
  char *margin = attr_dup_first(form, "margin", "layout_margin");
  bool auto_layout = attr_is_true(form, "auto_layout");
  frame_t fr = {0, 0, 0, 0};
  frame_t pad = {0, 0, 0, 0};
  frame_t mar = {0, 0, 0, 0};
  if (!parse_frame(form, &fr)) {
    fprintf(stderr, "orionc: form '%s' has no valid frame\n", nonempty(id, ""));
    free(id); free(title); free(flags);
    free(layout_kind); free(layout_orientation); free(layout_columns); free(layout_spacing);
    free(padding); free(margin);
    return false;
  }
  if (!parse_rect_attr(form, "padding", &pad))
    (void)parse_rect_attr(form, "layout_padding", &pad);
  if (!parse_rect_attr(form, "margin", &mar))
    (void)parse_rect_attr(form, "layout_margin", &mar);

  char id_ident[128];
  make_ident(id_ident, sizeof(id_ident), id);
  fprintf(f, "static const form_ctrl_def_t %s_%s_children[] = {\n",
          prefix, id_ident);
  int child_count = 0;
  if (!emit_control_tree(f, form, id_ident, id_ident, "0", auto_layout, &child_count)) {
    free(id); free(title); free(flags);
    free(layout_kind); free(layout_orientation); free(layout_columns); free(layout_spacing);
    free(padding); free(margin);
    return false;
  }
  fprintf(f, "};\n\n");
  fprintf(f, "static const form_def_t %s_%s_form = {\n", prefix, id_ident);
  fputs("  .name = ", f);
  fprint_c_string(f, nonempty(title, nonempty(id, "")));
  fprintf(f, ",\n  .width = %d,\n  .height = %d,\n", fr.w, fr.h);
  fprintf(f, "  .flags = %s,\n", nonempty(flags, "0"));
  fprintf(f, "  .auto_layout = %s,\n", auto_layout ? "true" : "false");
  fprintf(f, "  .layout_kind = ");
  fprint_c_string(f, layout_kind_attr(layout_kind, "none"));
  fputs(",\n", f);
  fprintf(f, "  .layout_orientation = %s,\n",
          layout_orientation_c_token(layout_orientation_attr(layout_orientation, WINDOW_STACK_VERTICAL)));
  fprintf(f, "  .layout_columns = %u,\n",
          (unsigned)layout_columns_attr(layout_columns, 0));
  fprintf(f, "  .layout_spacing = %u,\n",
          (unsigned)layout_spacing_attr(layout_spacing, 4));
  fprintf(f, "  .padding = { %d, %d, %d, %d },\n", pad.x, pad.y, pad.w, pad.h);
  fprintf(f, "  .margin = { %d, %d, %d, %d },\n", mar.x, mar.y, mar.w, mar.h);
  fprintf(f, "  .children = %s_%s_children,\n", prefix, id_ident);
  fprintf(f, "  .child_count = %d,\n", child_count);
  fputs("};\n\n", f);

  free(id); free(title); free(flags);
  free(layout_kind); free(layout_orientation); free(layout_columns); free(layout_spacing);
  free(padding); free(margin);
  return true;
}

static void usage(const char *argv0) {
  fprintf(stderr,
          "usage: %s --input file.orion --output forms.h --prefix name [--form id]\n",
          base_name(argv0));
}

int main(int argc, char **argv) {
  const char *input = NULL;
  const char *output = NULL;
  const char *prefix = "orion";
  const char *only_form = NULL;

  for (int i = 1; i < argc; i++) {
    if (streq(argv[i], "--input") && i + 1 < argc) input = argv[++i];
    else if (streq(argv[i], "--output") && i + 1 < argc) output = argv[++i];
    else if (streq(argv[i], "--prefix") && i + 1 < argc) prefix = argv[++i];
    else if (streq(argv[i], "--form") && i + 1 < argc) only_form = argv[++i];
    else {
      usage(argv[0]);
      return 2;
    }
  }
  if (!input || !output) {
    usage(argv[0]);
    return 2;
  }

  char prefix_ident[128];
  make_ident(prefix_ident, sizeof(prefix_ident), prefix);

  xmlDocPtr doc = xmlReadFile(input, NULL, XML_PARSE_NONET);
  if (!doc) {
    fprintf(stderr, "orionc: failed to read %s\n", input);
    return 1;
  }
  xmlNodePtr root = xmlDocGetRootElement(doc);
  if (!is_element(root, "orion")) {
    fprintf(stderr, "orionc: %s is not an <orion> document\n", input);
    xmlFreeDoc(doc);
    return 1;
  }

  FILE *f = fopen(output, "wb");
  if (!f) {
    perror(output);
    xmlFreeDoc(doc);
    return 1;
  }

  char guard[256];
  make_ident(guard, sizeof(guard), output);
  for (char *p = guard; *p; p++)
    if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');

  fprintf(f, "/* Generated by orionc from %s. */\n", input);
  fprintf(f, "#ifndef %s\n#define %s\n\n", guard, guard);
  fputs("#include \"ui.h\"\n\n", f);

  int emitted = 0;
  ident_list_t control_ids = {0};
  ident_list_t command_ids = {0};
  xmlNodePtr menus = first_child_element(root, "menus");
  xmlNodePtr toolbars = first_child_element(root, "toolbars");
  xmlNodePtr forms = first_child_element(root, "forms");

  if (!collect_menu_idents(&command_ids, menus)) {
    fclose(f);
    xmlFreeDoc(doc);
    return 1;
  }
  if (!collect_toolbar_idents(&command_ids, toolbars)) {
    fclose(f);
    xmlFreeDoc(doc);
    return 1;
  }

  for (xmlNodePtr n = forms ? forms->children : NULL; n; n = n->next) {
    if (!is_element(n, "form")) continue;
    char *id = attr_dup(n, "name");
    bool want = !only_form || streq(id, only_form);
    free(id);
    if (!want) continue;
    char *form_id = attr_dup(n, "name");
    char form_ident[128];
    make_ident(form_ident, sizeof(form_ident), nonempty(form_id, "form"));
    if (!collect_form_idents(&control_ids, n, form_ident)) {
      free(form_id);
      fclose(f);
      xmlFreeDoc(doc);
      return 1;
    }
    free(form_id);
  }
  emit_command_enums(f, &command_ids);
  emit_control_idents(f, &control_ids);
  emit_menu_indices(f, menus);
  if (!emit_menu_resources(f, menus)) {
    fclose(f);
    xmlFreeDoc(doc);
    return 1;
  }
  if (!emit_toolbar_resources(f, toolbars)) {
    fclose(f);
    xmlFreeDoc(doc);
    return 1;
  }

  for (xmlNodePtr n = forms ? forms->children : NULL; n; n = n->next) {
    if (!is_element(n, "form")) continue;
    char *id = attr_dup(n, "name");
    bool want = !only_form || streq(id, only_form);
    free(id);
    if (!want) continue;
    if (!emit_form(f, n, prefix_ident)) {
      fclose(f);
      xmlFreeDoc(doc);
      return 1;
    }
    emitted++;
  }

  fprintf(f, "#endif /* %s */\n", guard);
  fclose(f);
  xmlFreeDoc(doc);

  if (emitted == 0) {
    fprintf(stderr, "orionc: no forms emitted from %s\n", input);
    return 1;
  }
  return 0;
}
