// Menu bar, document management, file I/O, and dialog entry points
// for the Orion Form Editor.

#include "formeditor.h"
#include "../../commctl/commctl.h"
#include "../../user/enum_parse.h"
#include <ctype.h>
#include <inttypes.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

static irect16_t doc_control_form_rect(form_doc_t *doc, window_t *el);

// ============================================================
// Menu definitions
// ============================================================

static const menu_item_t kFileItems[] = {
  {"New",        ID_FILE_NEW},
  {"Open...",    ID_FILE_OPEN},
  {NULL,         0},
  {"Save",       ID_FILE_SAVE},
  {"Save As...", ID_FILE_SAVEAS},
  {NULL,         0},
  {"Quit",       ID_FILE_QUIT},
};

static const menu_item_t kEditItems[] = {
  {"Delete",            ID_EDIT_DELETE},
  {NULL,                0},
  {"Properties...",     ID_EDIT_PROPS},
};

static const menu_item_t kViewItems[] = {
  {"Grid Settings...", ID_VIEW_GRID},
};

static const menu_item_t kHelpItems[] = {
  {"About...", ID_HELP_ABOUT},
};

menu_def_t kMenus[] = {
  {"File", kFileItems, (int)(sizeof(kFileItems)/sizeof(kFileItems[0]))},
  {"Edit", kEditItems, (int)(sizeof(kEditItems)/sizeof(kEditItems[0]))},
  {"View", kViewItems, (int)(sizeof(kViewItems)/sizeof(kViewItems[0]))},
  {"Help", kHelpItems, (int)(sizeof(kHelpItems)/sizeof(kHelpItems[0]))},
};
const int kNumMenus = (int)(sizeof(kMenus)/sizeof(kMenus[0]));

window_t *app_get_window(fe_window_role_t role) {
  if (!g_app || (unsigned)role >= FE_WIN_COUNT)
    return NULL;
  return g_app->windows[role];
}

void app_set_window(fe_window_role_t role, window_t *win) {
  if (!g_app || (unsigned)role >= FE_WIN_COUNT)
    return;
  g_app->windows[role] = win;
}

int app_doc_count(void) {
  return g_app ? g_app->doc_count : 0;
}

form_doc_t *app_doc_at(int idx) {
  if (!g_app || idx < 0 || idx >= g_app->doc_count)
    return NULL;
  return g_app->docs[idx];
}

int app_doc_index(form_doc_t *doc) {
  if (!g_app || !doc)
    return -1;
  for (int i = 0; i < g_app->doc_count; i++) {
    if (g_app->docs[i] == doc)
      return i;
  }
  return -1;
}

form_doc_t *app_active_doc(void) {
  if (!g_app)
    return NULL;
  return app_doc_at(g_app->active_doc_index);
}

bool app_set_active_doc_index(int idx) {
  if (!g_app || idx < 0 || idx >= g_app->doc_count)
    return false;
  g_app->active_doc_index = idx;
  return true;
}

bool app_add_doc(form_doc_t *doc) {
  if (!g_app || !doc || g_app->doc_count >= FE_MAX_DOCS)
    return false;
  g_app->docs[g_app->doc_count++] = doc;
  g_app->active_doc_index = g_app->doc_count - 1;
  return true;
}

void app_remove_doc_at(int idx) {
  if (!g_app || idx < 0 || idx >= g_app->doc_count)
    return;
  for (int i = idx; i < g_app->doc_count - 1; i++)
    g_app->docs[i] = g_app->docs[i + 1];
  g_app->doc_count--;
  g_app->docs[g_app->doc_count] = NULL;
  if (g_app->doc_count <= 0) {
    g_app->active_doc_index = -1;
    return;
  }
  if (g_app->active_doc_index > idx)
    g_app->active_doc_index--;
  else if (g_app->active_doc_index >= g_app->doc_count)
    g_app->active_doc_index = g_app->doc_count - 1;
}

// ============================================================
// Document title
// ============================================================

void form_doc_update_title(form_doc_t *doc) {
  if (!doc || !doc->doc_win) return;
  const char *name = doc->form_title[0] ? doc->form_title :
                     (doc->form_id[0] ? doc->form_id : "Untitled");
  const char *slash = strrchr(name, '/');
  if (slash) name = slash + 1;
  snprintf(doc->doc_win->title, sizeof(doc->doc_win->title), "%s%s",
           name, doc->modified ? " *" : "");
  invalidate_window(doc->doc_win);
}

void form_doc_activate(form_doc_t *doc) {
  if (!g_app || !doc) return;
  int next_idx = app_doc_index(doc);
  if (next_idx < 0) return;
  form_doc_t *prev = app_active_doc();
  if (prev == doc) return;
  g_app->active_doc_index = next_idx;
  if (prev && prev->doc_win)
    invalidate_window(prev->doc_win);
  if (doc->doc_win)
    invalidate_window(doc->doc_win);
  property_browser_refresh(doc);
  forms_browser_refresh();
}

void form_doc_show_only(form_doc_t *doc) {
  if (!g_app || !doc) return;
  for (int i = 0; i < app_doc_count(); i++) {
    form_doc_t *it = app_doc_at(i);
    if (it != doc && it->doc_win && is_window(it->doc_win))
      show_window(it->doc_win, false);
  }
  form_doc_activate(doc);
  if (doc->doc_win && is_window(doc->doc_win))
    show_window(doc->doc_win, true);
  forms_browser_refresh();
}

// ============================================================
// Document window procedure
// ============================================================

static result_t doc_win_proc(window_t *win, uint32_t msg,
                              uint32_t wparam, void *lparam) {
  form_doc_t *doc = (form_doc_t *)win->userdata;
  switch (msg) {
    case evCreate:
      return true;
    case evSetFocus:
      if (doc && window_has_state(win, WINDOW_STATE_VISIBLE)) form_doc_activate(doc);
      return false;
    case evPaint:
      fill_rect(get_sys_color(brWorkspaceBg), R(0, 0, win->frame.w, win->frame.h));
      return false;
    case evHScroll:
      // Forward the built-in hscroll notification to the canvas child.
      if (doc && doc->canvas_win)
        send_message(doc->canvas_win, evHScroll, wparam, lparam);
      return true;
    case evResize: {
      if (doc && doc->canvas_win) {
        irect16_t cr = get_client_rect(win);
        int new_w = MAX(1, cr.w);
        int new_h = MAX(1, cr.h);
        bool changed = (doc->form_size.w != new_w || doc->form_size.h != new_h);
        doc->form_size.w = new_w;
        doc->form_size.h = new_h;
        resize_window(doc->canvas_win, cr.w, cr.h);
        if (changed) {
          doc->modified = true;
          if (g_app)
            g_app->project.modified = true;
          form_doc_update_title(doc);
        }
      }
      return false;
    }
    case evClose: {
      if (!doc) return false;
      show_window(win, false);
      forms_browser_refresh();
      return true;
    }
    default:
      return false;
  }
}

// ============================================================
// create_form_doc / close_form_doc
// ============================================================

static irect16_t form_doc_frame_for_size(int form_w, int form_h, uint32_t form_flags) {
  int max_w = SCREEN_W - 4;
  int max_h = SCREEN_H - MENUBAR_HEIGHT - 4;
  bool has_status = (form_flags & WINDOW_STATUSBAR) != 0;
  int status_h = has_status ? STATUSBAR_HEIGHT : 0;
  bool needs_hscroll = form_w > max_w;
  int hstrip = (needs_hscroll && !has_status) ? SCROLLBAR_WIDTH : 0;
  int max_canvas_h = max_h - TITLEBAR_HEIGHT - status_h - hstrip;
  bool needs_vscroll;
  int frame_w;
  int frame_h;

  if (max_w < 1) max_w = 1;
  if (max_canvas_h < 1) max_canvas_h = 1;

  needs_vscroll = form_h > max_canvas_h;
  frame_w = form_w + (needs_vscroll ? SCROLLBAR_WIDTH : 0);
  if (frame_w > max_w) frame_w = max_w;

  frame_h = TITLEBAR_HEIGHT + status_h + hstrip + form_h;
  if (frame_h > max_h) frame_h = max_h;

  return (irect16_t){CW_USEDEFAULT, CW_USEDEFAULT, frame_w, frame_h};
}

static void form_doc_apply_window_flags_and_size(form_doc_t *doc) {
  if (!doc || !doc->doc_win) return;
  doc->doc_win->flags &= ~WINDOW_STATUSBAR;
  doc->doc_win->flags |= (doc->flags & WINDOW_STATUSBAR);
  irect16_t frame = form_doc_frame_for_size(doc->form_size.w, doc->form_size.h, doc->flags);
  resize_window(doc->doc_win, frame.w, frame.h);
  if (doc->canvas_win) {
    irect16_t cr = get_client_rect(doc->doc_win);
    resize_window(doc->canvas_win, cr.w, cr.h);
  }
}

form_doc_t *create_form_doc(int w, int h) {
  if (!g_app) return NULL;
  if (w <= 0 || h <= 0 || w > INT16_MAX || h > INT16_MAX) return NULL;
  form_doc_t *prev_doc = app_active_doc();

  form_doc_t *doc = (form_doc_t *)calloc(1, sizeof(form_doc_t));
  if (!doc) return NULL;

  doc->form_size.w    = w;
  doc->form_size.h    = h;
  doc->flags     = 0;
  doc->modified  = false;
  if (fe_default_auto_layout_enabled())
    doc->flags |= WINDOW_AUTO_LAYOUT;
  doc->layout_type = (doc->flags & WINDOW_AUTO_LAYOUT) ? 1 : 0;
  doc->flags &= ~WINDOW_STACK_HORIZONTAL;
  doc->grid_columns = 0;
  doc->spacing = 4;
  doc->padding = (irect16_t){0, 0, 0, 0};
  doc->margin = (irect16_t){0, 0, 0, 0};
  doc->next_id   = CTRL_ID_BASE;
  doc->grid_size    = 8;
  doc->show_grid    = true;
  doc->snap_to_grid = true;

  // Document window
  irect16_t doc_frame = form_doc_frame_for_size(w, h, doc->flags);
  set_default_window_position(DOC_START_X, DOC_START_Y);
  window_t *dwin = create_window(
      "Untitled",
      WINDOW_HSCROLL | (doc->flags & WINDOW_STATUSBAR),
      &doc_frame,
      NULL, doc_win_proc, g_app->hinstance, NULL);
  dwin->userdata = doc;
  doc->doc_win   = dwin;

  // Canvas child window (owns the VSCROLL) — sized to the document window's client area
  irect16_t cr = get_client_rect(dwin);
  window_t *cwin = create_window(
      "", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
      MAKERECT(0, 0, cr.w, cr.h),
      dwin, win_canvas_proc, 0, doc);
  cwin->flags &= ~WINDOW_NOTABSTOP;
  doc->canvas_win = cwin;
  cr = get_client_rect(dwin);
  resize_window(cwin, cr.w, cr.h);

  if (!app_add_doc(doc)) {
    destroy_window(dwin);
    free(doc);
    return NULL;
  }

  show_window(dwin, true);
  if (prev_doc && prev_doc->doc_win)
    invalidate_window(prev_doc->doc_win);
  form_doc_update_title(doc);
  send_message(dwin, evStatusBar, 0, (void *)"New form");
  property_browser_refresh(doc);
  forms_browser_refresh();
  return doc;
}

void close_form_doc(form_doc_t *doc) {
  if (!doc) return;
  int idx = app_doc_index(doc);
  if (idx >= 0)
    app_remove_doc_at(idx);
  if (doc->doc_win && is_window(doc->doc_win))
    destroy_window(doc->doc_win);
  property_browser_refresh(app_active_doc());
  forms_browser_refresh();
  free(doc);
}

// ============================================================
// Project I/O — XML .orion files
// ============================================================

// Map control type to a short keyword used in the file.
static const char *ctrl_type_token(int type) {
  const fe_component_desc_t *c = fe_component_by_id(type);
  return c ? c->token : "control";
}

static int ctrl_type_from_token(const char *tok) {
  const fe_component_desc_t *c = fe_component_by_token(tok);
  if (!c) return -1;
  for (int i = 0; i < fe_component_count(); i++) {
    const fe_component_desc_t *it = fe_component_at(i);
    if (it == c)
      return i;
  }
  return -1;
}

// ============================================================
// XML project I/O (.orion)
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

static const char *control_name_for_window(const window_t *el) {
  return (el && el->statusbar_text[0]) ? el->statusbar_text : "control";
}

static ui_font_t label_font_from_window(const window_t *el) {
  if (!el)
    return FONT_SMALL;
  uintptr_t packed = (uintptr_t)el->userdata;
  return (ui_font_t)((packed >> 8) & 0xffu);
}

static uint8_t label_color_from_window(const window_t *el) {
  if (!el)
    return brTextNormal;
  uintptr_t packed = (uintptr_t)el->userdata;
  return (uint8_t)(packed & 0xffu);
}

static bool label_color_set_from_window(const window_t *el) {
  if (!el)
    return false;
  uintptr_t packed = (uintptr_t)el->userdata;
  return (packed & (1u << 16)) != 0;
}

static bool parse_numeric_expr(const char *s, int *out) {
  if (!s || !*s || !out) return false;
  char *end = NULL;
  long n = strtol(s, &end, 0);
  if (!end || *end != '\0') return false;
  *out = (int)n;
  return true;
}

static uint32_t flag_value(const char *tok) {
  if (!tok || !*tok || strcmp(tok, "0") == 0) return 0;
  if (strcmp(tok, "BUTTON_DEFAULT") == 0) return BUTTON_DEFAULT;
  if (strcmp(tok, "WINDOW_NOTITLE") == 0) return WINDOW_NOTITLE;
  if (strcmp(tok, "WINDOW_NOFILL") == 0) return WINDOW_NOFILL;
  if (strcmp(tok, "WINDOW_NOTABSTOP") == 0) return WINDOW_NOTABSTOP;
  if (strcmp(tok, "WINDOW_STATUSBAR") == 0) return WINDOW_STATUSBAR;
  if (strcmp(tok, "WINDOW_DIALOG") == 0) return WINDOW_DIALOG;
  if (strcmp(tok, "WINDOW_NOTRAYBUTTON") == 0) return WINDOW_NOTRAYBUTTON;
  if (strcmp(tok, "WINDOW_NORESIZE") == 0) return WINDOW_NORESIZE;
  if (strcmp(tok, "WINDOW_HSCROLL") == 0) return WINDOW_HSCROLL;
  if (strcmp(tok, "WINDOW_VSCROLL") == 0) return WINDOW_VSCROLL;
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

static int project_resolve_control_id(form_doc_t *doc, const char *expr) {
  int id = 0;
  if (parse_numeric_expr(expr, &id))
    return id;
  return doc ? doc->next_id++ : CTRL_ID_BASE;
}

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

static void project_reset(void) {
  if (!g_app) return;
  while (app_doc_count() > 0)
    close_form_doc(app_doc_at(0));
  memset(&g_app->project, 0, sizeof(g_app->project));
}

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

static void project_load_controls(form_doc_t *doc, xmlNodePtr node) {
  for (xmlNodePtr n = node ? node->children : NULL; n; n = n->next) {
    if (n->type != XML_ELEMENT_NODE)
      continue;
    if (xmlStrcmp(n->name, BAD_CAST "requires") == 0)
      continue;
    int type = ctrl_type_from_token((const char *)n->name);

    if (type >= 0 && type < FE_MAX_COMPONENTS && doc->element_count < MAX_ELEMENTS) {
      irect16_t frame = {0};
      frame.x = int_attr(n, "x", 0);
      frame.y = int_attr(n, "y", 0);
      frame.w = int_attr(n, "width", int_attr(n, "w", 10));
      frame.h = int_attr(n, "height", int_attr(n, "h", 8));
      frame.w = MAX(1, frame.w);
      frame.h = MAX(1, frame.h);
      uint32_t parent_id = (uint32_t)int_attr(n, "parent", 0);
      int idx = canvas_add_element(doc, type, frame, -1, parent_id);
      if (idx < 0 || idx >= doc->element_count)
        continue;
      window_t *el = doc->elements[idx];
      if (!el)
        continue;
      char id_expr[32] = {0};
      copy_attr(n, "id", id_expr, sizeof(id_expr));
      int loaded_id = project_resolve_control_id(doc, id_expr);
      if (loaded_id > 0) {
        bool id_in_use = false;
        for (int j = 0; j < doc->element_count; j++) {
          if (j != idx && doc->elements[j] && doc->elements[j]->id == (uint32_t)loaded_id) {
            id_in_use = true;
            break;
          }
        }
        if (!id_in_use) {
          el->id = (uint32_t)loaded_id;
        }
      }
      char flags_expr[128] = {0};
      copy_attr(n, "flags", flags_expr, sizeof(flags_expr));
      el->flags = (el->flags & 0xff000000u) | parse_flags_expr(flags_expr);
      copy_attr(n, "text", el->title, sizeof(el->title));
      copy_attr(n, "name", el->statusbar_text, sizeof(el->statusbar_text));
      char *font = xml_attr_dup(n, "font");
      char *color = xml_attr_dup(n, "color");
      if (!el->statusbar_text[0])
        snprintf(el->statusbar_text, sizeof(el->statusbar_text), "control%d", idx + 1);
      if (strcmp(ctrl_type_token(type), "label") == 0) {
        uint8_t label_font = font_attr(font, FONT_SMALL);
        uint8_t label_color = color_attr(color, brTextNormal);
        bool label_color_set = (color != NULL);
        el->userdata = (void *)(uintptr_t)label_pack_userdata(label_color, (ui_font_t)label_font,
                                                              label_color_set);
      }
      char *h_align = xml_attr_dup(n, "h-align");
      char *v_align = xml_attr_dup(n, "v-align");
      if (!h_align) h_align = xml_attr_dup(n, "h_align");
      if (!v_align) v_align = xml_attr_dup(n, "v_align");
      el->layout.h_align = align_h_attr(h_align, el->layout.h_align);
      el->layout.v_align = align_v_attr(v_align, el->layout.v_align);
      el->layout.layout_padding = rect_attr(n, "padding",
                              rect_attr(n, "layout_padding", (irect16_t){0, 0, 0, 0}));
      el->layout.layout_margin = rect_attr(n, "margin",
                              rect_attr(n, "layout_margin", (irect16_t){0, 0, 0, 0}));
      free(font);
      free(color);
      free(h_align);
      free(v_align);
      if ((int)el->id >= doc->next_id)
        doc->next_id = (int)el->id + 1;
    }

    if (n->children)
      project_load_controls(doc, n);
  }
}

static void project_auto_layout_doc(form_doc_t *doc) {
  if (!doc || !(doc->flags & WINDOW_AUTO_LAYOUT)) return;
  
  // Apply component default sizes for auto-layout forms
  for (int i = 0; i < doc->element_count; i++) {
    window_t *el = doc->elements[i];
    const fe_component_desc_t *desc = fe_component_by_id((int)el->value);
    if (desc) {
      el->frame.w = MAX(1, desc->default_size.w);
      el->frame.h = MAX(1, desc->default_size.h);
    }
  }
  window_t *roots[MAX_ELEMENTS];
  int root_count = 0;
  for (int i = 0; i < doc->element_count; i++) {
    if (doc->elements[i] && doc->elements[i]->parent == doc->canvas_win)
      roots[root_count++] = doc->elements[i];
  }
  
  const int gap = doc->spacing > 0 ? doc->spacing : 4;
  int count = root_count;
  int pad_l = doc->padding.x;
  int pad_t = doc->padding.y;
  int pad_r = doc->padding.w;
  int pad_b = doc->padding.h;
  int max_w = MAX(1, doc->form_size.w - pad_l - pad_r);
  int max_h = MAX(1, doc->form_size.h - pad_t - pad_b);
  int content_x = pad_l;
  int content_y = pad_t;

  if (doc->layout_type == 2) {
    int cols = doc->grid_columns > 0 ? doc->grid_columns : 2;
    if (cols < 1) cols = 1;
    int rows = (count + cols - 1) / cols;
    if (rows < 1) rows = 1;
    int base_w = max_w / cols;
    int rem_w = max_w % cols;
    int base_h = max_h / rows;
    int rem_h = max_h % rows;
    for (int i = 0; i < count; i++) {
      window_t *el = roots[i];
      int row = i / cols;
      int col = i % cols;
      irect16_t margin = el->layout.layout_margin;
      int cell_w = base_w + (col < rem_w ? 1 : 0);
      int cell_h = base_h + (row < rem_h ? 1 : 0);
      int ow = el->frame.w > 0 ? el->frame.w + margin.x + margin.w : margin.x + margin.w + 1;
      int oh = el->frame.h > 0 ? el->frame.h + margin.y + margin.h : margin.y + margin.h + 1;
      int x = content_x + col * (base_w + gap) + (col < rem_w ? col : rem_w);
      int y = content_y + row * (base_h + gap) + (row < rem_h ? row : rem_h);
      int outer_x = x;
      int outer_y = y;
      int outer_w = (el->layout.h_align == LAYOUT_ALIGN_STRETCH) ? cell_w : MIN(ow, cell_w);
      int outer_h = (el->layout.v_align == LAYOUT_ALIGN_STRETCH) ? cell_h : MIN(oh, cell_h);
      if (el->layout.h_align == LAYOUT_ALIGN_CENTER)
        outer_x += (cell_w - outer_w) / 2;
      else if (el->layout.h_align == LAYOUT_ALIGN_END)
        outer_x += cell_w - outer_w;
      if (el->layout.v_align == LAYOUT_ALIGN_CENTER)
        outer_y += (cell_h - outer_h) / 2;
      else if (el->layout.v_align == LAYOUT_ALIGN_END)
        outer_y += cell_h - outer_h;
      el->frame = (irect16_t){
        outer_x + margin.x,
        outer_y + margin.y,
        MAX(1, outer_w - margin.x - margin.w),
        MAX(1, outer_h - margin.y - margin.h)
      };
    }
    return;
  }

  if (doc->flags & WINDOW_STACK_HORIZONTAL) {
    int x = content_x;
    for (int i = 0; i < count; i++) {
      window_t *el = roots[i];
      irect16_t margin = el->layout.layout_margin;
      if (i > 0) x += gap;
      int inner_w = el->frame.w > 0 ? el->frame.w : 1;
      int inner_h = el->frame.h > 0 ? el->frame.h : 1;
      int ow = inner_w + margin.x + margin.w;
      int oh = inner_h + margin.y + margin.h;
      int outer_y = content_y;
      if (el->layout.v_align == LAYOUT_ALIGN_STRETCH) {
        oh = max_h;
      } else {
        if (oh > max_h) oh = max_h;
        if (el->layout.v_align == LAYOUT_ALIGN_CENTER) outer_y = content_y + (max_h - oh) / 2;
        else if (el->layout.v_align == LAYOUT_ALIGN_END) outer_y = content_y + max_h - oh;
      }
      el->frame = (irect16_t){
        x + margin.x,
        outer_y + margin.y,
        MAX(1, ow - margin.x - margin.w),
        MAX(1, oh - margin.y - margin.h)
      };
      x += ow;
    }
  } else {
    int y = content_y;
    for (int i = 0; i < count; i++) {
      window_t *el = roots[i];
      irect16_t margin = el->layout.layout_margin;
      if (i > 0) y += gap;
      int inner_w = el->frame.w > 0 ? el->frame.w : 1;
      int inner_h = el->frame.h > 0 ? el->frame.h : 1;
      int ow = inner_w + margin.x + margin.w;
      int oh = inner_h + margin.y + margin.h;
      int outer_x = content_x;
      if (el->layout.h_align == LAYOUT_ALIGN_STRETCH) {
        ow = max_w;
      } else {
        if (ow > max_w) ow = max_w;
        if (el->layout.h_align == LAYOUT_ALIGN_CENTER) outer_x = content_x + (max_w - ow) / 2;
        else if (el->layout.h_align == LAYOUT_ALIGN_END) outer_x = content_x + max_w - ow;
      }
      el->frame = (irect16_t){
        outer_x + margin.x,
        y + margin.y,
        MAX(1, ow - margin.x - margin.w),
        MAX(1, oh - margin.y - margin.h)
      };
      y += oh;
    }
  }
}

void form_doc_auto_layout_reflow(form_doc_t *doc) {
  project_auto_layout_doc(doc);
  if (doc) canvas_sync_live_controls(doc);
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
    doc->layout_type = layout_mode_attr(layout_mode,
                                        (doc->flags & WINDOW_AUTO_LAYOUT) ? 1 : 0);
    if (layout_orientation_attr(layout_orientation, WINDOW_STACK_VERTICAL) & WINDOW_STACK_HORIZONTAL)
      doc->flags |= WINDOW_STACK_HORIZONTAL;
    else
      doc->flags &= ~WINDOW_STACK_HORIZONTAL;
    free(layout_mode);
    free(layout_orientation);
  }
  doc->grid_columns = (uint8_t)int_attr(form_node, "layout_columns", 0);
  doc->spacing = (uint8_t)int_attr(form_node, "spacing",
                                          int_attr(form_node, "layout_spacing", 0));
  doc->padding = rect_attr(form_node, "padding",
                           rect_attr(form_node, "layout_padding", (irect16_t){0, 0, 0, 0}));
  doc->margin = rect_attr(form_node, "margin",
                          rect_attr(form_node, "layout_margin", (irect16_t){0, 0, 0, 0}));
  project_load_requires(doc, form_node);
  project_load_controls(doc, form_node);
  
  // Detect fixed-layout forms: if any element has non-zero x/y, disable auto-layout
  for (int i = 0; i < doc->element_count; i++) {
    irect16_t fr = doc_control_form_rect(doc, doc->elements[i]);
    if (fr.x != 0 || fr.y != 0) {
      doc->flags &= ~WINDOW_AUTO_LAYOUT;
      break;
    }
  }
  
  // Only run auto-layout if enabled (otherwise preserve loaded x/y coordinates)
  if (doc->flags & WINDOW_AUTO_LAYOUT) {
    project_auto_layout_doc(doc);
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

bool form_project_load(const char *path) {
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
  project_load_menus(xdoc, root);
  formeditor_rebuild_tool_palette();
  project_load_forms(root);

  g_app->project.loaded = true;
  g_app->project.modified = false;
  if (app_doc_count() > 0) form_doc_show_only(app_doc_at(0));
  forms_browser_refresh();
  plugins_browser_refresh();
  xmlFreeDoc(xdoc);
  return true;
}

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

static irect16_t doc_control_form_rect(form_doc_t *doc, window_t *el) {
  if (!doc || !doc->canvas_win || !el)
    return (irect16_t){0, 0, 0, 0};
  int x = el->frame.x;
  int y = el->frame.y;
  for (window_t *p = el->parent; p && p != doc->canvas_win; p = p->parent) {
    x += p->frame.x;
    y += p->frame.y;
  }
  canvas_state_t *cs = (canvas_state_t *)doc->canvas_win->userdata;
  if (cs) {
    x += cs->pan.x;
    y += cs->pan.y;
  }
  return (irect16_t){(int16_t)x, (int16_t)y, el->frame.w, el->frame.h};
}

static void project_save_doc(FILE *f, form_doc_t *doc) {
  const char *label = doc->form_title[0] ? doc->form_title :
                      (doc->form_id[0] ? doc->form_id : "Untitled");
  fprintf(f, "      <form");
  xml_attr(f, "name", doc->form_id[0] ? doc->form_id : "form");
  xml_attr(f, "title", label);
  fprintf(f, "\n            width=\"%d\" height=\"%d\"\n            flags=\"%" PRIu32 "\"",
          doc->form_size.w, doc->form_size.h, doc->flags);
  if (doc->layout_type == 2) {
    fprintf(f, "\n            layout_mode=\"%s\"",
            layout_mode_token(doc->layout_type));
  }
  if (doc->flags & WINDOW_STACK_HORIZONTAL)
    fprintf(f, "\n            layout_orientation=\"%s\"",
        layout_orientation_token(doc->flags & WINDOW_STACK_HORIZONTAL));
  if (doc->spacing != 0)
    fprintf(f, "\n            spacing=\"%u\"", (unsigned)doc->spacing);
  if (doc->padding.x || doc->padding.y || doc->padding.w || doc->padding.h)
    fprintf(f, "\n            padding=\"%d %d %d %d\"",
            doc->padding.x, doc->padding.y, doc->padding.w, doc->padding.h);
  if (doc->margin.x || doc->margin.y || doc->margin.w || doc->margin.h)
    fprintf(f, "\n            margin=\"%d %d %d %d\"",
            doc->margin.x, doc->margin.y, doc->margin.w, doc->margin.h);
  fprintf(f, ">\n");

  if (doc->required_plugin[0]) {
    fprintf(f, "        <requires");
    xml_attr(f, "library", doc->required_plugin);
    fprintf(f, " />\n");
  }
  for (int i = 0; i < doc->element_count; i++) {
    window_t *el = doc->elements[i];
    if (!el)
      continue;
    int type = (int)el->value;
    fprintf(f, "        <%s", ctrl_type_token(type));
    xml_attr(f, "name", control_name_for_window(el));
    xml_attr(f, "text", el->title);
    if (el->parent && el->parent != doc->canvas_win)
      fprintf(f, " parent=\"%u\"", (unsigned)el->parent->id);
    if (type == CTRL_LABEL || strcmp(ctrl_type_token(type), "label") == 0) {
      ui_font_t label_font = label_font_from_window(el);
      uint8_t label_color = label_color_from_window(el);
      bool label_color_set = label_color_set_from_window(el);
      if (label_font != FONT_SMALL)
        xml_attr(f, "font", font_token(label_font));
      if (label_color_set || label_color != brTextNormal)
        xml_attr(f, "color", color_token(label_color));
    }
    // Only emit x/y for fixed-layout forms; auto-layout forms recalculate positions
    if (!(doc->flags & WINDOW_AUTO_LAYOUT)) {
      irect16_t fr = doc_control_form_rect(doc, el);
      fprintf(f, " x=\"%d\" y=\"%d\"", fr.x, fr.y);
    }
    irect16_t fr = doc_control_form_rect(doc, el);
    fprintf(f, " width=\"%d\" height=\"%d\"", fr.w, fr.h);
    fprintf(f, " h-align=\"%s\"", align_h_token(el->layout.h_align));
    fprintf(f, " v-align=\"%s\"", align_v_token(el->layout.v_align));
    if (el->layout.layout_padding.x || el->layout.layout_padding.y ||
        el->layout.layout_padding.w || el->layout.layout_padding.h)
      fprintf(f, " padding=\"%d %d %d %d\"",
              el->layout.layout_padding.x, el->layout.layout_padding.y,
              el->layout.layout_padding.w, el->layout.layout_padding.h);
    if (el->layout.layout_margin.x || el->layout.layout_margin.y ||
        el->layout.layout_margin.w || el->layout.layout_margin.h)
      fprintf(f, " margin=\"%d %d %d %d\"",
              el->layout.layout_margin.x, el->layout.layout_margin.y,
              el->layout.layout_margin.w, el->layout.layout_margin.h);
    char flags_buf[32];
    snprintf(flags_buf, sizeof(flags_buf), "%" PRIu32, (el->flags & 0x00FFFFFFu));
    xml_attr(f, "flags", flags_buf);
    fprintf(f, " />\n");
  }
  fprintf(f, "      </form>\n");
}

bool form_project_save(const char *path) {
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

  if (p->menus_xml[0]) {
    fprintf(f, "%s\n\n", p->menus_xml);
  }

  fprintf(f, "    <forms>\n");
  for (int i = 0; i < app_doc_count(); i++)
    project_save_doc(f, app_doc_at(i));
  fprintf(f, "    </forms>\n");
  fprintf(f, "</orion>\n");

  fclose(f);
  snprintf(p->filename, sizeof(p->filename), "%s", path);
  p->loaded = true;
  p->modified = false;
  for (int i = 0; i < app_doc_count(); i++) {
    form_doc_t *doc = app_doc_at(i);
    doc->modified = false;
    form_doc_update_title(doc);
  }
  return true;
}

// ============================================================
// About dialog
// ============================================================

#define ABOUT_W 220
#define ABOUT_H  80

static result_t about_proc(window_t *win, uint32_t msg,
                            uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      win->userdata = lparam;
      return true;
    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        end_dialog(win, 1);
        return true;
      }
      return false;
    default:
      return false;
  }
}

enum {
  ABOUT_ID_OK = 1,
};

static const form_ctrl_def_t kAboutChildren[] = {
  { .class_name = "label", .text = "Orion Form Editor", .name = "title",
    .h_align = LAYOUT_ALIGN_STRETCH, .font = FONT_SYSTEM, .font_set = true },
  { .class_name = "label", .text = "Version 1.0", .name = "version",
    .h_align = LAYOUT_ALIGN_STRETCH, .color = brTextDisabled, .color_set = true },
  { .class_name = "label", .text = "VB3-inspired form designer", .name = "desc",
    .h_align = LAYOUT_ALIGN_STRETCH, .color = brTextDisabled, .color_set = true },
  {
    .class_name = "stack",
    .name = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
      { .class_name = "button", .id = ABOUT_ID_OK, .size = {50, BUTTON_HEIGHT},
        .flags = BUTTON_DEFAULT, .text = "OK", .name = "ok", .h_align = LAYOUT_ALIGN_START },
    },
    .child_count = 2,
  },
};

static const form_def_t kAboutForm = {
  .name = "About Orion Form Editor",
  .flags = WINDOW_AUTO_LAYOUT,
  .width = ABOUT_W,
  .height = ABOUT_H,
  .layout_spacing = 6,
  .padding = {8, 8, 8, 8},
  .children = kAboutChildren,
  .child_count = ARRAY_LEN(kAboutChildren),
  .ok_id = ABOUT_ID_OK,
};

void show_about_dialog(window_t *parent) {
  show_dialog_from_form(&kAboutForm, "About Orion Form Editor",
                        parent, about_proc, NULL);
}

// ============================================================
// Grid Settings dialog
// ============================================================

#define GRID_W   180
#define GRID_H   108

#define GRID_ROW1_Y   6
#define GRID_ROW2_Y   (GRID_ROW1_Y + BUTTON_HEIGHT + 4)
#define GRID_ROW3_Y   (GRID_ROW2_Y + BUTTON_HEIGHT + 4)
#define GRID_BTN_Y    (GRID_H - BUTTON_HEIGHT - 6)

#define GRID_ID_SHOW   1
#define GRID_ID_SNAP   2
#define GRID_ID_SIZE   3
#define GRID_ID_OK     4
#define GRID_ID_CANCEL 5

#define GRID_SIZE_MIN  1
#define GRID_SIZE_MAX  64

// grid_size is bound via DDX_TEXT; checkboxes are handled manually.
typedef struct {
  int  grid_size;
} grid_size_data_t;

static const form_ctrl_def_t kGridChildren[] = {
  { .class_name = "checkbox", .id = GRID_ID_SHOW, .text = "Show grid", .name = "chk_show",
    .h_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "checkbox", .id = GRID_ID_SNAP, .text = "Snap to grid", .name = "chk_snap",
    .h_align = LAYOUT_ALIGN_STRETCH },
  {
    .class_name = "stack",
    .name = "size_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "label", .text = "Grid size:", .name = "lbl_size", .size = {60, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "textedit", .id = GRID_ID_SIZE, .text = "", .name = "edit_size", .size = {40, BUTTON_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
    },
    .child_count = 2,
  },
  {
    .class_name = "stack",
    .name = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 4,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
      { .class_name = "button", .id = GRID_ID_OK, .size = {50, BUTTON_HEIGHT},
        .flags = BUTTON_DEFAULT, .text = "OK", .name = "btn_ok", .h_align = LAYOUT_ALIGN_START },
      { .class_name = "button", .id = GRID_ID_CANCEL, .size = {50, BUTTON_HEIGHT},
        .text = "Cancel", .name = "btn_cancel", .h_align = LAYOUT_ALIGN_START },
    },
    .child_count = 3,
  },
};

static const ctrl_binding_t k_grid_bindings[] = {
  DDX_TEXT(GRID_ID_SIZE, grid_size_data_t, grid_size),
};

static const form_def_t kGridForm = {
  .name          = "Grid Settings",
  .width         = GRID_W,
  .height        = GRID_H,
  .flags = (0) | WINDOW_AUTO_LAYOUT,
  .layout_spacing = 6,
  .padding       = {8, 8, 8, 8},
  .children      = kGridChildren,
  .child_count   = ARRAY_LEN(kGridChildren),
  .bindings      = k_grid_bindings,
  .binding_count = ARRAY_LEN(k_grid_bindings),
  .ok_id         = GRID_ID_OK,
  .cancel_id     = GRID_ID_CANCEL,
};

typedef struct {
  form_doc_t *doc;
  bool        accepted;
} grid_dlg_state_t;

static result_t grid_dlg_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam) {
  grid_dlg_state_t *gs = (grid_dlg_state_t *)win->userdata;
  switch (msg) {
    case evCreate: {
      gs = (grid_dlg_state_t *)lparam;
      win->userdata = gs;
      form_doc_t *doc = gs->doc;
      // Set checkbox states manually (no checkbox DDX helper yet).
      window_t *chk_show = get_window_item(win, GRID_ID_SHOW);
      window_t *chk_snap = get_window_item(win, GRID_ID_SNAP);
      if (chk_show)
        send_message(chk_show, btnSetCheck,
                     doc->show_grid ? btnStateChecked : btnStateUnchecked, NULL);
      if (chk_snap)
        send_message(chk_snap, btnSetCheck,
                     doc->snap_to_grid ? btnStateChecked : btnStateUnchecked, NULL);
      // Push grid_size via DDX.
      grid_size_data_t gsd = { doc->grid_size };
      dialog_push(win, &gsd, k_grid_bindings, ARRAY_LEN(k_grid_bindings));
      return true;
    }
    case evCommand: {
      if (HIWORD(wparam) != btnClicked) return false;
      window_t *src = (window_t *)lparam;
      if (!src) return false;
      if (src->id == GRID_ID_OK) {
        form_doc_t *doc = gs->doc;
        // Pull grid_size via DDX.
        grid_size_data_t gsd = { doc->grid_size };
        dialog_pull(win, &gsd, k_grid_bindings, ARRAY_LEN(k_grid_bindings));
        if (gsd.grid_size < GRID_SIZE_MIN) gsd.grid_size = GRID_SIZE_MIN;
        if (gsd.grid_size > GRID_SIZE_MAX) gsd.grid_size = GRID_SIZE_MAX;
        doc->grid_size = gsd.grid_size;
        // Pull checkboxes manually.
        window_t *chk_show = get_window_item(win, GRID_ID_SHOW);
        window_t *chk_snap = get_window_item(win, GRID_ID_SNAP);
        if (chk_show)
          doc->show_grid = (send_message(chk_show, btnGetCheck, 0, NULL) == btnStateChecked);
        if (chk_snap)
          doc->snap_to_grid = (send_message(chk_snap, btnGetCheck, 0, NULL) == btnStateChecked);
        gs->accepted = true;
        end_dialog(win, 1);
        return true;
      }
      if (src->id == GRID_ID_CANCEL) {
        end_dialog(win, 0);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

static void show_grid_settings_dialog(window_t *parent, form_doc_t *doc) {
  grid_dlg_state_t gs = { doc, false };
  show_dialog_from_form(&kGridForm, "Grid Settings", parent, grid_dlg_proc, &gs);
  if (gs.accepted && doc->canvas_win)
    invalidate_window(doc->canvas_win);
}

// ============================================================
// Properties dialog
// ============================================================

#define PROPS_W  260
#define PROPS_H  110

// Child IDs
#define PROPS_ID_CAPTION   1
#define PROPS_ID_NAME      2
#define PROPS_ID_OK        3
#define PROPS_ID_CANCEL    4

// Computed row positions (mirrors the form below)
#define PROPS_ROW1_Y       4
#define PROPS_ROW2_Y       (PROPS_ROW1_Y + BUTTON_HEIGHT + 6)   // 23
#define PROPS_INFO_Y       (PROPS_ROW2_Y + BUTTON_HEIGHT + 6)   // 42
#define PROPS_BTN_Y        (PROPS_H - BUTTON_HEIGHT - 6)        // 86

static const form_ctrl_def_t kPropsChildren[] = {
  {
    .class_name = "grid",
    .name = "fields",
    .flags = WINDOW_FLEXSPACE,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      {
        .class_name = "column",
        .name = "labels",
        .size = {60, 0},
        .children = (const form_ctrl_def_t[]){
          { .class_name = "label", .text = "Caption:", .name = "lbl_caption", .size = {60, CONTROL_HEIGHT},
            .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
          { .class_name = "label", .text = "Name:", .name = "lbl_name", .size = {60, CONTROL_HEIGHT},
            .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
        },
        .child_count = 2,
      },
      {
        .class_name = "column",
        .name = "inputs",
        .flags = WINDOW_FLEXSPACE,
        .children = (const form_ctrl_def_t[]){
          { .class_name = "textedit", .id = PROPS_ID_CAPTION, .text = "", .name = "edit_caption",
            .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
          { .class_name = "textedit", .id = PROPS_ID_NAME, .text = "", .name = "edit_name",
            .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
        },
        .child_count = 2,
      },
    },
    .child_count = 2,
  },
  {
    .class_name = "stack",
    .name = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 4,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
      { .class_name = "button", .id = PROPS_ID_OK, .size = {50, BUTTON_HEIGHT},
        .flags = BUTTON_DEFAULT, .text = "OK", .name = "btn_ok", .h_align = LAYOUT_ALIGN_START },
      { .class_name = "button", .id = PROPS_ID_CANCEL, .size = {50, BUTTON_HEIGHT},
        .text = "Cancel", .name = "btn_cancel", .h_align = LAYOUT_ALIGN_START },
    },
    .child_count = 3,
  },
};

typedef struct {
  char caption[64];
  char name[32];
} props_fields_t;

// DDX bindings: caption and name edits.
static const ctrl_binding_t k_props_bindings[] = {
  DDX_TEXT(PROPS_ID_CAPTION, props_fields_t, caption),
  DDX_TEXT(PROPS_ID_NAME, props_fields_t, name),
};

static const form_def_t kPropsForm = {
  .name          = "Element Properties",
  .width         = PROPS_W,
  .height        = PROPS_H,
  .flags = (0) | WINDOW_AUTO_LAYOUT,
  .layout_spacing = 6,
  .padding       = {8, 8, 8, 8},
  .children      = kPropsChildren,
  .child_count   = ARRAY_LEN(kPropsChildren),
  .bindings      = k_props_bindings,
  .binding_count = ARRAY_LEN(k_props_bindings),
  .ok_id         = PROPS_ID_OK,
  .cancel_id     = PROPS_ID_CANCEL,
};

// ============================================================
// Form Properties dialog
// ============================================================

#define FORM_PROPS_W  220
#define FORM_PROPS_H   158

#define FORM_PROPS_ID_AUTO   1
#define FORM_PROPS_ID_KIND    2
#define FORM_PROPS_ID_ORIENT  3
#define FORM_PROPS_ID_COLUMNS 4
#define FORM_PROPS_ID_OK      5
#define FORM_PROPS_ID_CANCEL  6

#define FORM_PROPS_ROW1_Y      10
#define FORM_PROPS_ROW2_Y      34
#define FORM_PROPS_ROW3_Y      58
#define FORM_PROPS_ROW4_Y      82
#define FORM_PROPS_BTN_Y       (FORM_PROPS_H - BUTTON_HEIGHT - 6)

typedef struct {
  bool auto_layout_enabled;
  int  layout_type;
  int  layout_orientation;
  char grid_columns[8];
  bool accepted;
} form_props_state_t;

static void form_props_fill_layout_combos(window_t *win) {
  static const char *const kKindItems[] = { "None", "Stack", "Grid" };
  static const char *const kOrientItems[] = { "Vertical", "Horizontal" };
  window_t *kind = get_window_item(win, FORM_PROPS_ID_KIND);
  window_t *orient = get_window_item(win, FORM_PROPS_ID_ORIENT);
  if (kind) {
    send_message(kind, cbClear, 0, NULL);
    for (size_t i = 0; i < ARRAY_LEN(kKindItems); i++)
      send_message(kind, cbAddString, 0, (void *)kKindItems[i]);
  }
  if (orient) {
    send_message(orient, cbClear, 0, NULL);
    for (size_t i = 0; i < ARRAY_LEN(kOrientItems); i++)
      send_message(orient, cbAddString, 0, (void *)kOrientItems[i]);
  }
}

static const ctrl_binding_t k_form_props_bindings[] = {
  DDX_CHECK(FORM_PROPS_ID_AUTO, form_props_state_t, auto_layout_enabled),
  DDX_COMBO(FORM_PROPS_ID_KIND, form_props_state_t, layout_type, 0),
  DDX_COMBO(FORM_PROPS_ID_ORIENT, form_props_state_t, layout_orientation, WINDOW_STACK_VERTICAL),
  DDX_TEXT(FORM_PROPS_ID_COLUMNS, form_props_state_t, grid_columns),
};

static const form_ctrl_def_t kFormPropsChildren[] = {
  { .class_name = "checkbox", .id = FORM_PROPS_ID_AUTO, .text = "Use auto layout", .name = "chk_auto",
    .h_align = LAYOUT_ALIGN_STRETCH },
  {
    .class_name = "stack",
    .name = "kind_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "label", .text = "Layout:", .name = "lbl_kind", .size = {72, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "combobox", .id = FORM_PROPS_ID_KIND, .text = "", .name = "combo_kind", .size = {124, BUTTON_HEIGHT + 2},
        .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER, .flags = WINDOW_FLEXSPACE },
    },
    .child_count = 2,
  },
  {
    .class_name = "stack",
    .name = "orient_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "label", .text = "Orientation:", .name = "lbl_orient", .size = {72, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "combobox", .id = FORM_PROPS_ID_ORIENT, .text = "", .name = "combo_orient", .size = {124, BUTTON_HEIGHT + 2},
        .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER, .flags = WINDOW_FLEXSPACE },
    },
    .child_count = 2,
  },
  {
    .class_name = "stack",
    .name = "columns_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "label", .text = "Columns:", .name = "lbl_columns", .size = {72, CONTROL_HEIGHT},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
      { .class_name = "textedit", .id = FORM_PROPS_ID_COLUMNS, .text = "", .name = "edit_columns", .size = {64, BUTTON_HEIGHT + 2},
        .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
    },
    .child_count = 2,
  },
  {
    .class_name = "stack",
    .name = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 4,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      { .class_name = "space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
      { .class_name = "button", .id = FORM_PROPS_ID_OK, .size = {50, BUTTON_HEIGHT},
        .flags = BUTTON_DEFAULT, .text = "OK", .name = "btn_ok", .h_align = LAYOUT_ALIGN_START },
      { .class_name = "button", .id = FORM_PROPS_ID_CANCEL, .size = {50, BUTTON_HEIGHT},
        .text = "Cancel", .name = "btn_cancel", .h_align = LAYOUT_ALIGN_START },
    },
    .child_count = 3,
  },
};

static const form_def_t kFormPropsForm = {
  .name          = "Form Properties",
  .width         = FORM_PROPS_W,
  .height        = FORM_PROPS_H,
  .flags = (0) | WINDOW_AUTO_LAYOUT,
  .layout_spacing = 6,
  .padding       = {8, 8, 8, 8},
  .children      = kFormPropsChildren,
  .child_count   = ARRAY_LEN(kFormPropsChildren),
  .bindings      = k_form_props_bindings,
  .binding_count = ARRAY_LEN(k_form_props_bindings),
  .ok_id         = FORM_PROPS_ID_OK,
  .cancel_id     = FORM_PROPS_ID_CANCEL,
};

static result_t form_props_proc(window_t *win, uint32_t msg,
                                uint32_t wparam, void *lparam) {
  form_props_state_t *ps = (form_props_state_t *)win->userdata;
  switch (msg) {
    case evCreate:
      ps = (form_props_state_t *)lparam;
      win->userdata = ps;
      if (ps && app_active_doc()) {
        form_props_fill_layout_combos(win);
        dialog_push(win, ps, k_form_props_bindings, ARRAY_LEN(k_form_props_bindings));
      }
      return true;
    case evCommand: {
      if (HIWORD(wparam) != btnClicked || !ps) return false;
      window_t *src = (window_t *)lparam;
      if (!src) return false;
      if (src->id == FORM_PROPS_ID_OK) {
        form_doc_t *doc = app_active_doc();
        if (doc) {
          bool old_auto_layout = (doc->flags & WINDOW_AUTO_LAYOUT) != 0;
          uint8_t old_kind = doc->layout_type;
          flags_t old_orient = doc->flags & WINDOW_STACK_HORIZONTAL;
          uint8_t old_columns = doc->grid_columns;
          dialog_pull(win, ps, k_form_props_bindings, ARRAY_LEN(k_form_props_bindings));
          if (ps->auto_layout_enabled)
            doc->flags |= WINDOW_AUTO_LAYOUT;
          else
            doc->flags &= ~WINDOW_AUTO_LAYOUT;
          doc->layout_type = (uint8_t)ps->layout_type;
          if (ps->layout_orientation & WINDOW_STACK_HORIZONTAL)
            doc->flags |= WINDOW_STACK_HORIZONTAL;
          else
            doc->flags &= ~WINDOW_STACK_HORIZONTAL;
          {
            int cols = atoi(ps->grid_columns);
            if (cols < 0) cols = 0;
            if (cols > 255) cols = 255;
            doc->grid_columns = (uint8_t)cols;
          }
          if (((doc->flags & WINDOW_AUTO_LAYOUT) != 0) != old_auto_layout ||
              doc->layout_type != old_kind ||
              (doc->flags & WINDOW_STACK_HORIZONTAL) != old_orient ||
              doc->grid_columns != old_columns) {
            doc->modified = true;
            form_doc_update_title(doc);
          }
          if (doc->flags & WINDOW_AUTO_LAYOUT) {
            form_doc_auto_layout_reflow(doc);
            canvas_rebuild_live_controls(doc);
          }
        }
        ps->accepted = true;
        end_dialog(win, 1);
        return true;
      }
      if (src->id == FORM_PROPS_ID_CANCEL) {
        end_dialog(win, 0);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

static bool show_form_props_dialog(window_t *parent, form_doc_t *doc) {
  if (!doc) return false;
  form_props_state_t st = {
    .auto_layout_enabled = (doc->flags & WINDOW_AUTO_LAYOUT) != 0,
    .layout_type = doc->layout_type,
    .layout_orientation = (doc->flags & WINDOW_STACK_HORIZONTAL) ? WINDOW_STACK_HORIZONTAL : WINDOW_STACK_VERTICAL,
  };
  snprintf(st.grid_columns, sizeof(st.grid_columns), "%u",
           (unsigned)doc->grid_columns);
  show_dialog_from_form(&kFormPropsForm, "Form Properties", parent, form_props_proc, &st);
  return st.accepted;
}

typedef struct {
  form_doc_t     *doc;
  window_t       *el;
  props_fields_t  fields;
  bool            accepted;
} props_state_t;

static result_t props_proc(window_t *win, uint32_t msg,
                            uint32_t wparam, void *lparam) {
  props_state_t *ps = (props_state_t *)win->userdata;
  switch (msg) {
    case evCreate: {
      ps = (props_state_t *)lparam;
      win->userdata = ps;

      // Dynamic type-info label (content is computed at runtime).
      char info[64];
      snprintf(info, sizeof(info), "Type: %s  ID: %d  (%d, %d)  %d x %d",
               ctrl_type_token((int)ps->el->value), ps->el->id,
               ps->el->frame.x, ps->el->frame.y, ps->el->frame.w, ps->el->frame.h);
      create_window(info, WINDOW_NOTITLE | WINDOW_NOFILL,
          MAKERECT(4, PROPS_INFO_Y, PROPS_W - 8, CONTROL_HEIGHT),
          win, "label", 0, (void *)(uintptr_t)brTextDisabled);

      snprintf(ps->fields.caption, sizeof(ps->fields.caption), "%.*s",
               (int)sizeof(ps->fields.caption) - 1, ps->el->title);
      snprintf(ps->fields.name, sizeof(ps->fields.name), "%.*s",
               (int)sizeof(ps->fields.name) - 1, control_name_for_window(ps->el));
      dialog_push(win, &ps->fields, k_props_bindings, ARRAY_LEN(k_props_bindings));
      return true;
    }

    case evCommand: {
      if (HIWORD(wparam) != btnClicked) return false;
      window_t *src = (window_t *)lparam;
      if (!src) return false;

      if (src->id == PROPS_ID_OK) {
        dialog_pull(win, &ps->fields, k_props_bindings, ARRAY_LEN(k_props_bindings));
        snprintf(ps->el->title, sizeof(ps->el->title), "%s", ps->fields.caption);
        snprintf(ps->el->statusbar_text, sizeof(ps->el->statusbar_text), "%.*s",
                 (int)sizeof(ps->el->statusbar_text) - 1, ps->fields.name);
        invalidate_window(ps->el);
        ps->accepted = true;
        end_dialog(win, 1);
        return true;
      }
      if (src->id == PROPS_ID_CANCEL) {
        end_dialog(win, 0);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

bool show_props_dialog(window_t *parent, form_doc_t *doc, window_t *el) {
  props_state_t ps = {0};
  ps.doc      = doc;
  ps.el       = el;
  ps.accepted = false;
  show_dialog_from_form(&kPropsForm, "Element Properties", parent, props_proc, &ps);
  return ps.accepted;
}

// ============================================================
// File-picker wrapper (analogous to imageeditor/filepicker.c)
// ============================================================

static bool show_form_file_picker(window_t *parent, bool save_mode,
                                   char *out_path, size_t out_sz) {
  openfilename_t ofn = {0};
  ofn.lStructSize  = sizeof(ofn);
  ofn.hwndOwner    = parent;
  ofn.lpstrFile    = out_path;
  ofn.nMaxFile     = (uint32_t)out_sz;
  ofn.lpstrFilter  = "Orion Projects\0*.orion\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags        = save_mode ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST;
  return save_mode ? get_save_filename(&ofn) : get_open_filename(&ofn);
}

// ============================================================
// Menu command handler
// ============================================================

void handle_menu_command(uint16_t id) {
  if (!g_app) return;
  form_doc_t *doc = app_active_doc();

  switch (id) {
    case ID_FILE_NEW:
      create_form_doc(FORM_DEFAULT_W, FORM_DEFAULT_H);
      break;

    case ID_FILE_OPEN: {
      char path[512] = {0};
      window_t *owner = doc ? doc->doc_win : app_get_window(FE_WIN_MENUBAR);
      if (show_form_file_picker(owner, false, path, sizeof(path))) {
        if (!form_project_load(path) && owner)
          message_box(owner, "Failed to load Orion project.", "Open", MB_OK);
      }
      break;
    }

    case ID_FILE_SAVE:
      if (g_app->project.loaded && g_app->project.filename[0]) {
        if (form_project_save(g_app->project.filename)) {
          if (doc && doc->doc_win)
            send_message(doc->doc_win, evStatusBar, 0, (void *)"Project saved");
        } else if (doc && doc->doc_win) {
          send_message(doc->doc_win, evStatusBar, 0, (void *)"Project save failed");
        }
      } else {
        goto do_save_as;
      }
      break;

    do_save_as:
    case ID_FILE_SAVEAS: {
      if (!doc && app_doc_count() <= 0) break;
      char path[512] = {0};
      window_t *owner = doc ? doc->doc_win : app_get_window(FE_WIN_MENUBAR);
      if (show_form_file_picker(owner, true, path, sizeof(path))) {
        if (form_project_save(path)) {
          if (doc && doc->doc_win)
            send_message(doc->doc_win, evStatusBar, 0, path);
        } else {
          if (doc && doc->doc_win)
            send_message(doc->doc_win, evStatusBar, 0, (void *)"Project save failed");
        }
      }
      break;
    }

    case ID_FILE_QUIT:
#ifdef BUILD_AS_GEM
      if (g_app) {
        while (app_doc_count() > 0)
          close_form_doc(app_doc_at(0));
        if (app_get_window(FE_WIN_TOOLBOX))
          destroy_window(app_get_window(FE_WIN_TOOLBOX));
        if (app_get_window(FE_WIN_MENUBAR))
          destroy_window(app_get_window(FE_WIN_MENUBAR));
      }
#else
      ui_request_quit();
#endif
      break;

    case ID_EDIT_DELETE: {
      if (!doc) break;
      window_t *cwin = doc->canvas_win;
      if (!cwin) break;
      canvas_state_t *cs = (canvas_state_t *)cwin->userdata;
      if (!cs || cs->selected_idx < 0) break;
      int idx = cs->selected_idx;
      window_t *el = doc->elements[idx];
      if (el && is_window(el))
        destroy_window(el);
      for (int i = idx; i < doc->element_count - 1; i++)
        doc->elements[i] = doc->elements[i + 1];
      doc->element_count--;
      doc->elements[doc->element_count] = NULL;
      cs->selected_idx = -1;
      doc->modified = true;
      form_doc_update_title(doc);
      canvas_rebuild_live_controls(doc);
      break;
    }

    case ID_EDIT_PROPS: {
      if (!doc) break;
      window_t *cwin = doc->canvas_win;
      if (!cwin) break;
      canvas_state_t *cs = (canvas_state_t *)cwin->userdata;
      window_t *owner = app_get_window(FE_WIN_MENUBAR) ? app_get_window(FE_WIN_MENUBAR) : doc->doc_win;
      if (!cs || cs->selected_idx < 0) {
        show_form_props_dialog(owner, doc);
      } else {
        window_t *el = doc->elements[cs->selected_idx];
        if (show_props_dialog(owner, doc, el)) {
          doc->modified = true;
          form_doc_update_title(doc);
          canvas_sync_live_controls(doc);
          property_browser_refresh(doc);
        }
      }
      break;
    }

    case ID_VIEW_GRID: {
      if (!doc) break;
      window_t *owner = app_get_window(FE_WIN_MENUBAR) ? app_get_window(FE_WIN_MENUBAR) : doc->doc_win;
      show_grid_settings_dialog(owner, doc);
      break;
    }

    case ID_HELP_ABOUT: {
      window_t *owner = app_get_window(FE_WIN_MENUBAR) ? app_get_window(FE_WIN_MENUBAR) : (doc ? doc->doc_win : NULL);
      show_about_dialog(owner);
      break;
    }

    default:
      if (id != ID_TOOL_SELECT && fe_component_by_tool_ident(id))
        break;
      break;
  }
}

// ============================================================
// Menu bar window procedure
// ============================================================

result_t editor_menubar_proc(window_t *win, uint32_t msg,
                              uint32_t wparam, void *lparam) {
  if (msg == evCommand) {
    uint16_t notif = HIWORD(wparam);
    if (notif == kMenuBarNotificationItemClick ||
        notif == kAcceleratorNotification      ||
        notif == btnClicked) {
      handle_menu_command(LOWORD(wparam));
      return true;
    }
  }
  return win_menubar(win, msg, wparam, lparam);
}
