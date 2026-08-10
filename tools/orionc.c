#include <libxml/parser.h>
#include <libxml/tree.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <orion/user/enum_parse.h>
#include <orion/user/user.h>

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
  {"transparent", brTransparent}, {"control-bg", brControlBg},
  {"window-dark-bg", brWindowDarkBg}, {"workspace-bg", brWorkspaceBg},
  {"active-titlebar", brActiveTitlebar}, {"active-titlebar-text", brActiveTitlebarText},
  {"inactive-titlebar", brInactiveTitlebar}, {"inactive-titlebar-text", brInactiveTitlebarText},
  {"statusbar-bg", brStatusbarBg}, {"light-edge", brLightEdge},
  {"dark-edge", brDarkEdge}, {"flare", brFlare}, {"focus-ring", brFocusRing},
  {"button-inner", brButtonInner},
  {"button-hover", brButtonHover}, {"text-normal", brTextNormal},
  {"text-disabled", brTextDisabled}, {"text-error", brTextError},
  {"text-success", brTextSuccess}, {"border-focus", brBorderFocus},
  {"border-active", brBorderActive}, {"folder-text", brFolderText},
  {"column-view-bg", brColumnViewBg}, {"modal-overlay", brModalOverlay},
};

static bool eq(const char *a, const char *b) { return a && b && strcmp(a, b) == 0; }
static const char *nz(const char *s, const char *d) { return (s && *s) ? s : d; }
static bool elem(xmlNodePtr n, const char *name) {
  return n && n->type == XML_ELEMENT_NODE && xmlStrcasecmp(n->name, BAD_CAST name) == 0;
}
static char *attr(xmlNodePtr n, const char *name) { xmlChar *r = xmlGetProp(n, BAD_CAST name); char *s = r ? strdup((char *)r) : NULL; if (r) xmlFree(r); return s; }
static char *attrs_first(xmlNodePtr n, const char *a, const char *b) { char *v = attr(n, a); if (v && *v) return v; free(v); return b ? attr(n, b) : NULL; }
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
  return n && n->type == XML_ELEMENT_NODE && !elem(n, "requires") && !(elem(parent, "tableview") && elem(n, "column"));
}
static bool has_controls(xmlNodePtr n) { EACH_ELEMENT(c, n) if (is_control(n, c)) return true; return false; }

static void emit_defines(FILE *f, const ids_t *ids, const char *base) {
  for (int i = 0; i < ids->n; i++) OUT("#define %s (%s + %d)\n", ids->v[i].name, base, i + 1);
  if (ids->n) LINE("\n");
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

static void command_ref(char *out, size_t cap, const char *command,
                        const char *scope, const char *name, const char *label) {
  if (command && *command) { char id[ORIONC_MAX_IDENT]; ident(id, sizeof(id), command, true); snprintf(out, cap, "ID_%s", id); }
  else scoped(out, cap, scope, name, label);
}

static void collect_context_ids(ids_t *ids, xmlNodePtr contexts) {
  EACH_ELEMENT(menu, contexts) if (elem(menu, "contextmenu")) {
    char *scope = attr(menu, "name");
    EACH_ELEMENT(it, menu) if (elem(it, "item")) {
      char *command = attr(it, "command"), *name = attr(it, "name"), *label = attr(it, "label"), id[256];
      if (!command || !*command) { command_ref(id, sizeof(id), NULL, scope, name, label); add_id(ids, id); }
      free(command); free(name); free(label);
    }
    free(scope);
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

static void emit_context_menus(FILE *f, xmlNodePtr contexts) {
  EACH_ELEMENT(menu, contexts) if (elem(menu, "contextmenu")) {
    char *name = attr(menu, "name"), scope[128], base[160]; ident(scope, sizeof(scope), name, true); snprintf(base, sizeof(base), "CONTEXT_MENU_%s", scope);
    OUT("static const menu_item_t %s_ITEMS[] = {\n", base);
    EACH_ELEMENT(it, menu) {
      if (elem(it, "separator")) LINE("  { NULL, 0, NULL, 0 },\n");
      else if (elem(it, "item")) {
        char *command = attr(it, "command"), *item_name = attr(it, "name"), *raw = attr(it, "label"), id[256], label[ORIONC_STRING_SIZE];
        command_ref(id, sizeof(id), command, name, item_name, raw); cstr(label, sizeof(label), raw);
        OUT("  { %s, %s, NULL, 0 },\n", label, id);
        free(command); free(item_name); free(raw);
      }
    }
    OUT("};\n#define %s_COUNT ((int)(sizeof(%s_ITEMS) / sizeof(%s_ITEMS[0])))\n\n", base, base, base);
    free(name);
  }
}

static void emit_toolbars(FILE *f, xmlNodePtr toolbars) {
  EACH_ELEMENT(tb, toolbars) if (elem(tb, "toolbar")) {
    char *tbid = attr(tb, "name"), scope[128]; ident(scope, sizeof(scope), tbid, true);
    OUT("static const toolbar_item_t TB_%s[] = {\n", scope);
    EACH_ELEMENT(it, tb) if (toolbar_type(it)) {
      char *menu = attr(it, "menu"), *name = attr(it, "name"), *icon = attr(it, "icon"), *w = attr(it, "w"), *flags = attr(it, "flags"), *text = attr(it, "text"), *tooltip = attr(it, "tooltip");
      char id[256] = "0", textq[ORIONC_STRING_SIZE], tipq[ORIONC_STRING_SIZE], menu_scope[128];
      if (!elem(it, "separator") && !elem(it, "spacer")) { ident(menu_scope, sizeof(menu_scope), nz(menu, scope), true); scoped(id, sizeof(id), menu_scope, name, text); }
      if (text && *text) cstr(textq, sizeof(textq), text); else snprintf(textq, sizeof(textq), "NULL");
      if (tooltip && *tooltip) cstr(tipq, sizeof(tipq), tooltip); else snprintf(tipq, sizeof(tipq), "NULL");
      OUT("  { %s, %s, %s, %s, %s, %s, %s },\n", toolbar_type(it), id, nz(icon, "-1"), nz(w, "0"), nz(flags, "0"), textq, tipq);
      free(menu); free(name); free(icon); free(w); free(flags); free(text); free(tooltip);
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

static void emit_database(FILE *f, xmlNodePtr db, const char *prefix) {
  if (!db) return;
  int table_i = 0;
  EACH_ELEMENT(t, db) if (elem(t, "table")) {
    char *table = attr(t, "name"), *model = attr(t, "model"), type[128], meta[128];
    if (!table || !*table) { free(table); free(model); continue; }
    singular_type(type, sizeof(type), table, model); ident(meta, sizeof(meta), nz(model, table), false);
    OUT("typedef struct {\n");
    EACH_ELEMENT(field, t) if (elem(field, "field")) {
      char *name = attr(field, "name"), *kind = attr(field, "type"), *len = attr(field, "length"); const field_type_t *ft = field_type(kind);
      if (eq(ft->c, "char")) OUT("  char %s[%d];\n", name, len ? atoi(len) : ft->size); else OUT("  %s %s;\n", ft->c, name);
      free(name); free(kind); free(len);
    }
    OUT("} %s;\n\nstatic const db_field_meta_t %s_fields[] = {\n", type, meta);
    EACH_ELEMENT(field, t) if (elem(field, "field")) {
      char *name = attr(field, "name"), *kind = attr(field, "type"), *len = attr(field, "length"); const field_type_t *ft = field_type(kind);
      OUT("  { \"%s\", %s, offsetof(%s, %s), %d },\n", name, ft->db, type, name, eq(ft->c, "char") ? (len ? atoi(len) : ft->size) : 0);
      free(name); free(kind); free(len);
    }
    OUT("};\n");
    char up[128]; ident(up, sizeof(up), table, true); OUT("#define TABLE_%s %d\n", up, table_i++);
    free(table); free(model);
  }
  if (table_i) OUT("#define TABLE_COUNT %d\n\n", table_i);
  OUT("static const db_api_def_t %s_database_api = { NULL, 0, NULL, 0, NULL, 0 };\n\n", prefix);
}

static void table_name_from_source(char *out, size_t cap, const char *source) {
  const char *dot = source ? strchr(source, '.') : NULL; snprintf(out, cap, "%s", dot ? dot + 1 : nz(source, "table"));
  char *next = strchr(out, '.'); if (next) *next = 0;
}

// count_table_fields — count number of fields in a database table
static int count_table_fields(xmlNodePtr db, const char *table_name) {
  if (!db || !table_name) return 0;
  EACH_ELEMENT(t, db) if (elem(t, "table")) {
    char *name = attr(t, "name");
    if (eq(name, table_name)) {
      int count = 0;
      EACH_ELEMENT(field, t) if (elem(field, "field")) count++;
      free(name);
      return count;
    }
    free(name);
  }
  return 0;
}

static xmlNodePtr find_tableview_for_table(xmlNodePtr parent, const char *table) {
  EACH_ELEMENT(c, parent) {
    if (elem(c, "tableview")) {
      char *source = attrs_first(c, "source", "database");
      char found[128]; table_name_from_source(found, sizeof(found), source);
      free(source);
      if (eq(found, table)) return c;
    }
    xmlNodePtr nested = find_tableview_for_table(c, table);
    if (nested) return nested;
  }
  return NULL;
}

static xmlNodePtr find_tableview_by_name(xmlNodePtr parent, const char *name) {
  EACH_ELEMENT(c, parent) {
    if (elem(c, "tableview")) {
      char *found = attr(c, "name"); bool match = eq(found, name); free(found);
      if (match) return c;
    }
    xmlNodePtr nested = find_tableview_by_name(c, name);
    if (nested) return nested;
  }
  return NULL;
}

static bool table_relation(xmlNodePtr db, const char *table, int *field_index,
                           char *foreign_table, size_t table_cap,
                           char *foreign_field, size_t field_cap) {
  if (!db) return false;
  EACH_ELEMENT(t, db) if (elem(t, "table")) {
    char *name = attr(t, "name");
    bool match = eq(name, table); free(name);
    if (!match) continue;
    int index = 0;
    EACH_ELEMENT(field, t) if (elem(field, "field")) {
      char *relation = attr(field, "relation");
      if (relation && *relation) {
        char *dot = strchr(relation, '.');
        if (dot) {
          *dot = 0;
          snprintf(foreign_table, table_cap, "%s", relation);
          snprintf(foreign_field, field_cap, "%s", dot + 1);
          *field_index = index;
          free(relation);
          return true;
        }
      }
      free(relation); index++;
    }
  }
  return false;
}

static void emit_tableviews(FILE *f, xmlNodePtr parent, xmlNodePtr form_node,
                            xmlNodePtr database, const char *form) {
  EACH_ELEMENT(c, parent) {
    if (elem(c, "tableview")) {
      char *name = attr(c, "name"), *source = attrs_first(c, "source", "database");
      char param[256], table[128], table_id[128]; snprintf(param, sizeof(param), "%s_%s_tableview_params", form, nz(name, "unnamed"));
      table_name_from_source(table, sizeof(table), source); ident(table_id, sizeof(table_id), table, true);
      OUT("static const char *%s_fields[] = { ", param); EACH_ELEMENT(col, c) if (elem(col, "column")) { char *v = attr(col, "field"), q[ORIONC_STRING_SIZE]; cstr(q, sizeof(q), v); OUT("%s, ", q); free(v); } LINE("NULL };\n");
      OUT("static const char *%s_titles[] = { ", param); EACH_ELEMENT(col, c) if (elem(col, "column")) { char *v = attr(col, "title"), q[ORIONC_STRING_SIZE]; cstr(q, sizeof(q), v); OUT("%s, ", q); free(v); } LINE("NULL };\n");
      OUT("static const int %s_widths[] = { ", param); EACH_ELEMENT(col, c) if (elem(col, "column")) { char *v = attr(col, "width"); OUT("%d, ", v ? atoi(v) : 0); free(v); } LINE("-1 };\n");
      int relation_field = 0; char foreign_table[128] = {0}, foreign_field[128] = {0};
      xmlNodePtr master = NULL;
      char *master_name_attr = attr(c, "master");
      if (table_relation(database, table, &relation_field, foreign_table,
                         sizeof(foreign_table), foreign_field, sizeof(foreign_field))) {
        if (master_name_attr && *master_name_attr && !eq(master_name_attr, "none"))
          master = find_tableview_by_name(form_node, master_name_attr);
        else if (!master_name_attr || !*master_name_attr)
          master = find_tableview_for_table(form_node, foreign_table);
      }
      char master_id[256] = "0";
      if (master) {
        char *master_name = attr(master, "name");
        control_id(master_id, sizeof(master_id), form, master_name,
                   (char *)master->name, 0);
        free(master_name);
      }
      char master_key_q[ORIONC_STRING_SIZE];
      if (master && foreign_field[0]) cstr(master_key_q, sizeof(master_key_q), foreign_field);
      else snprintf(master_key_q, sizeof(master_key_q), "NULL");
      char *check_field = attrs_first(c, "check-field", "check_field");
      char check_field_q[ORIONC_STRING_SIZE];
      if (check_field && *check_field) cstr(check_field_q, sizeof(check_field_q), check_field);
      else snprintf(check_field_q, sizeof(check_field_q), "NULL");
      // LIMITATION: master_filter_field / filter_value are int-based,
      // so only integer FK values work.  String FKs (UUID, hash) would
      // be converted to 0 by the runtime's strtol() call.
      OUT("static const tableview_params_t %s = { .db = NULL, .table_id = TABLE_%s, .filter_field = 0, .filter_value = 0, .field_names = %s_fields, .column_titles = %s_titles, .column_widths = %s_widths, .check_field = %s, .master_id = %s, .master_filter_field = %d, .master_key = %s };\n\n",
          param, table_id, param, param, param, check_field_q, master_id,
          relation_field, master_key_q);
      free(check_field);
      free(master_name_attr);
      free(name); free(source);
    }
    emit_tableviews(f, c, form_node, database, form);
  }
}

static void emit_comboboxes(FILE *f, xmlNodePtr parent, const char *form) {
  EACH_ELEMENT(c, parent) {
    if (elem(c, "combobox")) {
      char *name = attr(c, "name"), *source = attr(c, "source");
      char *display = attr(c, "display"), *value = attr(c, "value");
      
      // Only generate params if source/display/value are all present
      if (source && display && value) {
        char param[256], table[128], table_id[128];
        char display_q[ORIONC_STRING_SIZE], value_q[ORIONC_STRING_SIZE];
        char filter_field_q[ORIONC_STRING_SIZE], filter_value_q[ORIONC_STRING_SIZE];
        char *filter_field = attr(c, "filter_field");
        char *filter_value_attr = attr(c, "filter_value");

        snprintf(param, sizeof(param), "%s_%s_combobox_params", form, nz(name, "unnamed"));
        table_name_from_source(table, sizeof(table), source);
        ident(table_id, sizeof(table_id), table, true);
        cstr(display_q, sizeof(display_q), display);
        cstr(value_q, sizeof(value_q), value);
        cstr(filter_field_q, sizeof(filter_field_q), filter_field);
        cstr(filter_value_q, sizeof(filter_value_q), filter_value_attr);

        OUT("static const combobox_params_t %s = { NULL, TABLE_%s, %s, %s, %s, %s };\n\n",
            param, table_id, display_q, value_q, filter_field_q, filter_value_q);
        free(filter_field); free(filter_value_attr);
      }

      free(name); free(source); free(display); free(value);
    }
    emit_comboboxes(f, c, form);
  }
}

static const char *binding_getter(const char *klass) {
  if (eq(klass, "textedit") || eq(klass, "multiedit")) return "edGetText";
  if (eq(klass, "combobox")) return "cbGetCurrentValue";  // Use value_field (ID) not row index
  if (eq(klass, "checkbox")) return "chkIsChecked";
  return "0";
}

static const kv_t kWindowFlags[] = {
  {"notitle", "WINDOW_NOTITLE"}, {"nofill", "WINDOW_NOFILL"},
  {"vscroll", "WINDOW_VSCROLL"}, {"flexspace", "WINDOW_FLEXSPACE"},
  {"toolbar", "WINDOW_TOOLBAR"}, {"statusbar", "WINDOW_STATUSBAR"},
  {"notitlebar", "WINDOW_NOTITLE"}, {"default", "0"},
};

static void resolve_flags(char *out, size_t cap, const char *raw) {
  if (!raw || !*raw) { snprintf(out, cap, "0"); return; }
  char buf[256]; snprintf(buf, sizeof(buf), "%s", raw);
  out[0] = 0;
  for (char *tok = strtok(buf, ",|+ "); tok; tok = strtok(NULL, ",|+ ")) {
    bool found = false;
    for (int i = 0; i < ARRAY_LEN(kWindowFlags); i++) {
      if (eq(tok, kWindowFlags[i].key)) {
        if (out[0]) strcat(out, " | ");
        strcat(out, kWindowFlags[i].value);
        found = true; break;
      }
    }
    if (!found) {
      if (out[0]) strcat(out, " | ");
      strcat(out, tok);
    }
  }
  if (!out[0]) snprintf(out, cap, "0");
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
    if (elem(c, "textbox"))
      snprintf(klass, sizeof(klass), "TextEdit");
    if (elem(c, "stack") || elem(c, "StackView"))
      snprintf(klass, sizeof(klass), "StackView");
    if (elem(c, "grid"))
      snprintf(klass, sizeof(klass), "GridView");
    if (elem(c, "flow"))
      snprintf(klass, sizeof(klass), "FlowView");
    if (has_controls(c) && !elem(c, "column")) sz = (rect_t){0};
    rect_attr(c, "padding", &pad) || rect_attr(c, "layout_padding", &pad); rect_attr(c, "margin", &mar) || rect_attr(c, "layout_margin", &mar);
    // Auto-add WINDOW_FLEXSPACE for space and multiedit elements (WPF-style)
    char resolved[256]; resolve_flags(resolved, sizeof(resolved), nz(a.v[A_FLAGS], "0"));
    const char *auto_flex = (elem(c, "space") || elem(c, "multiedit")) ? " | WINDOW_FLEXSPACE" : "";
    snprintf(flags, sizeof(flags), "(%s)%s%s", resolved, enum_parse_token(a.v[A_ORIENT], kOrient, ARRAY_LEN(kOrient), WINDOW_STACK_VERTICAL) & WINDOW_STACK_HORIZONTAL ? " | WINDOW_STACK_HORIZONTAL" : "", auto_flex);
    snprintf(spacing, sizeof(spacing), "%u", byte_attr(a.v[A_SPACING], ORIONC_DEFAULT_SPACING));
    snprintf(font, sizeof(font), "%s", eq(a.v[A_FONT], "system") ? "FONT_SYSTEM" : eq(a.v[A_FONT], "icon") ? "FONT_ICON" : "FONT_SMALL");
    snprintf(color, sizeof(color), "%u", (unsigned)enum_parse_token(a.v[A_COLOR], kColors, ARRAY_LEN(kColors), brTextNormal));
    if (elem(c, "tableview")) snprintf(lparam, sizeof(lparam), "&%s_%s_tableview_params", form, nz(a.v[A_NAME], "unnamed"));
    if (elem(c, "combobox") && attr(c, "source")) snprintf(lparam, sizeof(lparam), "&%s_%s_combobox_params", form, nz(a.v[A_NAME], "unnamed"));
    if (elem(c, "SplitView")) snprintf(lparam, sizeof(lparam), "(void *)%s", eq(a.v[A_ORIENT], "vertical") ? "SPLIT_HORZ" : "SPLIT_VERT");
    if (a.v[A_FIELD]) add_binding(bindings, id, a.v[A_FIELD], klass);
    
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
    
    char *context = attrs_first(c, "context-menu", "context_menu"), context_items[192] = "NULL", context_count[192] = "0";
    if (context && *context) { char context_id[128]; ident(context_id, sizeof(context_id), context, true); snprintf(context_items, sizeof(context_items), "CONTEXT_MENU_%s_ITEMS", context_id); snprintf(context_count, sizeof(context_count), "CONTEXT_MENU_%s_COUNT", context_id); }
    cstr(classq, sizeof(classq), klass); cstr(textq, sizeof(textq), a.v[A_TEXT]); cstr(nameq, sizeof(nameq), a.v[A_NAME]);
    OUT("  { %s, %s, { %d, %d }, %s, %s, %s, %u, %u, NULL, 0, %s, { %d, %d, %d, %d }, { %d, %d, %d, %d }, %s, %s, %s, %s, %s, %s, %s, %s },\n",
        classq, id, sz.w, sz.h, flags, textq, nameq,
        (unsigned)enum_parse_token(a.v[A_HA], kAlignH, ARRAY_LEN(kAlignH), 0),
        (unsigned)enum_parse_token(a.v[A_VA], kAlignV, ARRAY_LEN(kAlignV), 0),
        spacing, pad.x, pad.y, pad.w, pad.h, mar.x, mar.y, mar.w, mar.h,
        nz(parent_id, "0"), font, a.v[A_FONT] ? "true" : "false", color, a.v[A_COLOR] ? "true" : "false", lparam, context_items, context_count);
    (*count)++;
    emit_controls_ex(f, c, form, id, bindings, count, btn_ids);
    free(context); free_attrs(&a);
  }
}

static void emit_bindings(FILE *f, const char *prefix, const char *form, const bindings_t *b) {
  if (!b || !b->n) return;
  char type[128]; binding_record_type(type, sizeof(type), b->table);
  OUT("static const ctrl_binding_t %s_%s_bindings[] = {\n", prefix, form);
  for (int i = 0; i < b->n; i++) {
    const binding_t *x = &b->v[i];
    OUT("  { %s, 0, %s, offsetof(%s, %s), ", x->ctrl, binding_getter(x->klass), type, x->field);
    if (eq(x->klass, "textedit") || eq(x->klass, "multiedit")) OUT("sizeof_field(%s, %s)", type, x->field);
    else if (eq(x->klass, "combobox")) LINE("(size_t)-1");
    else LINE("0");
    LINE(", NULL, NULL },\n");
  }
  LINE("};\n\n");
}

static bool emit_form(FILE *f, xmlNodePtr form, const char *prefix, xmlNodePtr database) {
  char *name = attr(form, "name"), *title = attr(form, "title"), *flags_raw = attr(form, "flags"), *toolbar = attr(form, "toolbar"), *spacing = attrs_first(form, "spacing", "layout_spacing");
  rect_t sz = size_attr(form), pad = {0}, mar = {0}; char form_id[128], titleq[ORIONC_STRING_SIZE], flags[256];
  if (!sz.w) { fprintf(stderr, "orionc_alt: form '%s' requires width=\n", nz(name, "")); return false; }
  ident(form_id, sizeof(form_id), name, false); cstr(titleq, sizeof(titleq), nz(title, name));
  rect_attr(form, "padding", &pad) || rect_attr(form, "layout_padding", &pad); rect_attr(form, "margin", &mar) || rect_attr(form, "layout_margin", &mar);
  resolve_flags(flags, sizeof(flags), nz(flags_raw, "0"));
  emit_tableviews(f, form, form, database, form_id);
  emit_comboboxes(f, form, form_id);
  OUT("static const form_ctrl_def_t %s_%s_children[] = {\n", prefix, form_id);
  int count = 0; bindings_t bindings = {0}; button_ids_t btn_ids = {0}; 
  emit_controls_ex(f, form, form_id, "0", &bindings, &count, &btn_ids); LINE("};\n\n");
  emit_bindings(f, prefix, form_id, &bindings);
  OUT("static const form_def_t %s_%s_form = { .name = %s, .width = %d, .height = %d, .flags = (%s) | WINDOW_AUTO_LAYOUT, .layout_spacing = %u, .padding = { %d, %d, %d, %d }, .margin = { %d, %d, %d, %d }, .children = %s_%s_children, .child_count = %d",
      prefix, form_id, titleq, sz.w, sz.h, flags, byte_attr(spacing, ORIONC_DEFAULT_SPACING),
      pad.x, pad.y, pad.w, pad.h, mar.x, mar.y, mar.w, mar.h, prefix, form_id, count);
  if (toolbar && *toolbar) { char tb[128]; ident(tb, sizeof(tb), toolbar, true); OUT(", .toolbar_items = TB_%s, .toolbar_count = TB_%s_COUNT", tb, tb); }
  else LINE(", .toolbar_items = NULL, .toolbar_count = 0");
  if (bindings.n) {
    char table_id[128], meta[128]; 
    ident(table_id, sizeof(table_id), bindings.table, true);
    ident(meta, sizeof(meta), bindings.table, false);
    int field_count = count_table_fields(database, bindings.table);
    OUT(", .bindings = %s_%s_bindings, .binding_count = %d, .ok_id = %s, .cancel_id = %s, .db_name = \"%s\", .db_table = \"%s\", .db_table_id = TABLE_%s, .db_fields = %s_fields, .db_field_count = %d",
        prefix, form_id, bindings.n, 
        btn_ids.ok_id[0] ? btn_ids.ok_id : "0",
        btn_ids.cancel_id[0] ? btn_ids.cancel_id : "0",
        bindings.db, bindings.table, table_id, meta, field_count);
  } else {
    OUT(", .bindings = NULL, .binding_count = 0, .ok_id = %s, .cancel_id = %s, .db_name = NULL, .db_table = NULL, .db_table_id = 0, .db_fields = NULL, .db_field_count = 0",
        btn_ids.ok_id[0] ? btn_ids.ok_id : "0",
        btn_ids.cancel_id[0] ? btn_ids.cancel_id : "0");
  }
  LINE(" };\n\n");
  free(name); free(title); free(flags_raw); free(toolbar); free(spacing);
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
  xmlNodePtr menus = child(root, "menus"), contexts = child(root, "contextmenus"), toolbars = child(root, "toolbars"), forms = child(root, "forms"), databases = child(root, "databases"), database = databases ? child(databases, "database") : child(root, "database");
  ids_t commands = {0}, controls = {0}; collect_command_ids(&commands, menus, toolbars); collect_context_ids(&commands, contexts);
  EACH_ELEMENT(form, forms) if (elem(form, "form")) { char *name = attr(form, "name"), form_id[128]; if (!only || eq(name, only)) { ident(form_id, sizeof(form_id), name, false); collect_control_ids(&controls, form, form_id); } free(name); }
  tpl(f, "/* Generated by orionc_alt from {{input}}. */\n#ifndef {{guard}}\n#define {{guard}}\n\n#include <orion/ui.h>\n#include <orion/user/icons.h>\n\n",
      (kv_t[]){{"input", input}, {"guard", guard}, {NULL, NULL}});
  emit_defines(f, &commands, "ID_COMMAND_BASE"); emit_defines(f, &controls, "ID_CONTROL_BASE");
  emit_menus(f, menus); emit_context_menus(f, contexts); emit_toolbars(f, toolbars); emit_database(f, database, pre);
  EACH_ELEMENT(form, forms) if (elem(form, "form")) { char *name = attr(form, "name"); if (!only || eq(name, only)) { if (!emit_form(f, form, pre, database)) return 1; } free(name); }
  OUT("#endif /* %s */\n", guard);
  fclose(f); xmlFreeDoc(doc);
  return 0;
}
