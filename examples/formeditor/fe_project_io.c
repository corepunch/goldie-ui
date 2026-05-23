// Project XML I/O for the Orion Form Editor.
// Extracted from win_menubar.c to isolate .orion file load/save logic.

#include "formeditor.h"
#include "fe_project_io.h"
#include "fe_document.h"
#include "fe_layout.h"
#include "../../commctl/commctl.h"
#include "../../user/enum_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

// ============================================================
// External dependencies
// ============================================================

extern app_state_t *g_app;
extern void forms_browser_refresh(void);
extern void plugins_browser_refresh(void);
extern void databases_browser_refresh(void);
extern void form_doc_show_only(form_doc_t *doc);
extern void formeditor_rebuild_tool_palette(void);
extern form_doc_t *create_form_doc(int w, int h);
extern void close_form_doc(form_doc_t *doc);
extern void form_doc_update_title(form_doc_t *doc);
extern void canvas_rebuild_live_controls(form_doc_t *doc);
extern void canvas_sync_live_controls(form_doc_t *doc);
extern irect16_t form_doc_frame_for_size(int w, int h, flags_t flags);
extern void resize_window(window_t *win, int w, int h);

// Apply window flags and size to the document window
static void form_doc_apply_window_flags_and_size(form_doc_t *doc) {
  if (!doc || !doc->doc_win) return;
  doc->doc_win->flags &= ~(WINDOW_TOOLBAR | WINDOW_STATUSBAR);
  doc->doc_win->flags |= (doc->flags & (WINDOW_TOOLBAR | WINDOW_STATUSBAR));
  irect16_t frame = form_doc_frame_for_size(doc->form_size.w, doc->form_size.h, doc->flags);
  resize_window(doc->doc_win, frame.w, frame.h);
  if (doc->canvas_win) {
    irect16_t cr = get_client_rect(doc->doc_win);
    resize_window(doc->canvas_win, cr.w, cr.h);
  }
}
extern bool fe_load_component_plugin(const char *path);

// ============================================================
// Control type class-name mapping
// ============================================================

// Map control type to a short keyword used in the file.
static const char *ctrl_type_class_name(int type) {
  const fe_component_desc_t *c = fe_component_by_id(type);
  return c ? c->class_name : "control";
}

static int ctrl_type_from_class_name(const char *class_name) {
  const fe_component_desc_t *c = fe_component_by_class_name(class_name);
  if (!c) return -1;
  for (int i = 0; i < fe_component_count(); i++) {
    const fe_component_desc_t *it = fe_component_at(i);
    if (it == c)
      return i;
  }
  return -1;
}

// ============================================================
// XML attribute helpers
// ============================================================

static bool has_ext(const char *path, const char *ext) {
  if (!path || !ext) return false;
  size_t n = strlen(path);
  size_t e = strlen(ext);
  if (n < e) return false;
  return strcmp(path + n - e, ext) == 0;
}

static char *xml_attr_dup(xmlNodePtr node, const char *name) {
  xmlChar *v = xmlGetProp(node, BAD_CAST name);
  if (!v) return NULL;
  char *s = strdup((const char *)v);
  xmlFree(v);
  return s;
}

static void copy_attr(xmlNodePtr node, const char *name, char *dst, size_t dst_sz) {
  char *v;
  if (!dst || dst_sz == 0) return;
  v = xml_attr_dup(node, name);
  if (!v) return;
  snprintf(dst, dst_sz, "%s", v);
  free(v);
}

static int int_attr(xmlNodePtr node, const char *name, int fallback) {
  char *v = xml_attr_dup(node, name);
  if (!v) return fallback;
  char *end = NULL;
  long n = strtol(v, &end, 0);
  int out = (end && *end == '\0') ? (int)n : fallback;
  free(v);
  return out;
}

// ============================================================
// Token tables for enum parsing
// ============================================================

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

static const enum_token_t kLayoutKindTokens[] = {
  {"none",  0},
  {"stack", 1},
  {"grid",  2},
};

static const enum_token_t kLayoutOrientationTokens[] = {
  {"vertical",   WINDOW_STACK_VERTICAL},
  {"horizontal", WINDOW_STACK_HORIZONTAL},
};

static const enum_token_t kFontTokens[] = {
  {"system", FONT_SYSTEM},
  {"small",  FONT_SMALL},
  {"icon",   FONT_ICON},
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

// ============================================================
// Enum attribute parsers
// ============================================================

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

static uint8_t layout_mode_attr(const char *v, uint8_t fallback) {
  return (uint8_t)enum_parse_token(v, kLayoutKindTokens, ARRAY_LEN(kLayoutKindTokens), fallback);
}

static flags_t layout_orientation_attr(const char *v, flags_t fallback) {
  return (flags_t)enum_parse_token(v, kLayoutOrientationTokens, ARRAY_LEN(kLayoutOrientationTokens), (int)fallback);
}

static const char *layout_mode_token(uint8_t kind) {
  return enum_token_name(kind, kLayoutKindTokens, ARRAY_LEN(kLayoutKindTokens), "none");
}

static const char *layout_orientation_token(flags_t orientation) {
  return enum_token_name(orientation, kLayoutOrientationTokens,
                         ARRAY_LEN(kLayoutOrientationTokens), "vertical");
}

static uint8_t font_attr(const char *v, uint8_t fallback) {
  return (uint8_t)enum_parse_token(v, kFontTokens, ARRAY_LEN(kFontTokens), fallback);
}

static const char *font_token(uint8_t font) {
  return enum_token_name(font, kFontTokens, ARRAY_LEN(kFontTokens), "small");
}

static uint8_t color_attr(const char *v, uint8_t fallback) {
  return (uint8_t)enum_parse_token(v, kColorTokens, ARRAY_LEN(kColorTokens), fallback);
}

static const char *color_token(uint8_t color) {
  return enum_token_name(color, kColorTokens, ARRAY_LEN(kColorTokens), "text-normal");
}

static db_field_type_t db_field_type_attr(const char *s) {
  if (!s || !*s) return DB_TYPE_STRING;
  if (!strcasecmp(s, "integer") || !strcasecmp(s, "int"))
    return DB_TYPE_INT;
  if (!strcasecmp(s, "string") || !strcasecmp(s, "text"))
    return DB_TYPE_STRING;
  if (!strcasecmp(s, "boolean") || !strcasecmp(s, "bool"))
    return DB_TYPE_BOOL;
  if (!strcasecmp(s, "float"))
    return DB_TYPE_FLOAT;
  if (!strcasecmp(s, "double"))
    return DB_TYPE_DOUBLE;
  return DB_TYPE_STRING;
}

static bool truthy_attr(const char *s) {
  return s && (!strcasecmp(s, "yes") || !strcasecmp(s, "true") ||
               !strcasecmp(s, "1"));
}

static void split_relation(const char *relation,
                           char *table, size_t table_sz,
                           char *field, size_t field_sz) {
  if (table && table_sz) table[0] = '\0';
  if (field && field_sz) field[0] = '\0';
  if (!relation || !*relation)
    return;
  const char *dot = strchr(relation, '.');
  if (!dot) {
    if (table && table_sz) snprintf(table, table_sz, "%s", relation);
    return;
  }
  if (table && table_sz) {
    size_t n = (size_t)(dot - relation);
    if (n >= table_sz) n = table_sz - 1;
    memcpy(table, relation, n);
    table[n] = '\0';
  }
  if (field && field_sz)
    snprintf(field, field_sz, "%s", dot + 1);
}

static void relation_alias(char *out, size_t out_sz, const char *field_name,
                           const char *foreign_table) {
  if (!out || out_sz == 0) return;
  if (field_name && *field_name) {
    snprintf(out, out_sz, "%s", field_name);
    size_t n = strlen(out);
    if (n > 3 && strcmp(out + n - 3, "_id") == 0)
      out[n - 3] = '\0';
    return;
  }
  snprintf(out, out_sz, "%s", foreign_table ? foreign_table : "");
}

// ============================================================
// Flags parsing
// ============================================================

static uint32_t flag_value(const char *tok) {
  if (!tok || !*tok || strcmp(tok, "0") == 0) return 0;
  if (strcmp(tok, "BUTTON_DEFAULT") == 0) return BUTTON_DEFAULT;
  if (strcmp(tok, "WINDOW_NOTITLE") == 0) return WINDOW_NOTITLE;
  if (strcmp(tok, "WINDOW_TRANSPARENT") == 0) return WINDOW_TRANSPARENT;
  if (strcmp(tok, "WINDOW_NOFILL") == 0) return WINDOW_NOFILL;
  if (strcmp(tok, "WINDOW_ALWAYSONTOP") == 0) return WINDOW_ALWAYSONTOP;
  if (strcmp(tok, "WINDOW_ALWAYSINBACK") == 0) return WINDOW_ALWAYSINBACK;
  if (strcmp(tok, "WINDOW_HIDDEN") == 0) return WINDOW_HIDDEN;
  if (strcmp(tok, "WINDOW_NOTABSTOP") == 0) return WINDOW_NOTABSTOP;
  if (strcmp(tok, "WINDOW_TOOLBAR") == 0) return WINDOW_TOOLBAR;
  if (strcmp(tok, "WINDOW_STATUSBAR") == 0) return WINDOW_STATUSBAR;
  if (strcmp(tok, "WINDOW_DIALOG") == 0) return WINDOW_DIALOG;
  if (strcmp(tok, "WINDOW_NOTRAYBUTTON") == 0) return WINDOW_NOTRAYBUTTON;
  if (strcmp(tok, "WINDOW_NORESIZE") == 0) return WINDOW_NORESIZE;
  if (strcmp(tok, "WINDOW_NOACTIVATE") == 0) return WINDOW_NOACTIVATE;
  if (strcmp(tok, "WINDOW_HSCROLL") == 0) return WINDOW_HSCROLL;
  if (strcmp(tok, "WINDOW_VSCROLL") == 0) return WINDOW_VSCROLL;
  if (strcmp(tok, "WINDOW_STACK_HORIZONTAL") == 0) return WINDOW_STACK_HORIZONTAL;
  if (strcmp(tok, "WINDOW_STACK_VERTICAL") == 0) return WINDOW_STACK_VERTICAL;
  if (strcmp(tok, "WINDOW_FLEXSPACE") == 0) return WINDOW_FLEXSPACE;
  if (strcmp(tok, "WINDOW_AUTO_LAYOUT") == 0) return WINDOW_AUTO_LAYOUT;
  if (strcmp(tok, "WINDOW_LAYOUT_CONTAINER") == 0) return WINDOW_LAYOUT_CONTAINER;
  if (strcmp(tok, "WINDOW_SIDEBAR") == 0) return WINDOW_SIDEBAR;
  if (strcmp(tok, "BUTTON_PUSHLIKE") == 0) return BUTTON_PUSHLIKE;
  if (strcmp(tok, "BUTTON_AUTORADIO") == 0) return BUTTON_AUTORADIO;
  char *end = NULL;
  unsigned long n = strtoul(tok, &end, 0);
  return (end && *end == '\0') ? (uint32_t)n : 0;
}

static uint32_t parse_flags_expr(const char *expr) {
  if (!expr || !*expr) return 0;
  char buf[256];
  snprintf(buf, sizeof(buf), "%s", expr);
  uint32_t flags = 0;
  char *p = buf;
  while (*p) {
    while (*p == ' ' || *p == '\t' || *p == '|') p++;
    char *start = p;
    while (*p && *p != '|') p++;
    char save = *p;
    *p = '\0';
    char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
    flags |= flag_value(start);
    if (!save) break;
    p++;
  }
  return flags;
}

// ============================================================
// Plugin loading
// ============================================================

static bool load_component_plugin_named(const char *name) {
  if (!name || !*name) return false;
  if (strchr(name, '/') || has_ext(name, AX_DYNLIB_EXT))
    return fe_load_component_plugin(name);
  char path[4096];
  int n = snprintf(path, sizeof(path), "%s/../lib/%s%s",
                   ui_get_exe_dir(), name, AX_DYNLIB_EXT);
  if (n <= 0 || (size_t)n >= sizeof(path)) return false;
  return fe_load_component_plugin(path);
}

// ============================================================
// Rectangle attribute parsing
// ============================================================

static irect16_t rect_attr(xmlNodePtr node, const char *name, irect16_t fallback) {
  char *v = xml_attr_dup(node, name);
  if (!v) return fallback;
  int a = fallback.x, b = fallback.y, c = fallback.w, d = fallback.h;
  int n = sscanf(v, "%d %d %d %d", &a, &b, &c, &d);
  free(v);
  switch (n) {
    case 1: return (irect16_t){a, a, a, a};
    case 2: return (irect16_t){a, b, a, b};
    case 4: return (irect16_t){a, b, c, d};
    default: return fallback;
  }
}

// ============================================================
// Project reset
// ============================================================

static void project_reset(void) {
  if (!g_app) return;
  while (g_app->docs)
    close_form_doc(g_app->docs);
  memset(&g_app->project, 0, sizeof(g_app->project));
}

// ============================================================
// Project loading helpers
// ============================================================

static void project_load_plugins(xmlNodePtr root) {
  if (!g_app) return;
  for (xmlNodePtr n = root->children; n; n = n->next) {
    if (n->type != XML_ELEMENT_NODE || xmlStrcmp(n->name, BAD_CAST "plugins") != 0)
      continue;
    for (xmlNodePtr p = n->children; p; p = p->next) {
      if (p->type != XML_ELEMENT_NODE || xmlStrcmp(p->name, BAD_CAST "plugin") != 0)
        continue;
      if (g_app->project.plugin_count >= FE_MAX_PROJECT_PLUGINS) continue;
      form_plugin_ref_t *ref = &g_app->project.plugins[g_app->project.plugin_count];
      copy_attr(p, "name", ref->name, sizeof(ref->name));
      if (ref->name[0]) {
        load_component_plugin_named(ref->name);
        g_app->project.plugin_count++;
      }
    }
  }
}

static void project_load_menus(xmlDocPtr xdoc, xmlNodePtr root) {
  if (!g_app) return;
  g_app->project.menus_xml[0] = '\0';
  for (xmlNodePtr n = root->children; n; n = n->next) {
    if (n->type != XML_ELEMENT_NODE || xmlStrcmp(n->name, BAD_CAST "menus") != 0)
      continue;
    xmlBufferPtr buf = xmlBufferCreate();
    if (!buf) return;
    int ok = xmlNodeDump(buf, xdoc, n, 4, 1);
    if (ok >= 0) {
      snprintf(g_app->project.menus_xml, sizeof(g_app->project.menus_xml),
               "%s", (const char *)xmlBufferContent(buf));
    }
    xmlBufferFree(buf);
    return;
  }
}

static void project_add_relation_join(form_project_db_table_t *table,
                                      const form_project_db_field_t *field) {
  if (!table || !field || !field->relation_table[0] ||
      table->join_count >= FE_MAX_PROJECT_DB_JOINS)
    return;
  char alias[64];
  relation_alias(alias, sizeof(alias), field->name, field->relation_table);
  for (int i = 0; i < table->join_count; i++) {
    form_project_db_join_t *join = &table->joins[i];
    if (!strcmp(join->name, alias) &&
        !strcmp(join->local_field, field->name) &&
        !strcmp(join->foreign_table, field->relation_table) &&
        !strcmp(join->foreign_field, field->relation_field))
      return;
  }
  form_project_db_join_t *join = &table->joins[table->join_count++];
  snprintf(join->name, sizeof(join->name), "%s", alias);
  snprintf(join->local_field, sizeof(join->local_field), "%s", field->name);
  snprintf(join->foreign_table, sizeof(join->foreign_table), "%s", field->relation_table);
  snprintf(join->foreign_field, sizeof(join->foreign_field), "%s", field->relation_field);
}

static void project_load_database_node(xmlNodePtr db_node) {
  if (!g_app || !db_node || g_app->project.database_count >= FE_MAX_PROJECT_DATABASES)
    return;
  form_project_database_t *db =
      &g_app->project.databases[g_app->project.database_count++];
  memset(db, 0, sizeof(*db));
  copy_attr(db_node, "name", db->name, sizeof(db->name));
  copy_attr(db_node, "class", db->class_name, sizeof(db->class_name));
  copy_attr(db_node, "source", db->source_path, sizeof(db->source_path));

  int table_id = 0;
  for (xmlNodePtr t = db_node->children; t; t = t->next) {
    if (t->type != XML_ELEMENT_NODE || xmlStrcmp(t->name, BAD_CAST "table") != 0)
      continue;
    if (db->table_count >= FE_MAX_PROJECT_DB_TABLES)
      break;
    form_project_db_table_t *table = &db->tables[db->table_count++];
    memset(table, 0, sizeof(*table));
    table->table_id = table_id++;
    copy_attr(t, "name", table->name, sizeof(table->name));
    copy_attr(t, "model", table->model, sizeof(table->model));

    for (xmlNodePtr f = t->children; f; f = f->next) {
      if (f->type != XML_ELEMENT_NODE || xmlStrcmp(f->name, BAD_CAST "field") != 0)
        continue;
      if (table->field_count >= FE_MAX_PROJECT_DB_FIELDS)
        break;
      form_project_db_field_t *field = &table->fields[table->field_count++];
      memset(field, 0, sizeof(*field));
      copy_attr(f, "name", field->name, sizeof(field->name));
      char *type = xml_attr_dup(f, "type");
      field->type = db_field_type_attr(type);
      field->length = int_attr(f, "length", 0);
      char *key = xml_attr_dup(f, "key");
      field->primary_key = truthy_attr(key);
      char *relation = xml_attr_dup(f, "relation");
      split_relation(relation, field->relation_table, sizeof(field->relation_table),
                     field->relation_field, sizeof(field->relation_field));
      project_add_relation_join(table, field);
      free(type);
      free(key);
      free(relation);
    }
  }
}

static void project_load_tableview_columns(form_element_t *el, xmlNodePtr table_node) {
  if (!el || !table_node)
    return;
  for (xmlNodePtr c = table_node->children; c; c = c->next) {
    if (c->type != XML_ELEMENT_NODE || xmlStrcmp(c->name, BAD_CAST "Column") != 0)
      continue;
    if (el->db_column_count >= FE_MAX_TABLE_COLUMNS)
      break;
    int idx = el->db_column_count++;
    copy_attr(c, "field", el->db_column_fields[idx], sizeof(el->db_column_fields[idx]));
    copy_attr(c, "title", el->db_column_titles[idx], sizeof(el->db_column_titles[idx]));
    el->db_column_widths[idx] = int_attr(c, "width", 0);
  }
}

static void project_load_databases(xmlDocPtr xdoc, xmlNodePtr root) {
  if (!g_app) return;
  g_app->project.database_count = 0;
  g_app->project.databases_xml[0] = '\0';
  for (xmlNodePtr n = root ? root->children : NULL; n; n = n->next) {
    if (n->type != XML_ELEMENT_NODE)
      continue;
    if (xmlStrcmp(n->name, BAD_CAST "database") == 0) {
      project_load_database_node(n);
      if (!g_app->project.databases_xml[0]) {
        xmlBufferPtr buf = xmlBufferCreate();
        if (buf) {
          int ok = xmlNodeDump(buf, xdoc, n, 4, 1);
          if (ok >= 0)
            snprintf(g_app->project.databases_xml, sizeof(g_app->project.databases_xml),
                     "%s", (const char *)xmlBufferContent(buf));
          xmlBufferFree(buf);
        }
      }
    } else if (xmlStrcmp(n->name, BAD_CAST "databases") == 0) {
      xmlBufferPtr buf = xmlBufferCreate();
      if (buf) {
        int ok = xmlNodeDump(buf, xdoc, n, 4, 1);
        if (ok >= 0)
          snprintf(g_app->project.databases_xml, sizeof(g_app->project.databases_xml),
                   "%s", (const char *)xmlBufferContent(buf));
        xmlBufferFree(buf);
      }
      for (xmlNodePtr db = n->children; db; db = db->next) {
        if (db->type == XML_ELEMENT_NODE && xmlStrcmp(db->name, BAD_CAST "database") == 0)
          project_load_database_node(db);
      }
    }
  }
}

static void project_load_controls(form_doc_t *doc, xmlNodePtr node, uint32_t parent_id) {
  for (xmlNodePtr n = node ? node->children : NULL; n; n = n->next) {
    if (n->type != XML_ELEMENT_NODE)
      continue;
    if (xmlStrcmp(n->name, BAD_CAST "requires") == 0)
      continue;
    int type = ctrl_type_from_class_name((const char *)n->name);
    uint32_t child_parent_id = parent_id;

    if (type >= 0 && type < FE_MAX_COMPONENTS && doc->element_count < MAX_ELEMENTS) {
      form_element_t *el = &doc->elements[doc->element_count++];
      memset(el, 0, sizeof(*el));
      el->type = type;
      el->h_align = LAYOUT_ALIGN_STRETCH;
      el->v_align = LAYOUT_ALIGN_STRETCH;
      el->color = brTextNormal;
      copy_attr(n, "id", el->id_expr, sizeof(el->id_expr));
      el->id = fe_doc_resolve_control_id(doc, el->id_expr);
      el->frame.x = int_attr(n, "x", 0);
      el->frame.y = int_attr(n, "y", 0);
      el->frame.w = int_attr(n, "width", int_attr(n, "w", 0));
      el->frame.h = int_attr(n, "height", int_attr(n, "h", 0));
      el->parent = (uint32_t)int_attr(n, "parent", (int)parent_id);
      child_parent_id = (uint32_t)el->id;
      copy_attr(n, "flags", el->flags_expr, sizeof(el->flags_expr));
      el->flags = parse_flags_expr(el->flags_expr);
      char *orientation = xml_attr_dup(n, "orientation");
      if (!orientation)
        orientation = xml_attr_dup(n, "layout_orientation");
      if (orientation) {
        if (layout_orientation_attr(orientation, WINDOW_STACK_VERTICAL) & WINDOW_STACK_HORIZONTAL)
          el->flags |= WINDOW_STACK_HORIZONTAL;
        else
          el->flags &= ~WINDOW_STACK_HORIZONTAL;
      }
      el->layout_spacing = (uint8_t)int_attr(n, "spacing",
                                             int_attr(n, "layout_spacing", 0));
      copy_attr(n, "text", el->text, sizeof(el->text));
      copy_attr(n, "name", el->name, sizeof(el->name));
      char *font = xml_attr_dup(n, "font");
      el->font = font_attr(font, FONT_SMALL);
      el->font_set = (font != NULL);
      char *color = xml_attr_dup(n, "color");
      el->color = color_attr(color, brTextNormal);
      el->color_set = (color != NULL);
      if (!el->id_expr[0]) {
        fe_doc_make_control_id_expr(el->id_expr, sizeof(el->id_expr),
                             doc->form_id, el->name,
                             ctrl_type_class_name(el->type), doc->element_count);
      }
      char *h_align = xml_attr_dup(n, "h-align");
      char *v_align = xml_attr_dup(n, "v-align");
      if (!h_align) h_align = xml_attr_dup(n, "h_align");
      if (!v_align) v_align = xml_attr_dup(n, "v_align");
      el->h_align = align_h_attr(h_align, el->h_align);
      el->v_align = align_v_attr(v_align, el->v_align);
      el->padding = rect_attr(n, "padding",
                              rect_attr(n, "layout_padding", (irect16_t){0, 0, 0, 0}));
      el->margin = rect_attr(n, "margin",
                              rect_attr(n, "layout_margin", (irect16_t){0, 0, 0, 0}));
      // Database binding attributes (NeXTSTEP DBKit-style)
      copy_attr(n, "field", el->db_field, sizeof(el->db_field));
      copy_attr(n, "source", el->db_source, sizeof(el->db_source));
      copy_attr(n, "display", el->db_display, sizeof(el->db_display));
      copy_attr(n, "value", el->db_value, sizeof(el->db_value));
      if (strcmp(ctrl_type_class_name(el->type), "TableView") == 0)
        project_load_tableview_columns(el, n);
      free(font);
      free(color);
      free(h_align);
      free(v_align);
      free(orientation);
      // Note: default sizes for auto-layout are applied in fe_layout_reflow,
      // not here, so we preserve loaded w/h for fixed-layout forms
      if (!el->name[0])
        snprintf(el->name, sizeof(el->name), "control%d", doc->element_count);
      if (el->id >= doc->next_id)
        doc->next_id = el->id + 1;
      if (type >= 0 && type < FE_MAX_COMPONENTS)
        doc->type_counters[type]++;
    }

    if (type >= 0 && strcmp(ctrl_type_class_name(type), "TableView") == 0)
      continue;
    if (n->children)
      project_load_controls(doc, n, child_parent_id);
  }
}

static void project_load_requires(form_doc_t *doc, xmlNodePtr form_node) {
  for (xmlNodePtr n = form_node->children; n; n = n->next) {
    if (n->type != XML_ELEMENT_NODE || xmlStrcmp(n->name, BAD_CAST "requires") != 0)
      continue;
    copy_attr(n, "library", doc->required_plugin, sizeof(doc->required_plugin));
    return;
  }
}

static bool project_load_form_node(xmlNodePtr form_node) {
  int w = int_attr(form_node, "width", FORM_DEFAULT_W);
  int h = int_attr(form_node, "height", FORM_DEFAULT_H);
  form_doc_t *doc = create_form_doc(w, h);
  if (!doc) return false;

  copy_attr(form_node, "name", doc->form_id, sizeof(doc->form_id));
  if (!doc->form_id[0])
    copy_attr(form_node, "id", doc->form_id, sizeof(doc->form_id));
  copy_attr(form_node, "title", doc->form_title, sizeof(doc->form_title));
  char flags_expr[128] = {0};
  copy_attr(form_node, "flags", flags_expr, sizeof(flags_expr));
  doc->flags = parse_flags_expr(flags_expr);
  // auto_layout defaults to true, but will be set to false if any element has non-zero x/y
  doc->flags |= WINDOW_AUTO_LAYOUT;
  {
    char *layout_mode = xml_attr_dup(form_node, "layout_mode");
    char *layout_orientation = xml_attr_dup(form_node, "orientation");
    if (!layout_orientation)
      layout_orientation = xml_attr_dup(form_node, "layout_orientation");
    doc->layout_mode = layout_mode_attr(layout_mode,
                                        (doc->flags & WINDOW_AUTO_LAYOUT) ? 1 : 0);
    if (layout_orientation_attr(layout_orientation, WINDOW_STACK_VERTICAL) & WINDOW_STACK_HORIZONTAL)
      doc->flags |= WINDOW_STACK_HORIZONTAL;
    else
      doc->flags &= ~WINDOW_STACK_HORIZONTAL;
    free(layout_mode);
    free(layout_orientation);
  }
  doc->layout_columns = (uint8_t)int_attr(form_node, "layout_columns", 0);
  doc->layout_spacing = (uint8_t)int_attr(form_node, "spacing",
                                          int_attr(form_node, "layout_spacing", 0));
  doc->padding = rect_attr(form_node, "padding",
                           rect_attr(form_node, "layout_padding", (irect16_t){0, 0, 0, 0}));
  doc->margin = rect_attr(form_node, "margin",
                          rect_attr(form_node, "layout_margin", (irect16_t){0, 0, 0, 0}));
  // Database binding context (form-level)
  copy_attr(form_node, "database", doc->database_name, sizeof(doc->database_name));
  copy_attr(form_node, "table", doc->table_name, sizeof(doc->table_name));
  project_load_requires(doc, form_node);
  project_load_controls(doc, form_node, 0);
  
  // Detect fixed-layout forms: if any element has non-zero x/y, disable auto-layout
  for (int i = 0; i < doc->element_count; i++) {
    if (doc->elements[i].frame.x != 0 || doc->elements[i].frame.y != 0) {
      doc->flags &= ~WINDOW_AUTO_LAYOUT;
      break;
    }
  }
  
  form_doc_apply_window_flags_and_size(doc);
  canvas_rebuild_live_controls(doc);
  doc->modified = false;
  form_doc_update_title(doc);
  return true;
}

static void project_load_forms(xmlNodePtr root) {
  for (xmlNodePtr n = root->children; n; n = n->next) {
    if (n->type != XML_ELEMENT_NODE || xmlStrcmp(n->name, BAD_CAST "forms") != 0)
      continue;
    for (xmlNodePtr f = n->children; f; f = f->next) {
      if (f->type == XML_ELEMENT_NODE && xmlStrcmp(f->name, BAD_CAST "form") == 0)
        project_load_form_node(f);
    }
  }
}

// ============================================================
// XML write helpers
// ============================================================

static void xml_write_escaped(FILE *f, const char *s) {
  for (const char *p = s ? s : ""; *p; p++) {
    switch (*p) {
      case '&':  fputs("&amp;", f); break;
      case '<':  fputs("&lt;", f); break;
      case '>':  fputs("&gt;", f); break;
      case '"':  fputs("&quot;", f); break;
      default:   fputc(*p, f); break;
    }
  }
}

static void xml_attr(FILE *f, const char *name, const char *value) {
  fprintf(f, " %s=\"", name);
  xml_write_escaped(f, value);
  fputc('"', f);
}

// ============================================================
// Project saving helpers
// ============================================================

static void project_sync_doc_from_window_tree(form_doc_t *doc) {
  if (!doc)
    return;
  for (int i = 0; i < doc->element_count; i++) {
    form_element_t *el = &doc->elements[i];
    window_t *win = el->live_win;
    if (!win || !is_window(win))
      continue;
    el->frame = win->frame;
    el->parent = (win->parent && win->parent->editor_id != 0)
                   ? (uint32_t)win->parent->editor_id
                   : 0;
    el->layout_spacing = win->layout.layout_spacing;
    el->padding = win->layout.layout_padding;
    el->margin = win->layout.layout_margin;
  }
}

static void project_write_element_attrs(FILE *f, form_doc_t *doc,
                                        form_element_t *el) {
  xml_attr(f, "name", el->name);
  xml_attr(f, "text", el->text);
  if (el->parent != 0)
    fprintf(f, " parent=\"%u\"", (unsigned)el->parent);
  if (el->font_set || el->font != FONT_SMALL)
    xml_attr(f, "font", font_token(el->font));
  if (el->color_set || el->color != brTextNormal)
    xml_attr(f, "color", color_token(el->color));
  if (!(doc->flags & WINDOW_AUTO_LAYOUT))
    fprintf(f, " x=\"%d\" y=\"%d\"", el->frame.x, el->frame.y);
  fprintf(f, " width=\"%d\" height=\"%d\"", el->frame.w, el->frame.h);
  fprintf(f, " h-align=\"%s\"", align_h_token(el->h_align));
  fprintf(f, " v-align=\"%s\"", align_v_token(el->v_align));
  if (el->layout_spacing != 0)
    fprintf(f, " spacing=\"%u\"", (unsigned)el->layout_spacing);
  if (el->flags & WINDOW_STACK_HORIZONTAL)
    fprintf(f, " orientation=\"%s\"", layout_orientation_token(WINDOW_STACK_HORIZONTAL));
  if (el->padding.x || el->padding.y || el->padding.w || el->padding.h)
    fprintf(f, " padding=\"%d %d %d %d\"",
            el->padding.x, el->padding.y, el->padding.w, el->padding.h);
  if (el->margin.x || el->margin.y || el->margin.w || el->margin.h)
    fprintf(f, " margin=\"%d %d %d %d\"",
            el->margin.x, el->margin.y, el->margin.w, el->margin.h);
  char flags_buf[32];
  snprintf(flags_buf, sizeof(flags_buf), "%" PRIu32, el->flags);
  xml_attr(f, "flags", el->flags_expr[0] ? el->flags_expr : flags_buf);
  if (el->db_field[0])
    xml_attr(f, "field", el->db_field);
  if (el->db_source[0])
    xml_attr(f, "source", el->db_source);
  if (el->db_display[0])
    xml_attr(f, "display", el->db_display);
  if (el->db_value[0])
    xml_attr(f, "value", el->db_value);
}

static void project_write_table_columns(FILE *f, const form_element_t *el) {
  if (!f || !el)
    return;
  for (int i = 0; i < el->db_column_count; i++) {
    fputs("          ", f);
    fprintf(f, "<Column");
    xml_attr(f, "field", el->db_column_fields[i]);
    xml_attr(f, "title", el->db_column_titles[i]);
    fprintf(f, " width=\"%d\" />\n", el->db_column_widths[i]);
  }
}

static void project_save_doc(FILE *f, form_doc_t *doc) {
  const char *label = doc->form_title[0] ? doc->form_title :
                      (doc->form_id[0] ? doc->form_id : "Untitled");
  fprintf(f, "      <form");
  xml_attr(f, "name", doc->form_id[0] ? doc->form_id : "form");
  xml_attr(f, "title", label);
  fprintf(f, "\n            width=\"%d\" height=\"%d\"\n            flags=\"%" PRIu32 "\"",
          doc->form_size.w, doc->form_size.h, doc->flags);
  if (doc->layout_mode == 2)
    fprintf(f, "\n            layout_mode=\"%s\"",
            layout_mode_token(doc->layout_mode));
  if (doc->flags & WINDOW_STACK_HORIZONTAL)
    fprintf(f, "\n            layout_orientation=\"%s\"",
        layout_orientation_token(doc->flags & WINDOW_STACK_HORIZONTAL));
  if (doc->layout_spacing != 0)
    fprintf(f, "\n            spacing=\"%u\"", (unsigned)doc->layout_spacing);
  if (doc->padding.x || doc->padding.y || doc->padding.w || doc->padding.h)
    fprintf(f, "\n            padding=\"%d %d %d %d\"",
            doc->padding.x, doc->padding.y, doc->padding.w, doc->padding.h);
  if (doc->margin.x || doc->margin.y || doc->margin.w || doc->margin.h)
    fprintf(f, "\n            margin=\"%d %d %d %d\"",
            doc->margin.x, doc->margin.y, doc->margin.w, doc->margin.h);
  // Database binding context (form-level)
  if (doc->database_name[0])
    xml_attr(f, "database", doc->database_name);
  if (doc->table_name[0])
    xml_attr(f, "table", doc->table_name);
  fprintf(f, ">\n");

  if (doc->required_plugin[0]) {
    fprintf(f, "        <requires");
    xml_attr(f, "library", doc->required_plugin);
    fprintf(f, " />\n");
  }
  project_sync_doc_from_window_tree(doc);
  for (int i = 0; i < doc->element_count; i++) {
    form_element_t *el = &doc->elements[i];
    fprintf(f, "        <%s", ctrl_type_class_name(el->type));
    project_write_element_attrs(f, doc, el);
    if (el->db_column_count > 0) {
      fprintf(f, ">\n");
      project_write_table_columns(f, el);
      fprintf(f, "        </%s>\n", ctrl_type_class_name(el->type));
    } else {
      fprintf(f, " />\n");
    }
  }
  fprintf(f, "      </form>\n");
}

// ============================================================
// Public API: Project load/save
// ============================================================

bool fe_project_load(const char *path) {
  xmlDocPtr xdoc = xmlReadFile(path, NULL, XML_PARSE_NONET);
  if (!xdoc) return false;
  xmlNodePtr root = xmlDocGetRootElement(xdoc);
  if (!root || xmlStrcmp(root->name, BAD_CAST "orion") != 0) {
    xmlFreeDoc(xdoc);
    return false;
  }

  project_reset();
  snprintf(g_app->project.filename, sizeof(g_app->project.filename), "%s", path);
  copy_attr(root, "name", g_app->project.name, sizeof(g_app->project.name));
  copy_attr(root, "title", g_app->project.title, sizeof(g_app->project.title));
  copy_attr(root, "root", g_app->project.root, sizeof(g_app->project.root));

  project_load_plugins(root);
  project_load_databases(xdoc, root);
  project_load_menus(xdoc, root);
  formeditor_rebuild_tool_palette();
  project_load_forms(root);

  g_app->project.loaded = true;
  g_app->project.modified = false;
  if (g_app->docs) form_doc_show_only(g_app->docs);
  forms_browser_refresh();
  plugins_browser_refresh();
  databases_browser_refresh();
  xmlFreeDoc(xdoc);
  return true;
}

bool fe_project_save(const char *path) {
  if (!g_app) return false;
  FILE *f = fopen(path, "w");
  if (!f) return false;
  form_project_t *p = &g_app->project;

  fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(f, "<orion version=\"1\"");
  if (p->name[0]) xml_attr(f, "name", p->name);
  if (p->title[0]) xml_attr(f, "title", p->title);
  if (p->root[0]) xml_attr(f, "root", p->root);
  fprintf(f, ">\n\n");

  fprintf(f, "    <plugins>\n");
  for (int i = 0; i < p->plugin_count; i++) {
    fprintf(f, "      <plugin");
    xml_attr(f, "name", p->plugins[i].name);
    fprintf(f, " />\n");
  }
  fprintf(f, "    </plugins>\n\n");

  if (p->databases_xml[0]) {
    fprintf(f, "%s\n\n", p->databases_xml);
  }

  if (p->menus_xml[0]) {
    fprintf(f, "%s\n\n", p->menus_xml);
  }

  fprintf(f, "    <forms>\n");
  for (form_doc_t *doc = g_app->docs; doc; doc = doc->next)
    project_save_doc(f, doc);
  fprintf(f, "    </forms>\n");
  fprintf(f, "</orion>\n");

  fclose(f);
  snprintf(p->filename, sizeof(p->filename), "%s", path);
  p->loaded = true;
  p->modified = false;
  for (form_doc_t *doc = g_app->docs; doc; doc = doc->next) {
    doc->modified = false;
    form_doc_update_title(doc);
  }
  return true;
}
