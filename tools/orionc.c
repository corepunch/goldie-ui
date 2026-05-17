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

/* Window class defaults for redundant attribute optimization */
typedef struct {
  const char *class_name;
  int default_height;
  flags_t default_flags;
} class_defaults_t;

static const class_defaults_t kClassDefaults[] = {
  { "button",     19, 0 },
  { "label",      13, 0 },
  { "textedit",   13, 0 },
  { "checkbox",   13, 0 },
  { "combobox",   13, 0 },
  { "separator",   1, 0 },
  { "space",       0, WINDOW_FLEXSPACE },
  { "reportview", 100, WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_FLEXSPACE },
  { "tableview",  100, WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_FLEXSPACE },
  { "list",       100, WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE },
  { "multiedit",  100, WINDOW_VSCROLL | WINDOW_FLEXSPACE },
};

static const class_defaults_t *find_class_defaults(const char *class_name) {
  if (!class_name) return NULL;
  for (int i = 0; i < (int)(sizeof(kClassDefaults)/sizeof(kClassDefaults[0])); i++) {
    if (streq(kClassDefaults[i].class_name, class_name))
      return &kClassDefaults[i];
  }
  return NULL;
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
  
  /* frame= attribute is no longer supported - use width= and height= */
  char *w = attr_dup(node, "w");
  char *h = attr_dup(node, "h");
  if (!w) w = attr_dup(node, "width");
  if (!h) h = attr_dup(node, "height");
  
  if (w || h) {
    out->x = 0;
    out->y = 0;
    out->w = w ? atoi(w) : 0;
    out->h = h ? atoi(h) : 0;
    free(w); free(h);
    return true;
  }
  free(w); free(h);
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

static const enum_token_t kColorTokens[] = {
  {"transparent",         brTransparent},
  {"window-bg",           brWindowBg},
  {"window-dark-bg",      brWindowDarkBg},
  {"workspace-bg",        brWorkspaceBg},
  {"active-titlebar",     brActiveTitlebar},
  {"active-titlebar-text", brActiveTitlebarText},
  {"inactive-titlebar",   brInactiveTitlebar},
  {"inactive-titlebar-text", brInactiveTitlebarText},
  {"statusbar-bg",        brStatusbarBg},
  {"light-edge",          brLightEdge},
  {"dark-edge",           brDarkEdge},
  {"flare",               brFlare},
  {"focus-ring",          brFocusRing},
  {"button-bg",           brButtonBg},
  {"button-inner",        brButtonInner},
  {"button-hover",        brButtonHover},
  {"text-normal",         brTextNormal},
  {"text-disabled",       brTextDisabled},
  {"text-error",          brTextError},
  {"text-success",        brTextSuccess},
  {"border-focus",        brBorderFocus},
  {"border-active",       brBorderActive},
  {"folder-text",         brFolderText},
  {"column-view-bg",      brColumnViewBg},
  {"modal-overlay",       brModalOverlay},
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

static bool require_nonempty_attr(xmlNodePtr node, const char *elem_name,
                                  const char *attr_name, char **out) {
  char *v = attr_dup(node, attr_name);
  if (!v || !*v) {
    fprintf(stderr, "orionc: <%s> requires non-empty %s=\n", elem_name, attr_name);
    free(v);
    return false;
  }
  *out = v;
  return true;
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

static uint8_t color_attr(const char *v, uint8_t fallback) {
  return (uint8_t)enum_parse_token(v, kColorTokens, ARRAY_LEN(kColorTokens), fallback);
}

static const char *color_c_token(uint8_t color) {
  return enum_token_name(color, kColorTokens, ARRAY_LEN(kColorTokens), "text-normal");
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

// Generate tableview_params_t struct for a <tableview> element.
// Returns name of generated struct, or NULL on error. Caller must free.
static char *emit_tableview_params(FILE *f, xmlNodePtr tv, const char *form_ident,
                                   const char *tv_name, const char *prefix) {
  if (!is_element(tv, "tableview")) return NULL;
  
  char *db_attr = attr_dup(tv, "database");
  if (!db_attr || !*db_attr) {
    fprintf(stderr, "orionc: <tableview> requires database= attribute\n");
    free(db_attr);
    return NULL;
  }
  
  // Count columns
  int col_count = 0;
  for (xmlNodePtr col = tv->children; col; col = col->next) {
    if (is_element(col, "column")) col_count++;
  }
  if (col_count == 0) {
    fprintf(stderr, "orionc: <tableview> requires at least one <column>\n");
    free(db_attr);
    return NULL;
  }
  
  // Generate unique identifier for this tableview's params struct
  char params_ident[256];
  snprintf(params_ident, sizeof(params_ident), "%s_%s_tableview_params",
           form_ident, tv_name ? tv_name : "unnamed");
  
  // Emit field_names array
  fprintf(f, "static const char *%s_field_names[] = { ", params_ident);
  for (xmlNodePtr col = tv->children; col; col = col->next) {
    if (!is_element(col, "column")) continue;
    char *field = attr_dup(col, "field");
    if (field && *field) {
      fprint_c_string(f, field);
      fputs(", ", f);
    }
    free(field);
  }
  fputs("NULL };\n", f);
  
  // Emit column_titles array
  fprintf(f, "static const char *%s_column_titles[] = { ", params_ident);
  for (xmlNodePtr col = tv->children; col; col = col->next) {
    if (!is_element(col, "column")) continue;
    char *title = attr_dup(col, "title");
    if (title && *title) {
      fprint_c_string(f, title);
      fputs(", ", f);
    } else {
      fputs("\"\"", f);
      fputs(", ", f);
    }
    free(title);
  }
  fputs("NULL };\n", f);
  
  // Emit column_widths array
  fprintf(f, "static const int %s_column_widths[] = { ", params_ident);
  for (xmlNodePtr col = tv->children; col; col = col->next) {
    if (!is_element(col, "column")) continue;
    char *width_str = attr_dup(col, "width");
    int width = width_str ? atoi(width_str) : 0;
    fprintf(f, "%d, ", width);
    free(width_str);
  }
  fputs("-1 };\n", f);
  
  // Emit the tableview_params_t struct
  fprintf(f, "static const tableview_params_t %s = {\n", params_ident);
  fputs("  .db = NULL,\n", f);  // Set at runtime via app->db
  
  // Map database name to TABLE_* enum
  char table_enum[256];
  make_ident(table_enum, sizeof(table_enum), db_attr);
  for (char *p = table_enum; *p; p++)
    if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
  fprintf(f, "  .table_id = TABLE_%s,\n", table_enum);
  
  fputs("  .filter_field = 0,\n", f);  // Set at runtime via tvSetFilter
  fputs("  .filter_value = 0,\n", f);
  fprintf(f, "  .field_names = %s_field_names,\n", params_ident);
  fprintf(f, "  .column_titles = %s_column_titles,\n", params_ident);
  fprintf(f, "  .column_widths = %s_column_widths,\n", params_ident);
  fputs("};\n\n", f);
  
  free(db_attr);
  return strdup(params_ident);
}

// Forward declare emit_control_tree for recursion
static bool emit_control_tree(FILE *f, xmlNodePtr parent, const char *scope,
                              const char *form_ident, const char *parent_expr,
                              int *out_count);

// Walk tree and emit all tableview params before the children array
static bool emit_all_tableview_params(FILE *f, xmlNodePtr parent,
                                      const char *form_ident, const char *scope) {
  for (xmlNodePtr c = parent ? parent->children : NULL; c; c = c->next) {
    if (!is_control_node(c)) continue;
    
    char *klass = control_class_name(c);
    if (klass && strcmp(klass, "tableview") == 0) {
      char *name = attr_dup(c, "name");
      char *emitted_name = emit_tableview_params(f, c, form_ident, name, scope);
      free(name);
      free(emitted_name);
      if (!emitted_name) {
        free(klass);
        return false;
      }
    }
    free(klass);
    
    // Recurse into children
    if (has_child_controls(c)) {
      if (!emit_all_tableview_params(f, c, form_ident, scope)) {
        return false;
      }
    }
  }
  return true;
}

static bool emit_control_tree(FILE *f, xmlNodePtr parent, const char *scope,
                              const char *form_ident, const char *parent_expr,
                              int *out_count);

static bool emit_control_node(FILE *f, xmlNodePtr c, const char *scope,
                              const char *form_ident, const char *ident,
                              const char *parent_expr) {
  char *klass = control_class_name(c);
  char *name = attr_dup(c, "name");
  char *text = attr_dup(c, "text");
  char *cflags = attr_dup(c, "flags");
  char *h_align = attr_dup(c, "h-align");
  char *v_align = attr_dup(c, "v-align");
  if (!h_align) h_align = attr_dup(c, "h_align");
  if (!v_align) v_align = attr_dup(c, "v_align");
  char *layout_orientation = attr_dup_first(c, "orientation", "layout_orientation");
  char *layout_spacing = attr_dup_first(c, "spacing", "layout_spacing");
  char *padding = attr_dup_first(c, "padding", "layout_padding");
  char *margin = attr_dup_first(c, "margin", "layout_margin");
  char *font = attr_dup(c, "font");
  char *color = attr_dup(c, "color");
  frame_t cr = {0, 0, 0, 0};
  frame_t pad = {0, 0, 0, 0};
  frame_t mar = {0, 0, 0, 0};
  bool nested = has_child_controls(c);
  const char *emit_class = nonempty(klass, "");
  bool keep_nested_frame = !strcmp(emit_class, "column");

  if (!emit_class || !*emit_class) {
    free(klass); free(name); free(text); free(cflags);
    free(h_align); free(v_align);
    free(layout_orientation); free(layout_spacing);
    free(padding); free(margin); free(font); free(color);
    return false;
  }
  
  // Handle tableview - construct params name (already emitted by emit_all_tableview_params)
  char *tv_params_name = NULL;
  if (streq(emit_class, "tableview")) {
    char params_ident[256];
    snprintf(params_ident, sizeof(params_ident), "%s_%s_tableview_params",
             form_ident, name ? name : "unnamed");
    tv_params_name = strdup(params_ident);
  }

  /* All layout is auto now */
  if (!parse_frame(c, &cr)) {
    cr = (frame_t){0, 0, 0, 0};
  } else if (nested && !keep_nested_frame) {
    cr = (frame_t){0, 0, 0, 0};
  }

  if (!layout_orientation || !*layout_orientation)
    layout_orientation = strdup("vertical");
  if (!layout_spacing || !*layout_spacing) {
    free(layout_spacing);
    layout_spacing = strdup("4");
  }

  if (!parse_rect_attr(c, "padding", &pad))
    (void)parse_rect_attr(c, "layout_padding", &pad);
  if (!parse_rect_attr(c, "margin", &mar))
    (void)parse_rect_attr(c, "layout_margin", &mar);
  uint8_t font_val = FONT_SMALL;
  bool font_set = false;
  uint8_t color_val = brTextNormal;
  bool color_set = false;
  if (font && *font) {
    if (!strcmp(font, "system")) font_val = FONT_SYSTEM;
    else if (!strcmp(font, "small")) font_val = FONT_SMALL;
    else if (!strcmp(font, "icon")) font_val = FONT_ICON;
    font_set = true;
  }
  if (color && *color) {
    color_val = color_attr(color, brTextNormal);
    color_set = true;
  }

  bool horizontal = (layout_orientation_attr(layout_orientation, WINDOW_STACK_VERTICAL) & WINDOW_STACK_HORIZONTAL) != 0;

  fputs("  { ", f);
  fprint_c_string(f, emit_class);
  if (horizontal) {
    fprintf(f, ", %s, { %d, %d }, (%s) | WINDOW_STACK_HORIZONTAL, ",
            nonempty(ident, "0"), cr.w, cr.h, nonempty(cflags, "0"));
  } else {
    fprintf(f, ", %s, { %d, %d }, %s, ",
            nonempty(ident, "0"), cr.w, cr.h, nonempty(cflags, "0"));
  }
  fprint_c_string(f, nonempty(text, ""));
  fputs(", ", f);
  fprint_c_string(f, nonempty(name, ""));
  fprintf(f, ", %u, %u, ", (unsigned)align_h_attr(h_align, 0), (unsigned)align_v_attr(v_align, 0));
  fputs("NULL, 0", f);
  fprintf(f, ", %u, { %d, %d, %d, %d }, { %d, %d, %d, %d }, %s, %u, %s, %u, %s, ",
          (unsigned)layout_spacing_attr(layout_spacing, 4),
          pad.x, pad.y, pad.w, pad.h,
          mar.x, mar.y, mar.w, mar.h,
          nonempty(parent_expr, "0"),
          (unsigned)font_val,
          font_set ? "true" : "false",
          (unsigned)color_val,
          color_set ? "true" : "false");
  
  // Emit lparam (tableview params or NULL)
  if (tv_params_name) {
    fprintf(f, "&%s },\n", tv_params_name);
    free(tv_params_name);
  } else {
    fputs("NULL },\n", f);
  }

  free(klass); free(name); free(text); free(cflags);
  free(h_align); free(v_align);
  free(layout_orientation); free(layout_spacing);
  free(padding); free(margin); free(font); free(color);
  return true;
}

static bool emit_control_tree(FILE *f, xmlNodePtr parent, const char *scope,
                              const char *form_ident, const char *parent_expr,
                              int *out_count) {
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

    if (!emit_control_node(f, c, scope, form_ident, ident, parent_expr))
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
      if (!emit_control_tree(f, c, scope, form_ident, next_parent, &subcount)) {
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

static void menu_node_base(char *out, size_t out_sz,
                           const char *parent_base, xmlNodePtr node) {
  char *id = attr_dup(node, "name");
  char ident[128];
  make_upper_ident(ident, sizeof(ident), nonempty(id, "submenu"));
  snprintf(out, out_sz, "%s_%s", nonempty(parent_base, "MENU"), ident);
  free(id);
}

static bool emit_menu_item_array(FILE *f, xmlNodePtr menu,
                                 const char *base, const char *command_scope,
                                 bool is_mutable);

static bool emit_submenu_arrays(FILE *f, xmlNodePtr menu,
                                const char *parent_base, const char *command_scope,
                                bool is_mutable) {
  for (xmlNodePtr it = menu ? menu->children : NULL; it; it = it->next) {
    if (!is_element(it, "submenu")) continue;
    char child_base[256];
    menu_node_base(child_base, sizeof(child_base), parent_base, it);
    if (!emit_submenu_arrays(f, it, child_base, command_scope, is_mutable))
      return false;
    if (!emit_menu_item_array(f, it, child_base, command_scope, is_mutable))
      return false;
  }
  return true;
}

static bool emit_menu_item_array(FILE *f, xmlNodePtr menu,
                                 const char *base, const char *command_scope,
                                 bool is_mutable) {
  int item_count = count_menu_items(menu);
  if (item_count <= 0) return true;
  fprintf(f, "static %smenu_item_t %s_ITEMS[] = {\n",
          is_mutable ? "" : "const ", base);
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
      char child_base[256];
      char *label = attr_dup(it, "label");
      char *count = attr_dup(it, "count");
      bool dynamic = attr_is_true(it, "dynamic");
      int child_count = count_menu_items(it);
      menu_node_base(child_base, sizeof(child_base), base, it);
      fputs("  { ", f);
      fprint_c_string(f, nonempty(label, ""));
      if (dynamic && child_count <= 0) {
        fprintf(f, ", 0, NULL, %s },\n", nonempty(count, "0"));
      } else if (child_count <= 0) {
        fputs(", 0, NULL, 0 },\n", f);
      } else if (count && *count) {
        fprintf(f, ", 0, %s_ITEMS, %s },\n", child_base, count);
      } else {
        fprintf(f, ", 0, %s_ITEMS, (int)(sizeof(%s_ITEMS) / sizeof(%s_ITEMS[0])) },\n",
                child_base, child_base, child_base);
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
    char menu_base[256];
    make_upper_ident(menu_scope, sizeof(menu_scope), nonempty(mid, "menu"));
    snprintf(menu_base, sizeof(menu_base), "MENU_%s", menu_scope);
    free(mid);
    int item_count = count_menu_items(m);
    if (item_count <= 0) {
      continue;
    }
    bool is_mutable = attr_is_true(m, "mutable");
    if (!emit_submenu_arrays(f, m, menu_base, menu_scope, is_mutable) ||
        !emit_menu_item_array(f, m, menu_base, menu_scope, is_mutable)) {
      free(menus_var); free(count_var);
      return false;
    }
  }

  fprintf(f, "static menu_def_t %s[] = {\n", menu_array);
  for (xmlNodePtr m = menus->children; m; m = m->next) {
    if (!is_element(m, "menu")) continue;
    char *label = attr_dup(m, "label");
    char *count = attr_dup(m, "count");
    char *mid = attr_dup(m, "name");
    char menu_scope[128];
    char menu_base[256];
    make_upper_ident(menu_scope, sizeof(menu_scope), nonempty(mid, "menu"));
    snprintf(menu_base, sizeof(menu_base), "MENU_%s", menu_scope);
    bool dynamic = attr_is_true(m, "dynamic");
    int item_count = count_menu_items(m);

    fputs("  { ", f);
    fprint_c_string(f, nonempty(label, ""));
    if (dynamic && item_count <= 0) {
      fprintf(f, ", NULL, %s },\n", nonempty(count, "0"));
    } else if (count && *count) {
      fprintf(f, ", %s_ITEMS, %s },\n", menu_base, count);
    } else {
      fprintf(f, ", %s_ITEMS, (int)(sizeof(%s_ITEMS) / sizeof(%s_ITEMS[0])) },\n",
              menu_base, menu_base, menu_base);
    }
    free(label);
    free(count);
    free(mid);
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
    char *tbid = attr_dup(tb, "name");
    int item_count = count_toolbar_items(tb);
    if (item_count <= 0) {
      free(tbid);
      continue;
    }
    if (!tbid || !*tbid) {
      fprintf(stderr, "orionc: toolbar with items has no name\n");
      free(tbid);
      return false;
    }

    char toolbar_name[128];
    make_upper_ident(toolbar_name, sizeof(toolbar_name), nonempty(tbid, "toolbar"));
    fprintf(f, "static const toolbar_item_t TB_%s[] = {\n", toolbar_name);

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
          make_scoped_ident(ident, sizeof(ident), toolbar_name, name, text, -1);
        } else {
          make_scoped_ident(ident, sizeof(ident), toolbar_name, text, "item", -1);
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
    fprintf(f, "static const int TB_%s_COUNT = (int)(sizeof(TB_%s) / sizeof(TB_%s[0]));\n",
            toolbar_name, toolbar_name, toolbar_name);
    fputc('\n', f);
    free(tbid);
  }

  return true;
}

static const char *db_action_kind_c_token(const char *kind) {
  if (!kind || !*kind) return "DB_ACTION_CUSTOM";
  if (!strcmp(kind, "fetch")) return "DB_ACTION_FETCH";
  if (!strcmp(kind, "insert")) return "DB_ACTION_INSERT";
  if (!strcmp(kind, "update")) return "DB_ACTION_UPDATE";
  if (!strcmp(kind, "delete")) return "DB_ACTION_DELETE";
  return "DB_ACTION_CUSTOM";
}

static bool emit_database_resources(FILE *f, xmlNodePtr database, const char *prefix) {
  if (!database) return true;

  int table_count = 0;
  int source_count = 0;
  int binding_count = 0;
  int action_count = 0;

  for (xmlNodePtr n = database ? database->children : NULL; n; n = n->next) {
    if (is_element(n, "table")) table_count++;
    else if (is_element(n, "source")) source_count++;
    else if (is_element(n, "binding")) binding_count++;
    else if (is_element(n, "action")) action_count++;
  }
  if (table_count == 0 && source_count == 0 && binding_count == 0 && action_count == 0)
    return true;

  // Emit TABLE_ enums for each table
  if (table_count > 0) {
    fprintf(f, "// Table identifiers\n");
    fprintf(f, "enum {\n");
    int table_index = 0;
    for (xmlNodePtr n = database->children; n; n = n->next) {
      if (!is_element(n, "table")) continue;
      char *table_name = attr_dup(n, "name");
      if (table_name && *table_name) {
        char table_enum[128];
        make_upper_ident(table_enum, sizeof(table_enum), table_name);
        fprintf(f, "  TABLE_%s = %d,\n", table_enum, table_index);
        table_index++;
      }
      free(table_name);
    }
    fprintf(f, "  TABLE_COUNT = %d\n", table_index);
    fprintf(f, "};\n\n");
  }

  if (source_count > 0) {
    fprintf(f, "static const db_source_def_t %s_db_sources[] = {\n", prefix);
    for (xmlNodePtr n = database->children; n; n = n->next) {
      if (!is_element(n, "source")) continue;
      char *name = NULL;
      char *model = NULL;
      if (!require_nonempty_attr(n, "source", "name", &name) ||
          !require_nonempty_attr(n, "source", "model", &model)) {
        free(name);
        free(model);
        return false;
      }
      fputs("  { ", f);
      fprint_c_string(f, name);
      fputs(", ", f);
      fprint_c_string(f, model);
      fputs(" },\n", f);
      free(name);
      free(model);
    }
    fputs("};\n\n", f);
  }

  if (binding_count > 0) {
    int binding_index = 0;
    for (xmlNodePtr n = database->children; n; n = n->next) {
      if (!is_element(n, "binding")) continue;
      int col_count = 0;
      for (xmlNodePtr c = n->children; c; c = c->next)
        if (is_element(c, "column")) col_count++;
      if (col_count <= 0) {
        binding_index++;
        continue;
      }

      char *bname = NULL;
      char *source = NULL;
      char *view = NULL;
      if (!require_nonempty_attr(n, "binding", "name", &bname) ||
          !require_nonempty_attr(n, "binding", "source", &source) ||
          !require_nonempty_attr(n, "binding", "view", &view)) {
        free(bname);
        free(source);
        free(view);
        return false;
      }
      char bident[128];
      make_ident(bident, sizeof(bident), bname);
      fprintf(f, "static const db_binding_column_t %s_db_bind_%s_%d_cols[] = {\n",
              prefix, bident, binding_index);
      bool ok = true;
      for (xmlNodePtr c = n->children; c; c = c->next) {
        if (!is_element(c, "column")) continue;
        char *field = NULL;
        char *title = attr_dup(c, "title");
        char *width = attr_dup(c, "width");
        if (!require_nonempty_attr(c, "column", "field", &field)) {
          free(title);
          free(width);
          ok = false;
          break;
        }
        fputs("  { ", f);
        fprint_c_string(f, field);
        fputs(", ", f);
        fprint_c_string(f, nonempty(title, nonempty(field, "")));
        fprintf(f, ", %d },\n", width ? atoi(width) : 0);
        free(field);
        free(title);
        free(width);
      }
      fputs("};\n\n", f);
      free(bname);
      free(source);
      free(view);
      if (!ok) return false;
      binding_index++;
    }

    fprintf(f, "static const db_view_binding_t %s_db_bindings[] = {\n", prefix);
    binding_index = 0;
    for (xmlNodePtr n = database->children; n; n = n->next) {
      if (!is_element(n, "binding")) continue;
      int col_count = 0;
      for (xmlNodePtr c = n->children; c; c = c->next)
        if (is_element(c, "column")) col_count++;
      char *name = NULL;
      char *source = NULL;
      char *view = NULL;
      if (!require_nonempty_attr(n, "binding", "name", &name) ||
          !require_nonempty_attr(n, "binding", "source", &source) ||
          !require_nonempty_attr(n, "binding", "view", &view)) {
        free(name);
        free(source);
        free(view);
        return false;
      }
      char bident[128];
      make_ident(bident, sizeof(bident), name);
      fputs("  { ", f);
      fprint_c_string(f, name);
      fputs(", ", f);
      fprint_c_string(f, source);
      fputs(", ", f);
      fprint_c_string(f, view);
      if (col_count > 0) {
        fprintf(f, ", %s_db_bind_%s_%d_cols, %d },\n",
                prefix, bident, binding_index, col_count);
      } else {
        fputs(", NULL, 0 },\n", f);
      }
      free(name);
      free(source);
      free(view);
      binding_index++;
    }
    fputs("};\n\n", f);
  }

  if (action_count > 0) {
    fprintf(f, "static const db_action_def_t %s_db_actions[] = {\n", prefix);
    for (xmlNodePtr n = database->children; n; n = n->next) {
      if (!is_element(n, "action")) continue;
      char *name = NULL;
      char *kind = attr_dup_first(n, "kind", "type");
      char *source = NULL;
      char *target = NULL;
      if (!require_nonempty_attr(n, "action", "name", &name) ||
          !require_nonempty_attr(n, "action", "source", &source) ||
          !require_nonempty_attr(n, "action", "target", &target)) {
        free(name);
        free(kind);
        free(source);
        free(target);
        return false;
      }
      fputs("  { ", f);
      fprint_c_string(f, name);
      fprintf(f, ", %s, ", db_action_kind_c_token(kind));
      fprint_c_string(f, source);
      fputs(", ", f);
      fprint_c_string(f, target);
      fputs(" },\n", f);
      free(name);
      free(kind);
      free(source);
      free(target);
    }
    fputs("};\n\n", f);
  }

  fprintf(f, "static const db_api_def_t %s_database_api = { ", prefix);
  if (source_count > 0) fprintf(f, ".sources = %s_db_sources, ", prefix);
  else fputs(".sources = NULL, ", f);
  fprintf(f, ".source_count = %d, ", source_count);
  if (binding_count > 0) fprintf(f, ".bindings = %s_db_bindings, ", prefix);
  else fputs(".bindings = NULL, ", f);
  fprintf(f, ".binding_count = %d, ", binding_count);
  if (action_count > 0) fprintf(f, ".actions = %s_db_actions, ", prefix);
  else fputs(".actions = NULL, ", f);
  fprintf(f, ".action_count = %d };\n\n", action_count);

  return true;
}

static bool emit_form(FILE *f, xmlNodePtr form, const char *prefix) {
  char *id = attr_dup(form, "name");
  char *title = attr_dup(form, "title");
  char *flags = attr_dup(form, "flags");
  char *toolbar = attr_dup(form, "toolbar");  // Link to toolbar definition
  char *layout_spacing = attr_dup_first(form, "spacing", "layout_spacing");
  char *padding = attr_dup_first(form, "padding", "layout_padding");
  char *margin = attr_dup_first(form, "margin", "layout_margin");
  /* auto_layout is always true now */
  frame_t fr = {0, 0, 0, 0};
  frame_t pad = {0, 0, 0, 0};
  frame_t mar = {0, 0, 0, 0};
  if (!parse_frame(form, &fr)) {
    fprintf(stderr, "orionc: form '%s' requires width= attribute\n", nonempty(id, ""));
    free(id); free(title); free(flags); free(toolbar);
    free(layout_spacing);
    free(padding); free(margin);
    return false;
  }
  if (!parse_rect_attr(form, "padding", &pad))
    (void)parse_rect_attr(form, "layout_padding", &pad);
  if (!parse_rect_attr(form, "margin", &mar))
    (void)parse_rect_attr(form, "layout_margin", &mar);

  char id_ident[128];
  make_ident(id_ident, sizeof(id_ident), id);
  
  // First pass: emit all tableview params before the children array
  if (!emit_all_tableview_params(f, form, id_ident, id_ident)) {
    free(id); free(title); free(flags); free(toolbar);
    free(layout_spacing);
    free(padding); free(margin);
    return false;
  }
  
  fprintf(f, "static const form_ctrl_def_t %s_%s_children[] = {\n",
          prefix, id_ident);
  int child_count = 0;
  if (!emit_control_tree(f, form, id_ident, id_ident, "0", &child_count)) {
    free(id); free(title); free(flags); free(toolbar);
    free(layout_spacing);
    free(padding); free(margin);
    return false;
  }
  fprintf(f, "};\n\n");
  fprintf(f, "static const form_def_t %s_%s_form = { .name = ", prefix, id_ident);
  fprint_c_string(f, nonempty(title, nonempty(id, "")));
    fprintf(f, ", .width = %d, .height = %d, .flags = (%s) | WINDOW_AUTO_LAYOUT, ",
      fr.w, fr.h, nonempty(flags, "0"));
    fprintf(f, ".layout_spacing = %u, .padding = { %d, %d, %d, %d }, .margin = { %d, %d, %d, %d }, .children = %s_%s_children, .child_count = %d",
          (unsigned)layout_spacing_attr(layout_spacing, 4),
          pad.x, pad.y, pad.w, pad.h,
          mar.x, mar.y, mar.w, mar.h,
          prefix, id_ident, child_count);
  
  // Emit toolbar reference if specified
  if (toolbar && *toolbar) {
    char toolbar_ident[128];
    make_ident(toolbar_ident, sizeof(toolbar_ident), toolbar);
    for (char *p = toolbar_ident; *p; p++)
      if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    fprintf(f, ", .toolbar_items = TB_%s, .toolbar_count = TB_%s_COUNT", toolbar_ident, toolbar_ident);
  } else {
    fputs(", .toolbar_items = NULL, .toolbar_count = 0", f);
  }
  
  fputs(" };\n\n", f);

  free(id); free(title); free(flags); free(toolbar);
  free(layout_spacing);
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
  xmlNodePtr databases = first_child_element(root, "databases");
  xmlNodePtr database = databases ? first_child_element(databases, "database") : first_child_element(root, "database");
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
  if (!emit_database_resources(f, database, prefix_ident)) {
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
