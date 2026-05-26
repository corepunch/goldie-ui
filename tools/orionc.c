#include <libxml/parser.h>
#include <libxml/tree.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../user/enum_parse.h"
#include "../user/user.h"

#define ORIONC_MAX_IDS 512
#define ORIONC_MAX_IDENT 128
#define ORIONC_DEFAULT_SPACING 4
#define ORIONC_STRING_SIZE 512
#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define EACH_ELEMENT(n, parent) for (xmlNodePtr n = (parent) ? (parent)->children : NULL; n; n = n->next) if ((n)->type == XML_ELEMENT_NODE)
#define OUT(...) fprintf(f, __VA_ARGS__)
#define LINE(s) fputs((s), f)

typedef struct { int x, y, w, h; } rect_t;
typedef struct { const char *key, *value; } kv_t;
typedef struct { char name[ORIONC_MAX_IDENT]; } orion_id_t;
typedef struct { orion_id_t v[ORIONC_MAX_IDS]; int n; } ids_t;
typedef struct { char *v[10]; } attrs_t;
enum { A_NAME, A_TEXT, A_FLAGS, A_HA, A_VA, A_ORIENT, A_SPACING, A_FONT, A_COLOR, A_FIELD };
typedef struct { char ctrl[128], db[64], table[64], field[64], klass[64]; } binding_t;
typedef struct { binding_t v[128]; int n; char db[64], table[64]; } bindings_t;
typedef struct { char ok_id[256], cancel_id[256]; } button_ids_t;

typedef struct {
  const char *xml, *c, *db;
  int size;
} field_type_t;

static const field_type_t kFieldTypes[] = {
  { "integer", "int", "DB_TYPE_INT", 0 }, { "int", "int", "DB_TYPE_INT", 0 },
  { "string", "char", "DB_TYPE_STRING", 256 }, { "boolean", "bool", "DB_TYPE_BOOL", 0 },
  { "bool", "bool", "DB_TYPE_BOOL", 0 }, { "float", "float", "DB_TYPE_FLOAT", 0 },
  { "real", "float", "DB_TYPE_FLOAT", 0 }, { "double", "double", "DB_TYPE_DOUBLE", 0 },
};

static const enum_token_t kAlignH[] = {
  {"stretch", LAYOUT_ALIGN_STRETCH}, {"left", LAYOUT_ALIGN_START},
  {"start", LAYOUT_ALIGN_START}, {"center", LAYOUT_ALIGN_CENTER},
  {"right", LAYOUT_ALIGN_END}, {"end", LAYOUT_ALIGN_END},
};
static const enum_token_t kAlignV[] = {
  {"stretch", LAYOUT_ALIGN_STRETCH}, {"top", LAYOUT_ALIGN_START},
  {"start", LAYOUT_ALIGN_START}, {"center", LAYOUT_ALIGN_CENTER},
  {"bottom", LAYOUT_ALIGN_END}, {"end", LAYOUT_ALIGN_END},
};
static const enum_token_t kOrient[] = {
  {"vertical", WINDOW_STACK_VERTICAL}, {"horizontal", WINDOW_STACK_HORIZONTAL},
};
static const enum_token_t kColors[] = {
  {"transparent", brTransparent}, {"window-bg", brWindowBg},
  {"window-dark-bg", brWindowDarkBg}, {"workspace-bg", brWorkspaceBg},
  {"active-titlebar", brActiveTitlebar}, {"active-titlebar-text", brActiveTitlebarText},
  {"inactive-titlebar", brInactiveTitlebar}, {"inactive-titlebar-text", brInactiveTitlebarText},
  {"statusbar-bg", brStatusbarBg}, {"light-edge", brLightEdge},
  {"dark-edge", brDarkEdge}, {"flare", brFlare}, {"focus-ring", brFocusRing},
  {"button-bg", brButtonBg}, {"button-inner", brButtonInner},
  {"button-hover", brButtonHover}, {"text-normal", brTextNormal},
  {"text-disabled", brTextDisabled}, {"text-error", brTextError},
  {"text-success", brTextSuccess}, {"border-focus", brBorderFocus},
  {"border-active", brBorderActive}, {"folder-text", brFolderText},
  {"column-view-bg", brColumnViewBg}, {"modal-overlay", brModalOverlay},
};

static bool eq(const char *a, const char *b) { return a && b && strcmp(a, b) == 0; }
static const char *nz(const char *s, const char *d) { return (s && *s) ? s : d; }
static bool g_orionc_failed = false;

typedef struct {
  const char *token;
  const char *expr;
} flag_token_t;

static const flag_token_t kShortFlagTokens[] = {
  {"autolayout", "WINDOW_AUTO_LAYOUT"},
  {"stackh", "WINDOW_STACK_HORIZONTAL"},
  {"stackv", "WINDOW_STACK_VERTICAL"},
  {"flexspace", "WINDOW_FLEXSPACE"},
  {"vscroll", "WINDOW_VSCROLL"},
  {"hscroll", "WINDOW_HSCROLL"},
  {"notitle", "WINDOW_NOTITLE"},
  {"nofill", "WINDOW_NOFILL"},
  {"dialog", "WINDOW_DIALOG"},
  {"toolbar", "WINDOW_TOOLBAR"},
  {"statusbar", "WINDOW_STATUSBAR"},
  {"noresize", "WINDOW_NORESIZE"},
  {"notraybutton", "WINDOW_NOTRAYBUTTON"},
  {"notabstop", "WINDOW_NOTABSTOP"},
  {"default", "BUTTON_DEFAULT"},
  {"pushlike", "BUTTON_PUSHLIKE"},
};

static const char *orionc_flag_expr_from_token(const char *token) {
  if (!token || !*token)
    return NULL;
  for (int i = 0; i < ARRAY_LEN(kShortFlagTokens); i++) {
    if (strcasecmp(token, kShortFlagTokens[i].token) == 0)
      return kShortFlagTokens[i].expr;
  }
  return NULL;
}

static void orionc_append_flag_expr(char *dst, size_t cap, const char *expr) {
  if (!dst || cap == 0 || !expr || !*expr)
    return;
  if (strcmp(dst, "0") == 0)
    snprintf(dst, cap, "%s", expr);
  else
    snprintf(dst + strlen(dst), cap - strlen(dst), " | %s", expr);
}

static bool orionc_parse_short_flags(char *out, size_t cap, const char *raw, const char *context) {
  if (!out || cap == 0)
    return false;
  snprintf(out, cap, "0");
  if (!raw || !*raw)
    return true;

  char buf[256];
  snprintf(buf, sizeof(buf), "%s", raw);
  char *save = NULL;
  for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
    while (*tok == ' ' || *tok == '\t' || *tok == '\n' || *tok == '\r')
      tok++;
    size_t n = strlen(tok);
    while (n > 0 && (tok[n - 1] == ' ' || tok[n - 1] == '\t' || tok[n - 1] == '\n' || tok[n - 1] == '\r'))
      tok[--n] = '\0';
    if (!tok[0])
      continue;

    const char *expr = orionc_flag_expr_from_token(tok);
    if (!expr) {
      fprintf(stderr, "orionc_alt: unknown short flag token '%s' in %s\n", tok, nz(context, "flags"));
      g_orionc_failed = true;
      return false;
    }
    orionc_append_flag_expr(out, cap, expr);
  }

  return true;
}
static bool elem(xmlNodePtr n, const char *name) {
  return n && n->type == XML_ELEMENT_NODE && xmlStrcasecmp(n->name, BAD_CAST name) == 0;
}
static char *attr(xmlNodePtr n, const char *name) { xmlChar *r = xmlGetProp(n, BAD_CAST name); char *s = r ? strdup((char *)r) : NULL; if (r) xmlFree(r); return s; }
static char *attrs_first(xmlNodePtr n, const char *a, const char *b) { char *v = attr(n, a); if (v && *v) return v; free(v); return b ? attr(n, b) : NULL; }
static bool truthy(const char *s) {
  return s && (strcasecmp(s, "true") == 0 || strcasecmp(s, "yes") == 0 ||
               strcasecmp(s, "many") == 0 || strcmp(s, "1") == 0);
}
static bool attr_truthy(xmlNodePtr n, const char *name) {
  char *v = attr(n, name);
  bool ok = truthy(v);
  free(v);
  return ok;
}
static xmlNodePtr child(xmlNodePtr n, const char *name) { EACH_ELEMENT(c, n) if (elem(c, name)) return c; return NULL; }

static void ident(char *out, size_t cap, const char *s, bool upper) {
  size_t n = 0;
  for (const unsigned char *p = (const unsigned char *)nz(s, "item"); *p && n + 1 < cap; p++) {
    bool ok = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9');
    char c = ok ? (char)*p : '_';
    out[n++] = (upper && c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
  }
  if (!n) out[n++] = '_';
  out[n] = 0;
}

static bool ident_expr(const char *s) {
  if (!s || !((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_')) return false;
  for (s++; *s; s++) if (!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '_')) return false;
  return true;
}

static bool db_path(const char *s, char *db, size_t db_cap, char *table, size_t table_cap, char *field, size_t field_cap) {
  const char *a = s ? strchr(s, '.') : NULL, *b = a ? strchr(a + 1, '.') : NULL;
  if (!a || !b || !a[1] || !b[1]) return false;
  snprintf(db, db_cap, "%.*s", (int)(a - s), s);
  snprintf(table, table_cap, "%.*s", (int)(b - a - 1), a + 1);
  snprintf(field, field_cap, "%s", b + 1);
  return true;
}

static void scoped(char *out, size_t cap, const char *scope, const char *name, const char *fallback) {
  char s[ORIONC_MAX_IDENT], n[ORIONC_MAX_IDENT];
  ident(s, sizeof(s), nz(scope, "scope"), true);
  ident(n, sizeof(n), nz(name, nz(fallback, "item")), true);
  snprintf(out, cap, "ID_%s_%s", s, n);
}

static void control_id(char *out, size_t cap, const char *form, const char *name, const char *klass, int ordinal) {
  char fallback[ORIONC_MAX_IDENT];
  snprintf(fallback, sizeof(fallback), "%s%d", nz(klass, "control"), ordinal);
  scoped(out, cap, form, name, fallback);
}

static void cstr(char *out, size_t cap, const char *s) {
  size_t n = 0;
  out[n++] = '"';
  for (const unsigned char *p = (const unsigned char *)nz(s, ""); *p && n + 5 < cap; p++) {
    if (*p == '\\' || *p == '"') { out[n++] = '\\'; out[n++] = (char)*p; }
    else if (*p == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
    else if (*p == '\t') { out[n++] = '\\'; out[n++] = 't'; }
    else out[n++] = (char)*p;
  }
  out[n++] = '"';
  out[n] = 0;
}

static void cstr_shortcut(char *out, size_t cap, const char *label, const char *shortcut) {
  char joined[ORIONC_STRING_SIZE];
  snprintf(joined, sizeof(joined), "%s%s%s", nz(label, ""), (shortcut && *shortcut) ? "\t" : "", nz(shortcut, ""));
  cstr(out, cap, joined);
}

static void tpl(FILE *f, const char *s, const kv_t *kv) {
  for (const char *p = s; *p;) {
    const char *open = strstr(p, "{{");
    if (!open) { fputs(p, f); break; }
    fwrite(p, 1, (size_t)(open - p), f);
    const char *close = strstr(open + 2, "}}");
    if (!close) { fputs(open, f); break; }
    char key[64];
    snprintf(key, sizeof(key), "%.*s", (int)(close - open - 2), open + 2);
    for (int i = 0; kv[i].key; i++) if (eq(kv[i].key, key)) { fputs(kv[i].value, f); break; }
    p = close + 2;
  }
}

static bool add_id(ids_t *ids, const char *name) {
  if (!ident_expr(name) || eq(name, "ID_OK") || eq(name, "ID_CANCEL")) return true;
  for (int i = 0; i < ids->n; i++) if (eq(ids->v[i].name, name)) return true;
  if (ids->n >= ORIONC_MAX_IDS) return false;
  snprintf(ids->v[ids->n++].name, ORIONC_MAX_IDENT, "%s", name);
  return true;
}

static bool rect_attr(xmlNodePtr n, const char *name, rect_t *r) {
  char *s = attr(n, name);
  int a = 0, b = 0, c = 0, d = 0, count = s ? sscanf(s, "%d %d %d %d", &a, &b, &c, &d) : 0;
  free(s);
  if (count == 1) *r = (rect_t){a, a, a, a};
  else if (count == 2) *r = (rect_t){a, b, a, b};
  else if (count == 4) *r = (rect_t){a, b, c, d};
  return count == 1 || count == 2 || count == 4;
}

static rect_t size_attr(xmlNodePtr n) {
  char *w = attrs_first(n, "w", "width"), *h = attrs_first(n, "h", "height");
  rect_t r = {0, 0, w ? atoi(w) : 0, h ? atoi(h) : 0};
  free(w); free(h);
  return r;
}

static unsigned byte_attr(const char *s, unsigned def) {
  char *end = NULL; long v = s ? strtol(s, &end, 0) : def;
  return (s && end && !*end && v >= 0 && v <= 255) ? (unsigned)v : def;
}

static void read_control_attrs(xmlNodePtr n, attrs_t *a) {
  static const struct { int slot; const char *name, *alias; } cfg[] = {
    {A_NAME, "name", NULL}, {A_TEXT, "text", NULL}, {A_FLAGS, "flags", NULL},
    {A_HA, "h-align", "h_align"}, {A_VA, "v-align", "v_align"},
    {A_ORIENT, "orientation", "layout_orientation"}, {A_SPACING, "spacing", "layout_spacing"},
    {A_FONT, "font", NULL}, {A_COLOR, "color", NULL}, {A_FIELD, "field", NULL},
  };
  memset(a, 0, sizeof(*a));
  for (int i = 0; i < ARRAY_LEN(cfg); i++) a->v[cfg[i].slot] = attrs_first(n, cfg[i].name, cfg[i].alias);
}

static void free_attrs(attrs_t *a) { for (int i = 0; i < ARRAY_LEN(a->v); i++) free(a->v[i]); }
static bool is_control(xmlNodePtr parent, xmlNodePtr n) {
  return n && n->type == XML_ELEMENT_NODE && !elem(n, "requires");
}
static bool has_controls(xmlNodePtr n) { EACH_ELEMENT(c, n) if (is_control(n, c)) return true; return false; }

static bool report_column_node(xmlNodePtr parent, xmlNodePtr n) {
  return elem(n, "column") && (elem(parent, "reportview") || elem(parent, "tableview"));
}

static void emit_enum_ids(FILE *f, const ids_t *ids, const char *base) {
  if (ids->n <= 0)
    return;
  LINE("enum {\n");
  for (int i = 0; i < ids->n; i++)
    OUT("  %s = (%s + %d),\n", ids->v[i].name, base, i + 1);
  LINE("};\n\n");
}

static void collect_menu_ids(ids_t *ids, xmlNodePtr menu, const char *scope) {
  EACH_ELEMENT(it, menu) {
    if (elem(it, "submenu")) { collect_menu_ids(ids, it, scope); continue; }
    if (!elem(it, "item")) continue;
    char *name = attr(it, "name"), *label = attr(it, "label"), id[256];
    scoped(id, sizeof(id), scope, name, label);
    add_id(ids, id);
    free(name); free(label);
  }
}

static const char *toolbar_type(xmlNodePtr n) {
  if (elem(n, "button")) return "TOOLBAR_ITEM_BUTTON";
  if (elem(n, "label")) return "TOOLBAR_ITEM_LABEL";
  if (elem(n, "combobox")) return "TOOLBAR_ITEM_COMBOBOX";
  if (elem(n, "textedit")) return "TOOLBAR_ITEM_TEXTEDIT";
  if (elem(n, "separator")) return "TOOLBAR_ITEM_SEPARATOR";
  if (elem(n, "spacer")) return "TOOLBAR_ITEM_SPACER";
  return NULL;
}

static void collect_command_ids(ids_t *ids, xmlNodePtr menus, xmlNodePtr toolbars) {
  EACH_ELEMENT(m, menus) if (elem(m, "menu")) { char *name = attr(m, "name"), scope[128]; ident(scope, sizeof(scope), name, true); collect_menu_ids(ids, m, scope); free(name); }
  EACH_ELEMENT(tb, toolbars) if (elem(tb, "toolbar")) {
    char *tbid = attr(tb, "name"), tb_scope[128]; ident(tb_scope, sizeof(tb_scope), tbid, true);
    EACH_ELEMENT(it, tb) if (toolbar_type(it) && !elem(it, "separator") && !elem(it, "spacer")) {
      char *menu = attr(it, "menu"), *name = attr(it, "name"), *text = attr(it, "text"), id[256], scope[128];
      ident(scope, sizeof(scope), nz(menu, tb_scope), true);
      scoped(id, sizeof(id), scope, name, text);
      add_id(ids, id);
      free(menu); free(name); free(text);
    }
    free(tbid);
  }
}

static void collect_control_ids(ids_t *ids, xmlNodePtr parent, const char *form) {
  int ordinal = 0;
  EACH_ELEMENT(c, parent) if (is_control(parent, c)) {
    char *name = attr(c, "name"), id[256];
    control_id(id, sizeof(id), form, name, (char *)c->name, ordinal++);
    add_id(ids, id);
    collect_control_ids(ids, c, form);
    free(name);
  }
}

static int count_menu_items(xmlNodePtr menu) {
  int n = 0; EACH_ELEMENT(it, menu) if (elem(it, "item") || elem(it, "separator") || elem(it, "submenu")) n++; return n;
}

static void emit_if(FILE *f, xmlNodePtr n, bool end) {
  char *e = attr(n, "if");
  if (e && *e) OUT(end ? "#endif\n" : "#if %s\n", e);
  free(e);
}

static void menu_base(char *out, size_t cap, const char *parent, xmlNodePtr n) {
  char *name = attr(n, "name"), id[128]; ident(id, sizeof(id), name, true);
  snprintf(out, cap, "%s_%s", nz(parent, "MENU"), id);
  free(name);
}

static void emit_menu_items(FILE *f, xmlNodePtr menu, const char *base, const char *scope) {
  EACH_ELEMENT(it, menu) if (elem(it, "submenu")) { char b[256]; menu_base(b, sizeof(b), base, it); emit_menu_items(f, it, b, scope); }
  if (!count_menu_items(menu)) return;
  OUT("static const menu_item_t %s_ITEMS[] = {\n", base);
  EACH_ELEMENT(it, menu) {
    emit_if(f, it, false);
    if (elem(it, "separator")) LINE("  { NULL, 0, NULL, 0 },\n");
    else if (elem(it, "submenu")) {
      char b[256], label[ORIONC_STRING_SIZE], *raw = attr(it, "label");
      menu_base(b, sizeof(b), base, it); cstr(label, sizeof(label), raw);
      if (count_menu_items(it))
        OUT("  { %s, 0, %s_ITEMS, (int)(sizeof(%s_ITEMS) / sizeof(%s_ITEMS[0])) },\n", label, b, b, b);
      else
        OUT("  { %s, 0, NULL, 0 },\n", label);
      free(raw);
    } else if (elem(it, "item")) {
      char *name = attr(it, "name"), *raw = attr(it, "label"), *shortcut = attr(it, "shortcut");
      char id[256], label[ORIONC_STRING_SIZE]; scoped(id, sizeof(id), scope, name, raw); cstr_shortcut(label, sizeof(label), raw, shortcut);
      OUT("  { %s, %s, NULL, 0 },\n", label, id);
      free(name); free(raw); free(shortcut);
    }
    emit_if(f, it, true);
  }
  LINE("};\n\n");
}

static void emit_menus(FILE *f, xmlNodePtr menus) {
  if (!menus) return;
  EACH_ELEMENT(m, menus) if (elem(m, "menu")) { char *name = attr(m, "name"), scope[128], base[256]; ident(scope, sizeof(scope), name, true); snprintf(base, sizeof(base), "MENU_%s", scope); emit_menu_items(f, m, base, scope); free(name); }
  char *var = attrs_first(menus, "var", NULL), *count = attrs_first(menus, "count", NULL);
  OUT("static menu_def_t %s[] = {\n", nz(var, "kMenus"));
  EACH_ELEMENT(m, menus) if (elem(m, "menu")) {
    char *name = attr(m, "name"), *raw = attr(m, "label"), label[ORIONC_STRING_SIZE], scope[128], base[256];
    ident(scope, sizeof(scope), name, true); snprintf(base, sizeof(base), "MENU_%s", scope); cstr(label, sizeof(label), raw);
    OUT("  { %s, %s_ITEMS, (int)(sizeof(%s_ITEMS) / sizeof(%s_ITEMS[0])) },\n", label, base, base, base);
    free(name); free(raw);
  }
  OUT("};\n#define %s ((int)(sizeof(%s) / sizeof(%s[0])))\n\n", nz(count, "kNumMenus"), nz(var, "kMenus"), nz(var, "kMenus"));
  free(var); free(count);
}

static void emit_toolbars(FILE *f, xmlNodePtr toolbars) {
  EACH_ELEMENT(tb, toolbars) if (elem(tb, "toolbar")) {
    char *tbid = attr(tb, "name"), scope[128]; ident(scope, sizeof(scope), tbid, true);
    OUT("static const toolbar_item_t TB_%s[] = {\n", scope);
    EACH_ELEMENT(it, tb) if (toolbar_type(it)) {
      char *menu = attr(it, "menu"), *name = attr(it, "name"), *icon = attr(it, "icon"), *w = attr(it, "w"), *flags = attr(it, "flags"), *text = attr(it, "text");
      char id[256] = "0", textq[ORIONC_STRING_SIZE], menu_scope[128], item_flags[256];
      orionc_parse_short_flags(item_flags, sizeof(item_flags), flags, "toolbar item flags");
      if (!elem(it, "separator") && !elem(it, "spacer")) { ident(menu_scope, sizeof(menu_scope), nz(menu, scope), true); scoped(id, sizeof(id), menu_scope, name, text); }
      if (text && *text) cstr(textq, sizeof(textq), text); else snprintf(textq, sizeof(textq), "NULL");
      OUT("  { %s, %s, %s, %s, %s, %s },\n", toolbar_type(it), id, nz(icon, "-1"), nz(w, "0"), item_flags, textq);
      free(menu); free(name); free(icon); free(w); free(flags); free(text);
    }
    OUT("};\n#define TB_%s_COUNT ((int)(sizeof(TB_%s) / sizeof(TB_%s[0])))\n\n", scope, scope, scope);
    free(tbid);
  }
}

static const field_type_t *field_type(const char *type) { for (int i = 0; i < ARRAY_LEN(kFieldTypes); i++) if (eq(type, kFieldTypes[i].xml)) return &kFieldTypes[i]; return &kFieldTypes[0]; }
static void singular_type(char *out, size_t cap, const char *table, const char *model) {
  if (model && *model) { snprintf(out, cap, "%s", model); return; }
  char id[96]; ident(id, sizeof(id), table, false); size_t n = strlen(id); if (n > 1 && id[n - 1] == 's') id[n - 1] = 0;
  snprintf(out, cap, "db_%s_t", id);
}

static void singular_name(char *out, size_t cap, const char *name) {
  char id[96];
  ident(id, sizeof(id), name, false);
  size_t n = strlen(id);
  if (n > 1 && id[n - 1] == 's')
    id[n - 1] = 0;
  snprintf(out, cap, "%s", id);
}

static xmlNodePtr db_find_table_node(xmlNodePtr db, const char *table_name) {
  if (!db || !table_name || !*table_name)
    return NULL;
  EACH_ELEMENT(t, db) if (elem(t, "table")) {
    char *name = attr(t, "name");
    bool ok = name && strcmp(name, table_name) == 0;
    free(name);
    if (ok)
      return t;
  }
  return NULL;
}

static bool table_has_field_named(xmlNodePtr table, const char *field_name) {
  if (!table || !field_name || !*field_name)
    return false;
  EACH_ELEMENT(field, table) if (elem(field, "field")) {
    char *name = attr(field, "name");
    bool ok = name && strcmp(name, field_name) == 0;
    free(name);
    if (ok)
      return true;
  }
  return false;
}

static bool table_has_other_field_named(xmlNodePtr table, xmlNodePtr ignore, const char *field_name) {
  if (!table || !field_name || !*field_name)
    return false;
  EACH_ELEMENT(field, table) if (elem(field, "field") && field != ignore) {
    char *name = attr(field, "name");
    bool ok = name && strcmp(name, field_name) == 0;
    free(name);
    if (ok)
      return true;
  }
  return false;
}

static bool has_suffix(const char *s, const char *suffix) {
  if (!s || !suffix)
    return false;
  size_t n = strlen(s), m = strlen(suffix);
  return n >= m && strcmp(s + n - m, suffix) == 0;
}

static bool is_many_relationship_field(xmlNodePtr field) {
  char *kind = attr(field, "type");
  bool ok = eq(kind, "relationship") && attr_truthy(field, "many");
  free(kind);
  return ok;
}

static bool is_storage_field(xmlNodePtr field) {
  return !is_many_relationship_field(field);
}

static bool split_relation_ref(const char *relation, char *table, size_t table_cap, char *field, size_t field_cap) {
  if (!relation || !*relation)
    return false;
  const char *dot = strchr(relation, '.');
  if (dot) {
    if (dot == relation || !dot[1])
      return false;
    snprintf(table, table_cap, "%.*s", (int)(dot - relation), relation);
    snprintf(field, field_cap, "%s", dot + 1);
  } else {
    snprintf(table, table_cap, "%s", relation);
    snprintf(field, field_cap, "id");
  }
  return table[0] && field[0];
}

static void add_implicit_id_field(xmlNodePtr table) {
  if (!table || table_has_field_named(table, "id"))
    return;

  xmlNodePtr id = xmlNewNode(NULL, BAD_CAST "field");
  if (!id)
    return;
  xmlNewProp(id, BAD_CAST "name", BAD_CAST "id");
  xmlNewProp(id, BAD_CAST "type", BAD_CAST "integer");
  xmlNewProp(id, BAD_CAST "key", BAD_CAST "YES");

  EACH_ELEMENT(field, table) if (elem(field, "field")) {
    xmlAddPrevSibling(field, id);
    return;
  }
  xmlAddChild(table, id);
}

static void normalize_relationship_field(xmlNodePtr table, xmlNodePtr field, const char *table_name) {
  char *name = attr(field, "name");
  char *kind = attr(field, "type");
  char *relation = attr(field, "relation");
  char *alias = attr(field, "alias");
  bool many = attr_truthy(field, "many");
  if (!eq(kind, "relationship"))
    goto done;

  char rel_table[96] = {0}, rel_field[96] = {0};
  if (!split_relation_ref(relation, rel_table, sizeof(rel_table), rel_field, sizeof(rel_field))) {
    fprintf(stderr, "orionc_alt: table '%s' relationship field '%s' requires relation=\"table\" or relation=\"table.field\"\n",
            nz(table_name, "table"), nz(name, "field"));
    g_orionc_failed = true;
    goto done;
  }

  char relation_ref[192];
  snprintf(relation_ref, sizeof(relation_ref), "%s.%s", rel_table, rel_field);
  xmlSetProp(field, BAD_CAST "relation", BAD_CAST relation_ref);

  if (many)
    goto done;

  char fk_name[128];
  if (has_suffix(nz(name, "field"), "_id"))
    snprintf(fk_name, sizeof(fk_name), "%s", nz(name, "field"));
  else
    snprintf(fk_name, sizeof(fk_name), "%s_id", nz(name, "field"));

  if (table_has_other_field_named(table, field, fk_name)) {
    fprintf(stderr, "orionc_alt: table '%s' relationship field '%s' conflicts with local field '%s'\n",
            nz(table_name, "table"), nz(name, "field"), fk_name);
    g_orionc_failed = true;
    goto done;
  }

  xmlSetProp(field, BAD_CAST "name", BAD_CAST fk_name);
  xmlSetProp(field, BAD_CAST "type", BAD_CAST "integer");
  if (!alias || !*alias)
    xmlSetProp(field, BAD_CAST "alias", BAD_CAST nz(name, "field"));

done:
  free(name);
  free(kind);
  free(relation);
  free(alias);
}

static void normalize_database_schema(xmlNodePtr db) {
  if (!db)
    return;
  EACH_ELEMENT(t, db) if (elem(t, "table")) {
    char *table_name = attr(t, "name");
    add_implicit_id_field(t);
    EACH_ELEMENT(field, t) if (elem(field, "field"))
      normalize_relationship_field(t, field, table_name);
    free(table_name);
  }
}

static bool join_alias_field_available(xmlNodePtr table, const char *table_name, const char *alias, const char *foreign_field) {
  char alias_field[128];
  snprintf(alias_field, sizeof(alias_field), "%s_%s", nz(alias, "alias"), nz(foreign_field, "field"));
  if (!table_has_field_named(table, alias_field))
    return true;
  fprintf(stderr,
          "orionc_alt: table '%s' relation alias '%s' field '%s' conflicts with local field '%s'\n",
          nz(table_name, "table"), nz(alias, "alias"), nz(foreign_field, "field"), alias_field);
  g_orionc_failed = true;
  return false;
}

static void db_id_prefix(char *out, size_t cap, const char *db_name) {
  char up[96];
  ident(up, sizeof(up), nz(db_name, "db"), true);
  snprintf(out, cap, "ID_%s", up);
}

static int db_table_id_value(xmlNodePtr db, const char *table_name) {
  int table_i = 1;
  EACH_ELEMENT(t, db) if (elem(t, "table")) {
    char *name = attr(t, "name");
    bool match = name && table_name && strcmp(name, table_name) == 0;
    free(name);
    if (match)
      return table_i;
    table_i++;
  }
  return 0;
}

static int db_field_id_value(xmlNodePtr db, const char *table_name, const char *field_name) {
  int field_i = 1;
  EACH_ELEMENT(t, db) if (elem(t, "table")) {
    char *table = attr(t, "name");
    bool table_match = table && table_name && strcmp(table, table_name) == 0;
    free(table);
    EACH_ELEMENT(field, t) if (elem(field, "field")) {
      char *name = attr(field, "name");
      bool field_match = table_match && name && field_name && strcmp(name, field_name) == 0;
      free(name);
      if (field_match)
        return field_i;
      field_i++;
    }
  }
  return 0;
}

static void emit_database(FILE *f, xmlNodePtr db, const char *prefix) {
  if (!db) return;
  int table_i = 1;
  int field_i = 1;
  int model_i = 1;
  int source_i = 1;
  int action_i = 1;
  char *db_name = attr(db, "name");
  char idp[128];
  db_id_prefix(idp, sizeof(idp), db_name);

  // IDs for models/sources/actions are emitted first for stable references.
  LINE("enum {\n");
  EACH_ELEMENT(t, db) if (elem(t, "table")) {
    char *table = attr(t, "name");
    char *model = attr(t, "model");
    char table_up[96], model_up[96], singular[96];
    ident(table_up, sizeof(table_up), nz(table, "table"), true);
    singular_name(singular, sizeof(singular), nz(table, "table"));
    ident(model_up, sizeof(model_up), model && *model ? model : singular, true);
    OUT("  %s_MODEL_%s = %d,\n", idp, model_up, model_i++);
    OUT("  %s_SOURCE_%s = %d,\n", idp, table_up, source_i++);
    OUT("  %s_ACTION_FETCH_%s = %d,\n", idp, table_up, action_i++);
    OUT("  %s_ACTION_INSERT_%s = %d,\n", idp, table_up, action_i++);
    OUT("  %s_ACTION_UPDATE_%s = %d,\n", idp, table_up, action_i++);
    OUT("  %s_ACTION_DELETE_%s = %d,\n", idp, table_up, action_i++);
    free(table);
    free(model);
  }
  LINE("};\n\n");

  EACH_ELEMENT(t, db) if (elem(t, "table")) {
    char *table = attr(t, "name"), *model = attr(t, "model"), type[128], meta[128];
    if (!table || !*table) { free(table); free(model); continue; }
    singular_type(type, sizeof(type), table, model); ident(meta, sizeof(meta), nz(model, table), false);
    char table_up[96];
    ident(table_up, sizeof(table_up), table, true);
    LINE("enum {\n");
    OUT("  %s_%s = %d,\n", idp, table_up, table_i);

    // Field ids for this table.
    EACH_ELEMENT(field, t) if (elem(field, "field")) {
      char *name = attr(field, "name");
      char field_up[96];
      ident(field_up, sizeof(field_up), nz(name, "field"), true);
      OUT("  %s_%s_%s = %d,\n", idp, table_up, field_up, field_i++);
      free(name);
    }

    // Join alias ids for relation fields expose foreign fields as alias.field ids.
    EACH_ELEMENT(field, t) if (elem(field, "field")) {
      char *relation = attr(field, "relation");
      char *alias_attr = attr(field, "alias");
      if (relation && *relation && !attr_truthy(field, "many")) {
        char rel_table[96] = {0};
        char rel_field[96] = {0};
        const char *dot = strchr(relation, '.');
        if (dot) {
          snprintf(rel_table, sizeof(rel_table), "%.*s", (int)(dot - relation), relation);
          snprintf(rel_field, sizeof(rel_field), "%s", dot + 1);
        }
        if (rel_table[0]) {
          char alias[96], alias_up[96], rel_up[96];
          if (alias_attr && *alias_attr) snprintf(alias, sizeof(alias), "%s", alias_attr);
          else singular_name(alias, sizeof(alias), rel_table);
          ident(alias_up, sizeof(alias_up), alias, true);
          ident(rel_up, sizeof(rel_up), rel_table, true);
          EACH_ELEMENT(rt, db) if (elem(rt, "table")) {
            char *rt_name = attr(rt, "name");
            if (rt_name && strcmp(rt_name, rel_table) == 0) {
              EACH_ELEMENT(rf, rt) if (elem(rf, "field") && is_storage_field(rf)) {
                char *rf_name = attr(rf, "name");
                char rf_up[96];
                if (strcmp(nz(rf_name, "field"), rel_field) == 0) {
                  free(rf_name);
                  continue;
                }
                ident(rf_up, sizeof(rf_up), nz(rf_name, "field"), true);
                if (join_alias_field_available(t, table, alias, rf_name))
                  OUT("  %s_%s_%s_%s = %s_%s_%s,\n", idp, table_up, alias_up, rf_up, idp, rel_up, rf_up);
                free(rf_name);
              }
            }
            free(rt_name);
          }
        }
      }
      free(relation);
      free(alias_attr);
    }
    LINE("};\n");

    OUT("typedef struct {\n");
    EACH_ELEMENT(field, t) if (elem(field, "field") && is_storage_field(field)) {
      char *name = attr(field, "name"), *kind = attr(field, "type"), *len = attr(field, "length"); const field_type_t *ft = field_type(kind);
      if (eq(ft->c, "char")) OUT("  char %s[%d];\n", name, len ? atoi(len) : ft->size); else OUT("  %s %s;\n", ft->c, name);
      free(name); free(kind); free(len);
    }
    OUT("} %s;\n\nstatic const db_field_meta_t %s_fields[] = {\n", type, meta);
    EACH_ELEMENT(field, t) if (elem(field, "field") && is_storage_field(field)) {
      char *name = attr(field, "name"), *kind = attr(field, "type"), *len = attr(field, "length"); const field_type_t *ft = field_type(kind);
      char field_up[96];
      ident(field_up, sizeof(field_up), nz(name, "field"), true);
      OUT("  { %s_%s_%s, \"%s\", %s, offsetof(%s, %s), %d },\n",
          idp, table_up, field_up, name, ft->db, type, name,
          eq(ft->c, "char") ? (len ? atoi(len) : ft->size) : 0);
      free(name); free(kind); free(len);
    }
    OUT("};\n");

    OUT("static const db_field_schema_t %s_schema_fields[] = {\n", meta);
    EACH_ELEMENT(field, t) if (elem(field, "field")) {
      char *name = attr(field, "name");
      char *kind = attr(field, "type");
      char *len = attr(field, "length");
      char *key = attr(field, "key");
      char *relation = attr(field, "relation");
      bool relation_many = attr_truthy(field, "many");
      const field_type_t *ft = eq(kind, "relationship") ? field_type("integer") : field_type(kind);
      char field_up[96];
      char rel_table[96] = {0};
      char rel_field[96] = {0};
      char rel_table_up[96] = {0};
      char rel_field_up[96] = {0};
      char relation_table_id[160] = "0";
      char relation_field_id[160] = "0";
      ident(field_up, sizeof(field_up), nz(name, "field"), true);
      if (relation && *relation) {
        const char *dot = strchr(relation, '.');
        if (dot) {
          snprintf(rel_table, sizeof(rel_table), "%.*s", (int)(dot - relation), relation);
          snprintf(rel_field, sizeof(rel_field), "%s", dot + 1);
        }
      }
      if (rel_table[0]) ident(rel_table_up, sizeof(rel_table_up), rel_table, true);
      if (rel_field[0]) ident(rel_field_up, sizeof(rel_field_up), rel_field, true);
      if (rel_table_up[0] && rel_field_up[0]) {
        int rel_table_id = db_table_id_value(db, rel_table);
        int rel_field_id = db_field_id_value(db, rel_table, rel_field);
        if (rel_table_id > 0)
          snprintf(relation_table_id, sizeof(relation_table_id), "%d", rel_table_id);
        else
          snprintf(relation_table_id, sizeof(relation_table_id), "%s_%s", idp, rel_table_up);
        if (rel_field_id > 0)
          snprintf(relation_field_id, sizeof(relation_field_id), "%d", rel_field_id);
        else
          snprintf(relation_field_id, sizeof(relation_field_id), "%s_%s_%s", idp, rel_table_up, rel_field_up);
      }
      OUT("  { %s_%s_%s, \"%s\", %s, %d, %s, %s, %s, %s },\n",
          idp, table_up, field_up, name, ft->db,
          eq(ft->c, "char") ? (len ? atoi(len) : ft->size) : 0,
          key && eq(key, "YES") ? "true" : "false",
          relation_table_id,
          relation_field_id,
          relation_many ? "true" : "false");
      free(name);
      free(kind);
      free(len);
      free(key);
      free(relation);
    }
    OUT("};\n");

    int join_count = 0;
    EACH_ELEMENT(field, t) if (elem(field, "field")) {
      char *relation = attr(field, "relation");
      char *alias_attr = attr(field, "alias");
      if (relation && *relation && !attr_truthy(field, "many"))
        join_count++;
      free(relation);
      free(alias_attr);
    }

    if (join_count > 0) {
      OUT("static const db_join_schema_t %s_schema_joins[] = {\n", meta);
      EACH_ELEMENT(field, t) if (elem(field, "field")) {
        char *name = attr(field, "name");
        char *relation = attr(field, "relation");
        char *alias_attr = attr(field, "alias");
        if (relation && *relation && !attr_truthy(field, "many")) {
          char rel_table[96] = {0};
          const char *dot = strchr(relation, '.');
          if (dot) snprintf(rel_table, sizeof(rel_table), "%.*s", (int)(dot - relation), relation);
          if (rel_table[0]) {
            char alias[96], alias_up[96], rel_up[96], local_field_up[96];
            if (alias_attr && *alias_attr) snprintf(alias, sizeof(alias), "%s", alias_attr);
            else singular_name(alias, sizeof(alias), rel_table);
            ident(alias_up, sizeof(alias_up), alias, true);
            ident(rel_up, sizeof(rel_up), rel_table, true);
            ident(local_field_up, sizeof(local_field_up), nz(name, "field"), true);
            EACH_ELEMENT(rt, db) if (elem(rt, "table")) {
              char *rt_name = attr(rt, "name");
              if (rt_name && strcmp(rt_name, rel_table) == 0) {
                OUT("  { %s_%s_%s, \"%s\", %s_%s_%s, %s_%s, %s_%s_ID },\n",
                    idp, table_up, local_field_up, alias,
                    idp, table_up, local_field_up,
                    idp, rel_up,
                    idp, rel_up);
              }
              free(rt_name);
            }
          }
        }
        free(name);
        free(relation);
        free(alias_attr);
      }
      OUT("};\n");
    }
    table_i++;
    free(table); free(model);
  }
  OUT("static const db_table_schema_t %s_schema_tables[] = {\n", prefix);
  EACH_ELEMENT(t, db) if (elem(t, "table")) {
    char *table = attr(t, "name");
    char *model = attr(t, "model");
    char meta[128], table_up[96], singular[96], model_up[96], join_name[160];
    const char *model_name;
    const char *joins_expr;
    int join_count = 0;
    if (!table || !*table) { free(table); free(model); continue; }
    ident(meta, sizeof(meta), nz(model, table), false);
    ident(table_up, sizeof(table_up), table, true);
    singular_name(singular, sizeof(singular), nz(table, "table"));
    model_name = model && *model ? model : singular;
    ident(model_up, sizeof(model_up), model_name, true);
    EACH_ELEMENT(field, t) if (elem(field, "field")) {
      char *relation = attr(field, "relation");
      char *alias_attr = attr(field, "alias");
      if (relation && *relation && !attr_truthy(field, "many"))
        join_count++;
      free(relation);
      free(alias_attr);
    }
    snprintf(join_name, sizeof(join_name), "%s_schema_joins", meta);
    joins_expr = join_count > 0 ? join_name : "NULL";
    OUT("  { %s_%s, \"%s\", %s_MODEL_%s, \"%s\", STATIC_ARRAY(%s_schema_fields), %s, %d },\n",
      idp, table_up, table, idp, model_up,
        model_name,
        meta,
        joins_expr,
        join_count > 0 ? join_count : 0);
    free(table);
    free(model);
  }
  OUT("};\n");
  OUT("static db_schema_def_t %s_database_schema = { NULL, NULL, NULL, STATIC_ARRAY(%s_schema_tables) };\n\n", prefix, prefix);
  OUT("static const db_api_def_t %s_database_api = { NULL, 0, NULL, 0, NULL, 0 };\n\n", prefix);
  free(db_name);
}

static bool table_name_from_table_id(char *out, size_t cap, xmlNodePtr db, const char *table_name) {
  if (!db || !table_name || !*table_name)
    return false;
  if (!db_find_table_node(db, table_name))
    return false;
  snprintf(out, cap, "%s", table_name);
  return true;
}

static bool infer_many_relation_filter(xmlNodePtr db, const char *master_table_name,
                                       const char *relation_name,
                                       char *detail_table, size_t detail_table_cap,
                                       char *filter_field, size_t filter_field_cap) {
  if (!db || !master_table_name || !relation_name || !detail_table || !filter_field)
    return false;

  xmlNodePtr master = db_find_table_node(db, master_table_name);
  if (!master)
    return false;

  char relation_table[128] = {0};
  EACH_ELEMENT(field, master) if (elem(field, "field")) {
    char *name = attr(field, "name");
    bool name_ok = name && strcmp(name, relation_name) == 0;
    free(name);
    if (!name_ok)
      continue;

    char *relation = attr(field, "relation");
    bool many = attr_truthy(field, "many");
    if (!many || !relation || !*relation) {
      free(relation);
      return false;
    }
    char rel_field[128] = {0};
    bool split_ok = split_relation_ref(relation, relation_table, sizeof(relation_table),
                                       rel_field, sizeof(rel_field));
    free(relation);
    if (!split_ok)
      return false;
    break;
  }

  if (!relation_table[0])
    return false;

  xmlNodePtr detail = db_find_table_node(db, relation_table);
  if (!detail)
    return false;

  char expected_relation[192];
  snprintf(expected_relation, sizeof(expected_relation), "%s.id", master_table_name);
  char match[128] = {0};
  int matches = 0;
  EACH_ELEMENT(field, detail) if (elem(field, "field")) {
    if (attr_truthy(field, "many"))
      continue;
    char *relation = attr(field, "relation");
    char *name = attr(field, "name");
    if (relation && strcmp(relation, expected_relation) == 0 && name && *name) {
      snprintf(match, sizeof(match), "%s", name);
      matches++;
    }
    free(relation);
    free(name);
  }
  if (matches != 1)
    return false;

  snprintf(detail_table, detail_table_cap, "%s", relation_table);
  snprintf(filter_field, filter_field_cap, "%s", match);
  return true;
}

static bool resolve_table_source(xmlNodePtr db, const char *source,
                                 char *table, size_t table_cap,
                                 char *filter_field, size_t filter_field_cap) {
  if (table && table_cap > 0)
    table[0] = '\0';
  if (filter_field && filter_field_cap > 0)
    filter_field[0] = '\0';
  if (!table || table_cap == 0)
    return false;

  char path[256];
  snprintf(path, sizeof(path), "%s", nz(source, "table"));
  char *parts[4] = {0};
  int n = 0;
  for (char *p = strtok(path, "."); p && n < 4; p = strtok(NULL, "."))
    parts[n++] = p;

  if (n == 0)
    return false;
  if (n == 1) {
    snprintf(table, table_cap, "%s", parts[0]);
    return true;
  }
  if (n == 2) {
    snprintf(table, table_cap, "%s", parts[1]);
    return true;
  }
  if (n == 3) {
    char inferred_filter[128] = {0};
    bool ok = infer_many_relation_filter(db, parts[1], parts[2],
                                         table, table_cap,
                                         inferred_filter, sizeof(inferred_filter));
    if (ok && filter_field && filter_field_cap > 0)
      snprintf(filter_field, filter_field_cap, "%s", inferred_filter);
    return ok;
  }

  return false;
}

static void table_name_from_source(char *out, size_t cap, const char *source) {
  if (!resolve_table_source(NULL, source, out, cap, NULL, 0)) {
    const char *dot = source ? strchr(source, '.') : NULL;
    snprintf(out, cap, "%s", dot ? dot + 1 : nz(source, "table"));
    char *next = strchr(out, '.');
    if (next) *next = 0;
  }
}

static bool field_id_expr_from_source(char *out, size_t cap, xmlNodePtr db, const char *db_name, const char *source, const char *field) {
  char table[128], db_up[96], table_up[96], field_up[96];
  if (!resolve_table_source(db, source, table, sizeof(table), NULL, 0))
    table_name_from_source(table, sizeof(table), source);
  ident(db_up, sizeof(db_up), nz(db_name, "db"), true);
  xmlNodePtr source_table = db_find_table_node(db, table);
  if (!source_table) {
    fprintf(stderr, "orionc_alt: source '%s' references unknown table '%s'\n", nz(source, ""), table);
    g_orionc_failed = true;
    snprintf(out, cap, "0");
    return false;
  }
  if (field && strchr(field, '.')) {
    char alias[96] = {0};
    char rel_table[96] = {0};
    char matched_rel_table[96] = {0};
    char matched_rel_field[96] = {0};
    char matched_local_field[96] = {0};
    const char *dot = strchr(field, '.');
    int matches = 0;
    snprintf(alias, sizeof(alias), "%.*s", (int)(dot - field), field);
    EACH_ELEMENT(relf, source_table) if (elem(relf, "field")) {
      char *local_name = attr(relf, "name");
      char *relation = attr(relf, "relation");
      char *alias_attr = attr(relf, "alias");
      char rel_alias[96] = {0};
      char rel_field[96] = {0};
      if (relation && *relation) {
        const char *relation_dot = strchr(relation, '.');
        if (relation_dot) {
          snprintf(rel_table, sizeof(rel_table), "%.*s", (int)(relation_dot - relation), relation);
          snprintf(rel_field, sizeof(rel_field), "%s", relation_dot + 1);
          if (alias_attr && *alias_attr) snprintf(rel_alias, sizeof(rel_alias), "%s", alias_attr);
          else singular_name(rel_alias, sizeof(rel_alias), rel_table);
          if (strcmp(rel_alias, alias) == 0) {
            matches++;
            snprintf(matched_rel_table, sizeof(matched_rel_table), "%s", rel_table);
            snprintf(matched_rel_field, sizeof(matched_rel_field), "%s", rel_field);
            snprintf(matched_local_field, sizeof(matched_local_field), "%s", nz(local_name, "field"));
          }
        }
      }
      free(local_name);
      free(relation);
      free(alias_attr);
      if (matches > 1)
        break;
    }
    if (matches == 0) {
      fprintf(stderr, "orionc_alt: field '%s' references unknown relation alias '%s' on source '%s'\n",
              nz(field, ""), alias, nz(source, ""));
      g_orionc_failed = true;
      snprintf(out, cap, "0");
      return false;
    }
    if (matches > 1) {
      fprintf(stderr, "orionc_alt: relation alias '%s' is ambiguous on source '%s'\n",
              alias, nz(source, ""));
      g_orionc_failed = true;
      snprintf(out, cap, "0");
      return false;
    }
    if (matched_rel_field[0] && strcmp(dot + 1, matched_rel_field) == 0) {
      char local_field_up[96];
      ident(table_up, sizeof(table_up), nz(table, "table"), true);
      ident(local_field_up, sizeof(local_field_up), matched_local_field, true);
      snprintf(out, cap, "ID_%s_%s_%s", db_up, table_up, local_field_up);
      return true;
    }
        xmlNodePtr foreign_table = db_find_table_node(db, matched_rel_table);
    if (!foreign_table || !table_has_field_named(foreign_table, dot + 1)) {
      fprintf(stderr, "orionc_alt: field '%s' references unknown joined field '%s' on table '%s'\n",
            nz(field, ""), dot + 1, matched_rel_table);
      g_orionc_failed = true;
      snprintf(out, cap, "0");
      return false;
    }
    ident(table_up, sizeof(table_up), nz(table, "table"), true);
    char alias_up[96];
    ident(alias_up, sizeof(alias_up), alias, true);
    ident(field_up, sizeof(field_up), dot + 1, true);
    snprintf(out, cap, "ID_%s_%s_%s_%s", db_up, table_up, alias_up, field_up);
    return true;
  }
  if (!table_has_field_named(source_table, nz(field, "id"))) {
    fprintf(stderr, "orionc_alt: field '%s' does not exist on source '%s'\n",
            nz(field, ""), nz(source, ""));
    g_orionc_failed = true;
    snprintf(out, cap, "0");
    return false;
  }
  if (!table_name_from_table_id(table, sizeof(table), db, table)) {
    snprintf(out, cap, "0");
    return false;
  }
  ident(table_up, sizeof(table_up), table, true);
  ident(field_up, sizeof(field_up), nz(field, "id"), true);
  snprintf(out, cap, "ID_%s_%s_%s", db_up, table_up, field_up);
  return true;
}

// count_table_fields — count number of fields in a database table
static int count_table_fields(xmlNodePtr db, const char *table_name) {
  if (!db || !table_name) return 0;
  EACH_ELEMENT(t, db) if (elem(t, "table")) {
    char *name = attr(t, "name");
    if (eq(name, table_name)) {
      int count = 0;
      EACH_ELEMENT(field, t) if (elem(field, "field") && is_storage_field(field)) count++;
      free(name);
      return count;
    }
    free(name);
  }
  return 0;
}

static bool emit_tableviews(FILE *f, xmlNodePtr parent, const char *form, xmlNodePtr database, const char *db_name) {
  EACH_ELEMENT(c, parent) {
    if (elem(c, "tableview")) {
      char *name = attr(c, "name"), *source = attrs_first(c, "source", "database");
      char *master_key = attr(c, "master-key");
      char db_up[96];
      char param[256], table[128], table_id[128], inferred_filter[128] = {0}; snprintf(param, sizeof(param), "%s_%s_tableview_params", form, nz(name, "unnamed"));
      char filter_expr[256] = "0";
      ident(db_up, sizeof(db_up), db_name, true);
      if (!resolve_table_source(database, source, table, sizeof(table),
                                inferred_filter, sizeof(inferred_filter))) {
        fprintf(stderr, "orionc_alt: TableView '%s' source '%s' could not be resolved\n",
                nz(name, "unnamed"), nz(source, ""));
        free(name);
        free(source);
        free(master_key);
        return false;
      }
      ident(table_id, sizeof(table_id), table, true);
      const char *filter_field = (master_key && *master_key) ? master_key : inferred_filter;
      if (filter_field && *filter_field && !field_id_expr_from_source(filter_expr, sizeof(filter_expr), database, db_name, source, filter_field)) {
        free(name);
        free(source);
        free(master_key);
        return false;
      }
      OUT("static const uint32_t %s_field_ids[] = { ", param);
      EACH_ELEMENT(col, c) if (elem(col, "column")) {
        char *v = attr(col, "field");
        char id_expr[256];
        if (!field_id_expr_from_source(id_expr, sizeof(id_expr), database, db_name, source, v)) {
          free(v);
          free(name);
          free(source);
          return false;
        }
        OUT("%s, ", id_expr);
        free(v);
      }
      LINE("};\n");
      OUT("static const char *%s_titles[] = { ", param); EACH_ELEMENT(col, c) if (elem(col, "column")) { char *v = attr(col, "title"), q[ORIONC_STRING_SIZE]; cstr(q, sizeof(q), v); OUT("%s, ", q); free(v); } LINE("NULL };\n");
      OUT("static const int %s_widths[] = { ", param); EACH_ELEMENT(col, c) if (elem(col, "column")) { char *v = attr(col, "width"); OUT("%d, ", v ? atoi(v) : 0); free(v); } LINE("};\n");
      int col_count = 0; EACH_ELEMENT(col, c) if (elem(col, "column")) col_count++;
      OUT("static const tableview_params_t %s = { NULL, ID_%s_%s, %s, 0, %s_field_ids, %s_titles, %s_widths, %d };\n\n", param, db_up, table_id, filter_expr, param, param, param, col_count);
      free(name); free(source); free(master_key);
    }
    if (!emit_tableviews(f, c, form, database, db_name))
      return false;
  }
  return true;
}

static bool emit_comboboxes(FILE *f, xmlNodePtr parent, const char *form, xmlNodePtr database, const char *db_name) {
  EACH_ELEMENT(c, parent) {
    if (elem(c, "combobox")) {
      char *name = attr(c, "name"), *source = attr(c, "source");
      char *display = attr(c, "display"), *value = attr(c, "value");
      
      // Only generate params if source/display/value are all present
      if (source && display && value) {
        char db_up[96];
        char param[256], table[128], table_id[128];
        char display_id[256], value_id[256];
        
        snprintf(param, sizeof(param), "%s_%s_combobox_params", form, nz(name, "unnamed"));
        ident(db_up, sizeof(db_up), db_name, true);
        table_name_from_source(table, sizeof(table), source);
        ident(table_id, sizeof(table_id), table, true);
        if (!field_id_expr_from_source(display_id, sizeof(display_id), database, db_name, source, display) ||
            !field_id_expr_from_source(value_id, sizeof(value_id), database, db_name, source, value)) {
          free(name); free(source); free(display); free(value);
          return false;
        }
        
        OUT("static const combobox_params_t %s = { NULL, ID_%s_%s, %s, %s };\n\n",
            param, db_up, table_id, display_id, value_id);
      }
      
      free(name); free(source); free(display); free(value);
    }
    if (!emit_comboboxes(f, c, form, database, db_name))
      return false;
  }
  return true;
}

static const char *binding_getter(const char *klass) {
  if (klass && (strcasecmp(klass, "textedit") == 0 || strcasecmp(klass, "multiedit") == 0))
    return "edGetText";
  if (klass && strcasecmp(klass, "combobox") == 0)
    return "cbGetCurrentValue";  // Use value_field (ID) not row index
  if (klass && strcasecmp(klass, "checkbox") == 0)
    return "chkIsChecked";
  return "0";
}

static void binding_record_type(char *out, size_t cap, const char *table) {
  char id[96]; ident(id, sizeof(id), table, false);
  size_t n = strlen(id); if (n > 1 && id[n - 1] == 's') id[n - 1] = 0;
  snprintf(out, cap, "db_%s_t", id);
}

static void add_binding(bindings_t *b, const char *ctrl, const char *path, const char *klass) {
  if (!b || b->n >= ARRAY_LEN(b->v)) return;
  binding_t next = {0};
  if (!db_path(path, next.db, sizeof(next.db), next.table, sizeof(next.table), next.field, sizeof(next.field))) return;
  snprintf(next.ctrl, sizeof(next.ctrl), "%s", ctrl);
  snprintf(next.klass, sizeof(next.klass), "%s", klass);
  if (!b->n) { snprintf(b->db, sizeof(b->db), "%s", next.db); snprintf(b->table, sizeof(b->table), "%s", next.table); }
  b->v[b->n++] = next;
}

static void emit_controls_ex(FILE *f, xmlNodePtr parent, const char *form, const char *parent_id, bindings_t *bindings, int *count, button_ids_t *btn_ids) {
  int ordinal = 0;
  EACH_ELEMENT(c, parent) if (is_control(parent, c)) {
    attrs_t a; rect_t sz = size_attr(c), pad = {0}, mar = {0}; char id[256], klass[128], classq[ORIONC_STRING_SIZE], textq[ORIONC_STRING_SIZE], nameq[ORIONC_STRING_SIZE], flags[256], spacing[16], font[16], color[16], lparam[256] = "NULL";
    read_control_attrs(c, &a); control_id(id, sizeof(id), form, a.v[A_NAME], (char *)c->name, ordinal++); ident(klass, sizeof(klass), (char *)c->name, false);
    bool is_report_column = report_column_node(parent, c);
    if (elem(c, "textbox"))
      snprintf(klass, sizeof(klass), "TextEdit");
    if (is_report_column)
      snprintf(klass, sizeof(klass), "ReportColumn");
    if (has_controls(c) && !elem(c, "column")) sz = (rect_t){0};
    rect_attr(c, "padding", &pad) || rect_attr(c, "layout_padding", &pad); rect_attr(c, "margin", &mar) || rect_attr(c, "layout_margin", &mar);
    orionc_parse_short_flags(flags, sizeof(flags), a.v[A_FLAGS], "control flags");
    if (enum_parse_token(a.v[A_ORIENT], kOrient, ARRAY_LEN(kOrient), WINDOW_STACK_VERTICAL) & WINDOW_STACK_HORIZONTAL)
      orionc_append_flag_expr(flags, sizeof(flags), "WINDOW_STACK_HORIZONTAL");
    if (elem(c, "space") || elem(c, "multiedit"))
      orionc_append_flag_expr(flags, sizeof(flags), "WINDOW_FLEXSPACE");
    snprintf(spacing, sizeof(spacing), "%u", byte_attr(a.v[A_SPACING], ORIONC_DEFAULT_SPACING));
    snprintf(font, sizeof(font), "%s", eq(a.v[A_FONT], "system") ? "FONT_SYSTEM" : eq(a.v[A_FONT], "icon") ? "FONT_ICON" : "FONT_SMALL");
    snprintf(color, sizeof(color), "%u", (unsigned)enum_parse_token(a.v[A_COLOR], kColors, ARRAY_LEN(kColors), brTextNormal));
    if (elem(c, "tableview")) snprintf(lparam, sizeof(lparam), "&%s_%s_tableview_params", form, nz(a.v[A_NAME], "unnamed"));
    if (elem(c, "combobox") && attr(c, "source")) snprintf(lparam, sizeof(lparam), "&%s_%s_combobox_params", form, nz(a.v[A_NAME], "unnamed"));
    if (a.v[A_FIELD] && !is_report_column) add_binding(bindings, id, a.v[A_FIELD], klass);
    
    // Track button IDs for ok_id/cancel_id form metadata
    if (elem(c, "button") && btn_ids) {
      char *action = attr(c, "action");
      if (action && (strstr(action, ".insert") || strstr(action, ".update"))) {
        snprintf(btn_ids->ok_id, sizeof(btn_ids->ok_id), "%s", id);
      }
      free(action);
      
      // Detect cancel button by text or name
      if ((a.v[A_TEXT] && strcasecmp(a.v[A_TEXT], "Cancel") == 0) ||
          (a.v[A_NAME] && strcasecmp(a.v[A_NAME], "cancel") == 0)) {
        snprintf(btn_ids->cancel_id, sizeof(btn_ids->cancel_id), "%s", id);
      }
    }
    
    char *report_column_title = is_report_column ? attr(c, "title") : NULL;
    cstr(classq, sizeof(classq), klass);
    cstr(textq, sizeof(textq), is_report_column ? nz(report_column_title, a.v[A_TEXT]) : a.v[A_TEXT]);
    cstr(nameq, sizeof(nameq), a.v[A_NAME]);
    OUT("  { %s, %s, { %d, %d }, %s, %s, %s, %u, %u, NULL, 0, %s, { %d, %d, %d, %d }, { %d, %d, %d, %d }, %s, %s, %s, %s, %s, %s },\n",
        classq, id, sz.w, sz.h, flags, textq, nameq,
        (unsigned)enum_parse_token(a.v[A_HA], kAlignH, ARRAY_LEN(kAlignH), 0),
        (unsigned)enum_parse_token(a.v[A_VA], kAlignV, ARRAY_LEN(kAlignV), 0),
        spacing, pad.x, pad.y, pad.w, pad.h, mar.x, mar.y, mar.w, mar.h,
        nz(parent_id, "0"), font, a.v[A_FONT] ? "true" : "false", color, a.v[A_COLOR] ? "true" : "false", lparam);
    free(report_column_title);
    (*count)++;
    emit_controls_ex(f, c, form, id, bindings, count, btn_ids);
    free_attrs(&a);
  }
}

static void emit_bindings(FILE *f, const char *prefix, const char *form, const bindings_t *b) {
  if (!b || !b->n) return;
  char type[128]; binding_record_type(type, sizeof(type), b->table);
  OUT("static const ctrl_binding_t %s_%s_bindings[] = {\n", prefix, form);
  for (int i = 0; i < b->n; i++) {
    const binding_t *x = &b->v[i];
    OUT("  { %s, 0, %s, offsetof(%s, %s), ", x->ctrl, binding_getter(x->klass), type, x->field);
    if (strcasecmp(x->klass, "textedit") == 0 || strcasecmp(x->klass, "multiedit") == 0) OUT("sizeof_field(%s, %s)", type, x->field);
    else if (strcasecmp(x->klass, "combobox") == 0) LINE("(size_t)-1");
    else LINE("0");
    LINE(", NULL, NULL },\n");
  }
  LINE("};\n\n");
}

static bool emit_form(FILE *f, xmlNodePtr form, const char *prefix, xmlNodePtr database) {
  char *name = attr(form, "name"), *title = attr(form, "title"), *flags = attr(form, "flags"), *toolbar = attr(form, "toolbar"), *spacing = attrs_first(form, "spacing", "layout_spacing");
  char form_flags[256];
  orionc_parse_short_flags(form_flags, sizeof(form_flags), flags, "form flags");
  rect_t sz = size_attr(form), pad = {0}, mar = {0}; char form_id[128], titleq[ORIONC_STRING_SIZE];
  if (!sz.w) { fprintf(stderr, "orionc_alt: form '%s' requires width=\n", nz(name, "")); return false; }
  ident(form_id, sizeof(form_id), name, false); cstr(titleq, sizeof(titleq), nz(title, name));
  rect_attr(form, "padding", &pad) || rect_attr(form, "layout_padding", &pad); rect_attr(form, "margin", &mar) || rect_attr(form, "layout_margin", &mar);
  char *db_name = database ? attr(database, "name") : NULL;
  if (!emit_tableviews(f, form, form_id, database, db_name ? db_name : "db")) return false;
  if (!emit_comboboxes(f, form, form_id, database, db_name ? db_name : "db")) return false;
  free(db_name);
  OUT("static const form_ctrl_def_t %s_%s_children[] = {\n", prefix, form_id);
  int count = 0; bindings_t bindings = {0}; button_ids_t btn_ids = {0}; 
  emit_controls_ex(f, form, form_id, "0", &bindings, &count, &btn_ids); LINE("};\n\n");
  emit_bindings(f, prefix, form_id, &bindings);
    OUT("static const form_def_t %s_%s_form = { .name = %s, .width = %d, .height = %d, .flags = (%s) | WINDOW_AUTO_LAYOUT, .layout_spacing = %u, .padding = { %d, %d, %d, %d }, .margin = { %d, %d, %d, %d }, .children = %s_%s_children, .child_count = %d",
      prefix, form_id, titleq, sz.w, sz.h, form_flags, byte_attr(spacing, ORIONC_DEFAULT_SPACING),
      pad.x, pad.y, pad.w, pad.h, mar.x, mar.y, mar.w, mar.h, prefix, form_id, count);
  if (toolbar && *toolbar) { char tb[128]; ident(tb, sizeof(tb), toolbar, true); OUT(", .toolbar_items = TB_%s, .toolbar_count = TB_%s_COUNT", tb, tb); }
  else LINE(", .toolbar_items = NULL, .toolbar_count = 0");
  if (bindings.n) {
    char db_up[96], table_id[128], meta[128]; 
    ident(db_up, sizeof(db_up), bindings.db, true);
    ident(table_id, sizeof(table_id), bindings.table, true);
    ident(meta, sizeof(meta), bindings.table, false);
    int field_count = count_table_fields(database, bindings.table);
    OUT(", .bindings = %s_%s_bindings, .binding_count = %d, .ok_id = %s, .cancel_id = %s, .db_name = \"%s\", .db_table = \"%s\", .db_table_id = ID_%s_%s, .db_fields = %s_fields, .db_field_count = %d",
        prefix, form_id, bindings.n, 
        btn_ids.ok_id[0] ? btn_ids.ok_id : "0",
        btn_ids.cancel_id[0] ? btn_ids.cancel_id : "0",
        bindings.db, bindings.table, db_up, table_id, meta, field_count);
  } else {
    OUT(", .bindings = NULL, .binding_count = 0, .ok_id = %s, .cancel_id = %s, .db_name = NULL, .db_table = NULL, .db_table_id = 0, .db_fields = NULL, .db_field_count = 0",
        btn_ids.ok_id[0] ? btn_ids.ok_id : "0",
        btn_ids.cancel_id[0] ? btn_ids.cancel_id : "0");
  }
  LINE(" };\n\n");
  free(name); free(title); free(flags); free(toolbar); free(spacing);
  return true;
}

static void usage(const char *argv0) { fprintf(stderr, "usage: %s --input file.orion --output forms.h --prefix name [--form id]\n", argv0); }

int main(int argc, char **argv) {
  const char *input = NULL, *output = NULL, *prefix = "orion", *only = NULL;
  for (int i = 1; i < argc; i++) {
    if (eq(argv[i], "--input") && i + 1 < argc) input = argv[++i];
    else if (eq(argv[i], "--output") && i + 1 < argc) output = argv[++i];
    else if (eq(argv[i], "--prefix") && i + 1 < argc) prefix = argv[++i];
    else if (eq(argv[i], "--form") && i + 1 < argc) only = argv[++i];
    else { usage(argv[0]); return 2; }
  }
  if (!input || !output) { usage(argv[0]); return 2; }
  xmlDocPtr doc = xmlReadFile(input, NULL, XML_PARSE_NONET);
  xmlNodePtr root = doc ? xmlDocGetRootElement(doc) : NULL;
  if (!elem(root, "orion")) { fprintf(stderr, "orionc_alt: %s is not an <orion> document\n", input); if (doc) xmlFreeDoc(doc); return 1; }
  FILE *f = fopen(output, "wb"); if (!f) { perror(output); xmlFreeDoc(doc); return 1; }
  char guard[256], pre[128]; ident(guard, sizeof(guard), output, true); ident(pre, sizeof(pre), prefix, false);
  xmlNodePtr menus = child(root, "menus"), toolbars = child(root, "toolbars"), forms = child(root, "forms"), databases = child(root, "databases"), database = databases ? child(databases, "database") : child(root, "database");
  normalize_database_schema(database);
  ids_t commands = {0}, controls = {0}; collect_command_ids(&commands, menus, toolbars);
  EACH_ELEMENT(form, forms) if (elem(form, "form")) { char *name = attr(form, "name"), form_id[128]; if (!only || eq(name, only)) { ident(form_id, sizeof(form_id), name, false); collect_control_ids(&controls, form, form_id); } free(name); }
  tpl(f, "/* Generated by orionc_alt from {{input}}. */\n#ifndef {{guard}}\n#define {{guard}}\n\n#include \"ui.h\"\n#include \"user/icons.h\"\n\n",
      (kv_t[]){{"input", input}, {"guard", guard}, {NULL, NULL}});
  emit_enum_ids(f, &commands, "ID_COMMAND_BASE"); emit_enum_ids(f, &controls, "ID_CONTROL_BASE");
  emit_menus(f, menus); emit_toolbars(f, toolbars); emit_database(f, database, pre);
  if (g_orionc_failed) return 1;
  int emitted = 0;
  EACH_ELEMENT(form, forms) if (elem(form, "form")) { char *name = attr(form, "name"); if (!only || eq(name, only)) { if (!emit_form(f, form, pre, database)) return 1; emitted++; } free(name); }
  if (g_orionc_failed) return 1;
  OUT("#endif /* %s */\n", guard);
  fclose(f); xmlFreeDoc(doc);
  if (!emitted) { fprintf(stderr, "orionc_alt: no forms emitted from %s\n", input); return 1; }
  return 0;
}
