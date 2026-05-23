// Minimal project XML I/O for window-first form runtime.

#include "formeditor.h"
#include <libxml/parser.h>
#include <libxml/tree.h>

extern app_state_t *g_app;
extern void forms_browser_refresh(void);
extern void plugins_browser_refresh(void);
extern void databases_browser_refresh(void);
extern void form_doc_show_only(window_t *doc);
extern window_t *create_form_doc(int w, int h);
extern void close_form_doc(window_t *doc);
extern void form_doc_update_title(window_t *doc);
extern void canvas_rebuild_live_controls(window_t *doc);

static int feio_xml_attr_int(xmlNodePtr node, const char *name, int fallback) {
  xmlChar *v = xmlGetProp(node, BAD_CAST name);
  if (!v)
    return fallback;
  char *end = NULL;
  long n = strtol((const char *)v, &end, 0);
  bool ok = (end && *end == '\0');
  xmlFree(v);
  if (!ok)
    return fallback;
  return (int)n;
}

static void feio_xml_attr_copy(xmlNodePtr node, const char *name, char *dst, size_t dst_sz) {
  if (!dst || dst_sz == 0)
    return;
  dst[0] = '\0';
  xmlChar *v = xmlGetProp(node, BAD_CAST name);
  if (!v)
    return;
  snprintf(dst, dst_sz, "%s", (const char *)v);
  xmlFree(v);
}

static void fe_close_all_docs(void) {
  if (!g_app)
    return;
  while (g_app->form_count > 0 && g_app->forms[0])
    close_form_doc(g_app->forms[0]);
  g_app->active_form = NULL;
}

static void fe_load_project_meta(xmlNodePtr root) {
  if (!g_app)
    return;
  feio_xml_attr_copy(root, "name", g_app->project.name, sizeof(g_app->project.name));
  feio_xml_attr_copy(root, "title", g_app->project.title, sizeof(g_app->project.title));
  feio_xml_attr_copy(root, "root", g_app->project.root, sizeof(g_app->project.root));
}

static void fe_load_form_node(xmlNodePtr form_node) {
  xmlChar *title_x = xmlGetProp(form_node, BAD_CAST "title");
  xmlChar *frame_x = xmlGetProp(form_node, BAD_CAST "frame");
  xmlChar *width_x = xmlGetProp(form_node, BAD_CAST "width");
  xmlChar *height_x = xmlGetProp(form_node, BAD_CAST "height");

  int w = feio_xml_attr_int(form_node, "width", FORM_DEFAULT_W);
  int h = feio_xml_attr_int(form_node, "height", FORM_DEFAULT_H);
  if (w < 1) w = FORM_DEFAULT_W;
  if (h < 1) h = FORM_DEFAULT_H;

  window_t *doc = create_form_doc(w, h);
  if (!doc) {
    if (title_x) xmlFree(title_x);
    if (frame_x) xmlFree(frame_x);
    if (width_x) xmlFree(width_x);
    if (height_x) xmlFree(height_x);
    return;
  }

  if (doc->userdata2) {
    xmlFreeNode((xmlNodePtr)doc->userdata2);
    doc->userdata2 = NULL;
  }
  doc->userdata2 = xmlCopyNode(form_node, 1);

  feio_xml_attr_copy(form_node, "title", doc->title, sizeof(doc->title));
  if (!doc->title[0])
    feio_xml_attr_copy(form_node, "name", doc->title, sizeof(doc->title));
  // Keep form XML metadata on doc->userdata2 for the preview runtime builder.
  // The document shell window itself must remain editor-owned and not inherit
  // runtime form flags/layout (toolbar/statusbar/padding), otherwise preview
  // and editor chrome get mixed.

  form_doc_update_title(doc);
  canvas_rebuild_live_controls(doc);

  if (title_x) xmlFree(title_x);
  if (frame_x) xmlFree(frame_x);
  if (width_x) xmlFree(width_x);
  if (height_x) xmlFree(height_x);
}

bool fe_project_load(const char *path) {
  if (!path || !g_app)
    return false;

  xmlDocPtr xdoc = xmlReadFile(path, NULL, XML_PARSE_NOBLANKS);
  if (!xdoc)
    return false;

  xmlNodePtr root = xmlDocGetRootElement(xdoc);
  if (!root || xmlStrcasecmp(root->name, BAD_CAST "orion") != 0) {
    xmlFreeDoc(xdoc);
    return false;
  }

  snprintf(g_app->project.filename, sizeof(g_app->project.filename), "%s", path);
  fe_close_all_docs();
  fe_load_project_meta(root);

  for (xmlNodePtr n = root->children; n; n = n->next) {
    if (n->type != XML_ELEMENT_NODE)
      continue;
    if (xmlStrcasecmp(n->name, BAD_CAST "forms") != 0)
      continue;

    for (xmlNodePtr f = n->children; f; f = f->next) {
      if (f->type != XML_ELEMENT_NODE)
        continue;
      if (xmlStrcasecmp(f->name, BAD_CAST "form") == 0)
        fe_load_form_node(f);
    }
  }

  if (g_app->form_count > 0 && g_app->forms[0]) {
    form_doc_show_only(g_app->forms[0]);
  }

  g_app->project.loaded = true;
  g_app->project.modified = false;
  forms_browser_refresh();
  plugins_browser_refresh();
  databases_browser_refresh();

  xmlFreeDoc(xdoc);
  return true;
}

static void fe_write_form(FILE *f, const window_t *doc) {
  if (!f || !doc)
    return;

  char name_buf[512];
  snprintf(name_buf, sizeof(name_buf), "%s", doc->title[0] ? doc->title : "Form");
  size_t len = strlen(name_buf);
  if (len >= 2 && strcmp(name_buf + len - 2, " *") == 0)
    name_buf[len - 2] = '\0';

  fprintf(f,
          "    <form name=\"%s\" title=\"%s\" width=\"%d\" height=\"%d\"/>\n",
      name_buf,
      doc->title[0] ? doc->title : "Form",
      doc->children ? doc->children->frame.w : FORM_DEFAULT_W,
      doc->children ? doc->children->frame.h : FORM_DEFAULT_H);
}

bool fe_project_save(const char *path) {
  if (!g_app)
    return false;

  const char *out = path && *path ? path : g_app->project.filename;
  if (!out || !*out)
    return false;

  FILE *f = fopen(out, "wb");
  if (!f)
    return false;

  fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(f, "<orion version=\"1\" name=\"%s\" title=\"%s\" root=\"%s\">\n",
          g_app->project.name[0] ? g_app->project.name : "project",
          g_app->project.title[0] ? g_app->project.title : "Orion Project",
          g_app->project.root[0] ? g_app->project.root : "./");
  fprintf(f, "  <forms>\n");

  for (int i = 0; i < g_app->form_count; i++) {
    window_t *w = g_app->forms[i];
    if (w)
      fe_write_form(f, w);
  }

  fprintf(f, "  </forms>\n");
  fprintf(f, "</orion>\n");
  fclose(f);

  snprintf(g_app->project.filename, sizeof(g_app->project.filename), "%s", out);
  g_app->project.modified = false;
  return true;
}
