#include "scener.h"

static irect16_t document_workspace_rect(void) {
	int sw = ui_get_system_metrics(kSystemMetricScreenWidth);
	int sh = ui_get_system_metrics(kSystemMetricScreenHeight);
	int margin = 6;
	int left = margin;
	int top = MENUBAR_HEIGHT + TOOLBAR_BAND_HEIGHT + margin;
	int right = g_app && g_app->command_panel_win
		? g_app->command_panel_win->frame.x - margin : sw - margin;
	int bottom = sh - margin;
	if (right <= left) right = left + 1;
	if (bottom <= top) bottom = top + 1;
	return (irect16_t){ left, top, right - left, bottom - top };
}

static void doc_win_resize_children(scene_doc_t *doc) {
  if (!doc || !doc->win) return;
  irect16_t cr = get_client_rect(doc->win);
  if (doc->viewport_win)
    resize_window(doc->viewport_win, cr.w, cr.h);
}

result_t doc_win_proc(window_t *win, uint32_t msg,
                      uint32_t wparam, void *lparam) {
  scene_doc_t *doc = (scene_doc_t *)win->userdata;

  switch (msg) {
    case evCreate:
      return true;

    case evSetFocus:
      if (g_app && doc) g_app->active_doc = doc;
      return true;

    case evResize:
      doc_win_resize_children(doc);
      return false;

    case evDestroy:
      if (g_app && g_app->active_doc == doc)
        g_app->active_doc = NULL;
      return false;

    default:
      return false;
  }
}

scene_doc_t *create_document(const char *path) {
  scene_doc_t *doc = calloc(1, sizeof(scene_doc_t));
  if (!doc) return NULL;

  memset(&doc->scene, 0, sizeof(Scene));
  doc->scene.camFov = 60;
  doc->scene.camPos = v3(3, 2, 5);
  doc->scene.camLook = v3(0, 0, 0);
  doc->scene.ambient = v3(0.15f, 0.15f, 0.15f);
  doc->scene.bg = v3(0.25f, 0.3f, 0.35f);
  doc->scene.editMode = EDIT_Q_SELECT;
  doc->scene.selectedObj = -1;

  if (path && path[0]) {
    if (!load_scene(path, &doc->scene)) {
      free(doc);
      return NULL;
    }
    strncpy(doc->filename, path, sizeof(doc->filename)-1);
    doc->filename[sizeof(doc->filename)-1] = '\0';
  } else {
    Light sun = {0};
    sun.dir = v3(-0.4f, -0.8f, -0.3f);
    sun.color = v3(1, 0.95f, 0.9f);
    sun.intensity = 1.0f;
    sun.isDirectional = 1;
    sun.castsShadow = 1;
    DA_PUSH(doc->scene.lights, doc->scene.nlights, doc->scene.clights, sun);
    doc->scene.svols = calloc((size_t)doc->scene.nlights, sizeof(ShadowVolume));

    Mesh floor = gen_box(10, 0.1f, 10);
    scene_add_obj(&doc->scene, floor, mat4_identity(), mat4_identity(),
                  v3(0.5f, 0.5f, 0.5f), 16, 1, 1, 0);
  }

  scene_build_all_shadow_volumes(&doc->scene);
  scene_init_textures(&doc->scene);

  irect16_t workspace = document_workspace_rect();
  set_default_window_position(workspace.x, workspace.y);

  window_t *dwin = create_window(
      path ? path : "Untitled",
      WINDOW_STATUSBAR,
      MAKERECT(CW_USEDEFAULT, CW_USEDEFAULT, workspace.w, workspace.h),
      NULL, doc_win_proc, g_app->hinstance, NULL);
  if (!dwin) {
    scene_free_textures(&doc->scene);
    scene_free(&doc->scene);
    free(doc);
    return NULL;
  }
  dwin->userdata = doc;
  doc->win = dwin;

  doc->viewport_win = create_viewport_window(dwin, doc);

  show_window(dwin, true);
  doc->next = g_app->docs;
  g_app->docs = doc;
  g_app->active_doc = doc;

  doc_update_title(doc);
  return doc;
}

void close_document(scene_doc_t *doc) {
  if (!doc) return;

  if (g_app->active_doc == doc)
    g_app->active_doc = NULL;

  if (g_app->docs == doc) {
    g_app->docs = doc->next;
  } else {
    for (scene_doc_t *d = g_app->docs; d; d = d->next) {
      if (d->next == doc) { d->next = doc->next; break; }
    }
  }

  if (doc->win) destroy_window(doc->win);
  scene_free_textures(&doc->scene);
  scene_free(&doc->scene);
  free(doc);
}

bool scener_open_file_path(const char *path) {
  if (!g_app || !path || !path[0]) return false;
  scene_doc_t *doc = create_document(path);
  return doc != NULL;
}

void doc_update_title(scene_doc_t *doc) {
  if (!doc || !doc->win) return;
  if (doc->filename[0]) {
    const char *name = strrchr(doc->filename, '/');
    name = name ? name + 1 : doc->filename;
    snprintf(doc->win->title, sizeof(doc->win->title), "%s%s", name, doc->modified ? " *" : "");
  } else {
    snprintf(doc->win->title, sizeof(doc->win->title), "Untitled%s", doc->modified ? " *" : "");
  }
}
