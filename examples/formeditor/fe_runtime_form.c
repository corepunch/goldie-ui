#include "formeditor.h"
#include "../../commctl/commctl.h"
#include <ctype.h>
#include <libxml/tree.h>

#define FE_RUNTIME_MAX_CONTROLS 512
#define FE_RUNTIME_MAX_ALLOCS 4096

typedef struct {
  tableview_params_t table;
  const char *fields[FE_MAX_TABLE_COLUMNS + 1];
  const char *titles[FE_MAX_TABLE_COLUMNS + 1];
  int widths[FE_MAX_TABLE_COLUMNS + 1];
  combobox_params_t combo;
} runtime_params_t;

typedef struct {
  runtime_params_t params[FE_RUNTIME_MAX_CONTROLS];
  int param_count;
  int next_generated_id;
  void *allocs[FE_RUNTIME_MAX_ALLOCS];
  int alloc_count;
} runtime_build_ctx_t;

static void *runtime_alloc(runtime_build_ctx_t *ctx, size_t size) {
  if (!ctx || size == 0 || ctx->alloc_count >= FE_RUNTIME_MAX_ALLOCS)
    return NULL;
  void *p = calloc(1, size);
  if (!p)
    return NULL;
  ctx->allocs[ctx->alloc_count++] = p;
  return p;
}

static char *runtime_strdup(runtime_build_ctx_t *ctx, const char *s) {
  if (!s)
    return NULL;
  size_t n = strlen(s) + 1;
  char *d = (char *)runtime_alloc(ctx, n);
  if (!d)
    return NULL;
  memcpy(d, s, n);
  return d;
}

static void runtime_release(runtime_build_ctx_t *ctx) {
  if (!ctx)
    return;
  for (int i = ctx->alloc_count - 1; i >= 0; i--)
    free(ctx->allocs[i]);
  ctx->alloc_count = 0;
  ctx->param_count = 0;
}

static int runtime_xml_attr_int(xmlNodePtr node, const char *name, int fallback) {
  xmlChar *v = xmlGetProp(node, BAD_CAST name);
  if (!v)
    return fallback;
  char *end = NULL;
  long n = strtol((const char *)v, &end, 0);
  xmlFree(v);
  if (!end || *end != '\0')
    return fallback;
  return (int)n;
}

static char *runtime_xml_attr_dup(runtime_build_ctx_t *ctx, xmlNodePtr node, const char *name) {
  xmlChar *v = xmlGetProp(node, BAD_CAST name);
  if (!v)
    return NULL;
  char *out = runtime_strdup(ctx, (const char *)v);
  xmlFree(v);
  return out;
}

static void runtime_xml_attr_copy(xmlNodePtr node, const char *name, char *dst, size_t dst_sz) {
  if (!dst || dst_sz == 0)
    return;
  dst[0] = '\0';
  xmlChar *v = xmlGetProp(node, BAD_CAST name);
  if (!v)
    return;
  snprintf(dst, dst_sz, "%s", (const char *)v);
  xmlFree(v);
}

static void parse_rect_attr(irect16_t *out, const char *s) {
  if (!out) return;
  *out = (irect16_t){0, 0, 0, 0};
  if (!s || !*s) return;
  int a = 0, b = 0, c = 0, d = 0;
  int n = sscanf(s, "%d %d %d %d", &a, &b, &c, &d);
  if (n == 1) {
    *out = (irect16_t){(int16_t)a, (int16_t)a, (int16_t)a, (int16_t)a};
  } else if (n == 2) {
    *out = (irect16_t){(int16_t)a, (int16_t)b, (int16_t)a, (int16_t)b};
  } else if (n == 4) {
    *out = (irect16_t){(int16_t)a, (int16_t)b, (int16_t)c, (int16_t)d};
  }
}

static int runtime_table_id_for_source(const char *source) {
  if (!source || !*source || !g_app)
    return -1;

  const char *table_name = source;
  const char *dot = strchr(source, '.');
  if (dot && dot[1])
    table_name = dot + 1;

  for (int d = 0; d < g_app->project.database_count; d++) {
    form_project_database_t *db = &g_app->project.databases[d];
    for (int t = 0; t < db->table_count; t++) {
      if (strcmp(db->tables[t].name, table_name) == 0)
        return db->tables[t].table_id;
    }
  }
  return -1;
}

static runtime_params_t *runtime_next_params(runtime_build_ctx_t *ctx) {
  if (!ctx || ctx->param_count >= FE_RUNTIME_MAX_CONTROLS)
    return NULL;
  runtime_params_t *p = &ctx->params[ctx->param_count++];
  memset(p, 0, sizeof(*p));
  return p;
}

static bool str_ieq(const char *a, const char *b) {
  if (!a || !b)
    return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
      return false;
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

static flags_t runtime_flag_from_name(const char *name) {
  if (!name || !*name) return 0;
  if (str_ieq(name, "WINDOW_AUTO_LAYOUT")) return WINDOW_AUTO_LAYOUT;
  if (str_ieq(name, "WINDOW_STACK_HORIZONTAL")) return WINDOW_STACK_HORIZONTAL;
  if (str_ieq(name, "WINDOW_STACK_VERTICAL")) return WINDOW_STACK_VERTICAL;
  if (str_ieq(name, "WINDOW_FLEXSPACE")) return WINDOW_FLEXSPACE;
  if (str_ieq(name, "WINDOW_VSCROLL")) return WINDOW_VSCROLL;
  if (str_ieq(name, "WINDOW_HSCROLL")) return WINDOW_HSCROLL;
  if (str_ieq(name, "WINDOW_NOTITLE")) return WINDOW_NOTITLE;
  if (str_ieq(name, "WINDOW_NOFILL")) return WINDOW_NOFILL;
  if (str_ieq(name, "WINDOW_DIALOG")) return WINDOW_DIALOG;
  if (str_ieq(name, "WINDOW_TOOLBAR")) return WINDOW_TOOLBAR;
  if (str_ieq(name, "WINDOW_STATUSBAR")) return WINDOW_STATUSBAR;
  if (str_ieq(name, "WINDOW_NORESIZE")) return WINDOW_NORESIZE;
  if (str_ieq(name, "WINDOW_NOTRAYBUTTON")) return WINDOW_NOTRAYBUTTON;
  if (str_ieq(name, "BUTTON_DEFAULT")) return BUTTON_DEFAULT;
  return 0;
}

static flags_t runtime_parse_flags(const char *expr) {
  if (!expr || !*expr)
    return 0;

  char *end = NULL;
  long numeric = strtol(expr, &end, 0);
  if (end && *end == '\0')
    return (flags_t)numeric;

  flags_t out = 0;
  const char *p = expr;
  while (*p) {
    while (*p && (isspace((unsigned char)*p) || *p == '|'))
      p++;
    if (!*p)
      break;

    char tok[96];
    int n = 0;
    while (*p && *p != '|') {
      if (n < (int)sizeof(tok) - 1)
        tok[n++] = *p;
      p++;
    }
    tok[n] = '\0';

    while (n > 0 && isspace((unsigned char)tok[n - 1]))
      tok[--n] = '\0';
    int i = 0;
    while (tok[i] && isspace((unsigned char)tok[i]))
      i++;
    if (tok[i])
      out |= runtime_flag_from_name(tok + i);
  }

  return out;
}

static uint8_t runtime_parse_align_h(const char *s) {
  if (!s || !*s) return LAYOUT_ALIGN_STRETCH;
  if (str_ieq(s, "left") || str_ieq(s, "start")) return LAYOUT_ALIGN_START;
  if (str_ieq(s, "center")) return LAYOUT_ALIGN_CENTER;
  if (str_ieq(s, "right") || str_ieq(s, "end")) return LAYOUT_ALIGN_END;
  return LAYOUT_ALIGN_STRETCH;
}

static uint8_t runtime_parse_align_v(const char *s) {
  if (!s || !*s) return LAYOUT_ALIGN_STRETCH;
  if (str_ieq(s, "top") || str_ieq(s, "start")) return LAYOUT_ALIGN_START;
  if (str_ieq(s, "center")) return LAYOUT_ALIGN_CENTER;
  if (str_ieq(s, "bottom") || str_ieq(s, "end")) return LAYOUT_ALIGN_END;
  return LAYOUT_ALIGN_STRETCH;
}

static uint8_t runtime_parse_font(const char *s, bool *set) {
  if (set) *set = false;
  if (!s || !*s)
    return FONT_SMALL;
  if (set) *set = true;
  if (str_ieq(s, "system")) return FONT_SYSTEM;
  if (str_ieq(s, "icon")) return FONT_ICON;
  return FONT_SMALL;
}

static uint8_t runtime_parse_color(const char *s, bool *set) {
  if (set) *set = false;
  if (!s || !*s)
    return brTransparent;
  if (set) *set = true;
  if (str_ieq(s, "text-normal")) return brTextNormal;
  if (str_ieq(s, "text-disabled")) return brTextDisabled;
  if (str_ieq(s, "text-error")) return brTextError;
  if (str_ieq(s, "text-success")) return brTextSuccess;
  if (str_ieq(s, "window-bg")) return brWindowBg;
  if (str_ieq(s, "workspace-bg")) return brWorkspaceBg;
  return brTransparent;
}

static void runtime_fill_table_params(runtime_build_ctx_t *ctx, xmlNodePtr node,
                                      const void **out_lparam) {
  runtime_params_t *rp = runtime_next_params(ctx);
  if (!rp || !out_lparam)
    return;

  char source[128] = {0};
  runtime_xml_attr_copy(node, "source", source, sizeof(source));
  if (!source[0])
    runtime_xml_attr_copy(node, "db_source", source, sizeof(source));

  rp->table.db = ui_get_database();
  rp->table.table_id = runtime_table_id_for_source(source);

  int col = 0;
  for (xmlNodePtr c = node->children; c && col < FE_MAX_TABLE_COLUMNS; c = c->next) {
    if (c->type != XML_ELEMENT_NODE)
      continue;
    if (xmlStrcasecmp(c->name, BAD_CAST "Column") != 0)
      continue;

    char *field = runtime_xml_attr_dup(ctx, c, "field");
    char *title = runtime_xml_attr_dup(ctx, c, "title");
    int width = runtime_xml_attr_int(c, "width", 80);

    if (!field)
      field = runtime_strdup(ctx, "id");
    if (!title)
      title = field;

    rp->fields[col] = field;
    rp->titles[col] = title;
    rp->widths[col] = width;
    col++;
  }

  if (col == 0) {
    rp->fields[0] = runtime_strdup(ctx, "id");
    rp->titles[0] = runtime_strdup(ctx, "ID");
    rp->widths[0] = 80;
    col = 1;
  }

  rp->widths[col] = -1;
  rp->table.field_names = rp->fields;
  rp->table.column_titles = rp->titles;
  rp->table.column_widths = rp->widths;
  *out_lparam = &rp->table;
}

static void runtime_fill_combo_params(runtime_build_ctx_t *ctx, xmlNodePtr node,
                                      const void **out_lparam) {
  runtime_params_t *rp = runtime_next_params(ctx);
  if (!rp || !out_lparam)
    return;

  char source[128] = {0};
  runtime_xml_attr_copy(node, "source", source, sizeof(source));
  if (!source[0])
    runtime_xml_attr_copy(node, "db_source", source, sizeof(source));

  char *display = runtime_xml_attr_dup(ctx, node, "display");
  if (!display)
    display = runtime_xml_attr_dup(ctx, node, "db_display");
  char *value = runtime_xml_attr_dup(ctx, node, "value");
  if (!value)
    value = runtime_xml_attr_dup(ctx, node, "db_value");

  rp->combo.db = ui_get_database();
  rp->combo.table_id = runtime_table_id_for_source(source);
  rp->combo.display_field = display;
  rp->combo.value_field = value;
  *out_lparam = &rp->combo;
}

static bool runtime_is_control_node(xmlNodePtr parent, xmlNodePtr node) {
  if (!node || node->type != XML_ELEMENT_NODE)
    return false;
  if (parent && xmlStrcasecmp(parent->name, BAD_CAST "TableView") == 0 &&
      xmlStrcasecmp(node->name, BAD_CAST "Column") == 0) {
    return false;
  }
  return true;
}

static const char *runtime_map_class_name(const char *xml_name) {
  if (!xml_name || !*xml_name)
    return "";
  if (str_ieq(xml_name, "TextBox"))
    return "TextEdit";
  return xml_name;
}

static int runtime_count_children(xmlNodePtr node) {
  int n = 0;
  for (xmlNodePtr c = node ? node->children : NULL; c; c = c->next) {
    if (runtime_is_control_node(node, c))
      n++;
  }
  return n;
}

static form_ctrl_def_t *runtime_build_defs(runtime_build_ctx_t *ctx, xmlNodePtr parent,
                                           int *out_count);

static void runtime_fill_def(runtime_build_ctx_t *ctx, xmlNodePtr node,
                             form_ctrl_def_t *out) {
  memset(out, 0, sizeof(*out));

  char *flags_expr = runtime_xml_attr_dup(ctx, node, "flags");
  char *text = runtime_xml_attr_dup(ctx, node, "text");
  char *name = runtime_xml_attr_dup(ctx, node, "name");
  char *h_align = runtime_xml_attr_dup(ctx, node, "h-align");
  if (!h_align) h_align = runtime_xml_attr_dup(ctx, node, "h_align");
  char *v_align = runtime_xml_attr_dup(ctx, node, "v-align");
  if (!v_align) v_align = runtime_xml_attr_dup(ctx, node, "v_align");
  char *font = runtime_xml_attr_dup(ctx, node, "font");
  char *color = runtime_xml_attr_dup(ctx, node, "color");
  char *padding = runtime_xml_attr_dup(ctx, node, "padding");
  char *margin = runtime_xml_attr_dup(ctx, node, "margin");
  char *orientation = runtime_xml_attr_dup(ctx, node, "orientation");
  if (!orientation) orientation = runtime_xml_attr_dup(ctx, node, "layout_orientation");

  out->class_name = runtime_strdup(ctx, (const char *)node->name);
  out->id = (uint32_t)runtime_xml_attr_int(node, "id", 0);
  out->size.w = (int16_t)runtime_xml_attr_int(node, "width", 0);
  out->size.h = (int16_t)runtime_xml_attr_int(node, "height", 0);
  out->flags = runtime_parse_flags(flags_expr);
  out->text = text ? text : "";
  out->name = name ? name : "";
  out->layout_spacing = (uint8_t)runtime_xml_attr_int(node, "spacing", 4);
  out->h_align = runtime_parse_align_h(h_align);
  out->v_align = runtime_parse_align_v(v_align);
  out->font = runtime_parse_font(font, &out->font_set);
  out->color = runtime_parse_color(color, &out->color_set);

  if (orientation && str_ieq(orientation, "horizontal"))
    out->flags |= WINDOW_STACK_HORIZONTAL;

  if (str_ieq((const char *)node->name, "Space") ||
      str_ieq((const char *)node->name, "MultiEdit")) {
    out->flags |= WINDOW_FLEXSPACE;
  }

  parse_rect_attr(&out->padding, padding);
  parse_rect_attr(&out->margin, margin);

  if (xmlStrcasecmp(node->name, BAD_CAST "TableView") == 0) {
    runtime_fill_table_params(ctx, node, &out->lparam);
  } else if (xmlStrcasecmp(node->name, BAD_CAST "ComboBox") == 0) {
    runtime_fill_combo_params(ctx, node, &out->lparam);
  }

  out->children = runtime_build_defs(ctx, node, &out->child_count);
  const char *mapped = runtime_map_class_name((const char *)node->name);
  out->class_name = runtime_strdup(ctx, mapped);
  if (out->child_count > 0)
    out->flags |= WINDOW_AUTO_LAYOUT;
}

static form_ctrl_def_t *runtime_build_defs(runtime_build_ctx_t *ctx, xmlNodePtr parent,
                                           int *out_count) {
  int count = runtime_count_children(parent);
  if (out_count)
    *out_count = count;
  if (count <= 0)
    return NULL;

  form_ctrl_def_t *defs = (form_ctrl_def_t *)runtime_alloc(ctx, (size_t)count * sizeof(form_ctrl_def_t));
  if (!defs)
    return NULL;

  int i = 0;
  for (xmlNodePtr c = parent ? parent->children : NULL; c; c = c->next) {
    if (!runtime_is_control_node(parent, c))
      continue;
    runtime_fill_def(ctx, c, &defs[i]);
    if (defs[i].id == 0)
      defs[i].id = (uint32_t)(ctx->next_generated_id++);
    i++;
  }

  return defs;
}

window_t *fe_create_runtime_form_window(window_t *doc,
                                        window_t *parent,
                                        winproc_t proc) {
  if (!doc || !parent || !proc)
    return NULL;

  runtime_build_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.next_generated_id = CTRL_ID_BASE;

  form_def_t def;
  memset(&def, 0, sizeof(def));
  def.name = doc->title;
  def.width = (doc->children && doc->children->frame.w > 0) ? doc->children->frame.w : FORM_DEFAULT_W;
  def.height = (doc->children && doc->children->frame.h > 0) ? doc->children->frame.h : FORM_DEFAULT_H;
  def.flags = WINDOW_NOTITLE | WINDOW_AUTO_LAYOUT;
  def.layout_spacing = 4;
  def.padding = (irect16_t){0, 0, 0, 0};
  def.margin = (irect16_t){0, 0, 0, 0};
  def.children = NULL;
  def.child_count = 0;

  xmlNodePtr form_node = (xmlNodePtr)doc->userdata2;
  if (form_node && form_node->type == XML_ELEMENT_NODE &&
      xmlStrcasecmp(form_node->name, BAD_CAST "form") == 0) {
    char flags_expr[128] = {0};
    runtime_xml_attr_copy(form_node, "flags", flags_expr, sizeof(flags_expr));
    def.flags = WINDOW_NOTITLE | WINDOW_AUTO_LAYOUT | runtime_parse_flags(flags_expr);
    def.layout_spacing = (uint8_t)runtime_xml_attr_int(form_node, "spacing", 4);

    char rect_expr[64] = {0};
    runtime_xml_attr_copy(form_node, "padding", rect_expr, sizeof(rect_expr));
    parse_rect_attr(&def.padding, rect_expr);
    rect_expr[0] = '\0';
    runtime_xml_attr_copy(form_node, "margin", rect_expr, sizeof(rect_expr));
    parse_rect_attr(&def.margin, rect_expr);

    // Runtime toolbar definitions from <toolbars> are not yet hydrated in the
    // editor preview path; avoid showing an empty toolbar strip.
    if (def.flags & WINDOW_TOOLBAR)
      def.flags &= ~WINDOW_TOOLBAR;

    def.child_count = 0;
    def.children = runtime_build_defs(&ctx, form_node, &def.child_count);
  }

  window_t *root = create_window_from_form(&def, 0, 0, parent, proc, 0, NULL);

  runtime_release(&ctx);
  return root;
}
