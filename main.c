#include "scener.h"
#include <orion/gem.h>
#include <orion/ui.h>
#include <orion/user/gl_compat.h>
#include <orion/user/image.h>

typedef struct {
	bool screenshot_mode;
	bool debug_flags_set;
	char scene_path[512];
	char output_path[1024];
	char camera_name[32];
	int width, height;
	int debug_flags;
} scener_cli_t;

app_state_t *g_app = NULL;
static scener_cli_t g_cli;

static const accel_t kAccelEntries[] = {
  { FCONTROL|FVIRTKEY, AX_KEY_Z, ID_EDIT_UNDO },
  { FCONTROL|FVIRTKEY, AX_KEY_Y, ID_EDIT_REDO },
  { FCONTROL|FVIRTKEY, AX_KEY_N, ID_FILE_NEW  },
  { FCONTROL|FVIRTKEY, AX_KEY_O, ID_FILE_OPEN },
  { FCONTROL|FVIRTKEY, AX_KEY_S, ID_FILE_SAVE },
  { FCONTROL|FVIRTKEY, AX_KEY_W, ID_FILE_CLOSE},
  { FCONTROL|FVIRTKEY, AX_KEY_D, ID_EDIT_DUPLICATE },
  { FVIRTKEY,          AX_KEY_DEL, ID_EDIT_DELETE },
  { FVIRTKEY,          AX_KEY_Q, ID_TOOL_SELECT },
  { FVIRTKEY,          AX_KEY_W, ID_TOOL_MOVE },
  { FVIRTKEY,          AX_KEY_E, ID_TOOL_ROTATE },
  { FVIRTKEY,          AX_KEY_R, ID_TOOL_SCALE },
  { FVIRTKEY,          AX_KEY_F, ID_VIEW_ZOOM_FIT },
};
#define kAccelCount (int)(sizeof(kAccelEntries)/sizeof(kAccelEntries[0]))

static const accel_t kNavigationAccelEntries[] = {
  { FCONTROL|FVIRTKEY, AX_KEY_Z, ID_EDIT_UNDO },
  { FCONTROL|FVIRTKEY, AX_KEY_Y, ID_EDIT_REDO },
  { FCONTROL|FVIRTKEY, AX_KEY_N, ID_FILE_NEW  },
  { FCONTROL|FVIRTKEY, AX_KEY_O, ID_FILE_OPEN },
  { FCONTROL|FVIRTKEY, AX_KEY_S, ID_FILE_SAVE },
  { FCONTROL|FVIRTKEY, AX_KEY_W, ID_FILE_CLOSE},
  { FCONTROL|FVIRTKEY, AX_KEY_D, ID_EDIT_DUPLICATE },
};
#define kNavigationAccelCount (int)(sizeof(kNavigationAccelEntries)/sizeof(kNavigationAccelEntries[0]))

accel_table_t *scener_active_accelerators(void) {
  if (!g_app) return NULL;
  return g_app->viewport_navigating ? g_app->navigation_accel : g_app->accel;
}

static void cli_init(void) {
	memset(&g_cli, 0, sizeof(g_cli));
	g_cli.width = 1280;
	g_cli.height = 800;
}

static void cli_parse_size(const char *s) {
	int w = 0, h = 0;
	if (!s) return;
	if (sscanf(s, "%dx%d", &w, &h) != 2 && sscanf(s, "%d %d", &w, &h) != 2) return;
	if (w > 0) g_cli.width = w;
	if (h > 0) g_cli.height = h;
}

static void cli_parse(int argc, char *argv[]) {
	cli_init();
	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		if (!arg || !arg[0]) continue;
		if (!strcmp(arg, "--screenshot") || !strcmp(arg, "-o") || !strcmp(arg, "--output")) {
			if (i + 1 < argc) {
				g_cli.screenshot_mode = true;
				snprintf(g_cli.output_path, sizeof(g_cli.output_path), "%s", argv[++i]);
			}
			continue;
		}
		if (!strcmp(arg, "--cam") || !strcmp(arg, "-cam")) {
			if (i + 1 < argc) snprintf(g_cli.camera_name, sizeof(g_cli.camera_name), "%s", argv[++i]);
			continue;
		}
		if (!strcmp(arg, "--size")) {
			if (i + 1 < argc) cli_parse_size(argv[++i]);
			continue;
		}
		if (!strcmp(arg, "-d")) {
			if (i + 1 < argc) {
				g_cli.debug_flags = atoi(argv[++i]);
				g_cli.debug_flags_set = true;
			}
			continue;
		}
		if (!strcmp(arg, "-no-shadows")) {
			g_cli.debug_flags |= DBG_NO_SHADOWS;
			g_cli.debug_flags_set = true;
			g_cli.screenshot_mode = true;
			continue;
		}
		if (!strcmp(arg, "-wireframe")) {
			g_cli.debug_flags |= DBG_WIRE_SHADOWVOL;
			g_cli.debug_flags_set = true;
			g_cli.screenshot_mode = true;
			continue;
		}
		if (arg[0] == '-') continue;
		if (!g_cli.scene_path[0]) snprintf(g_cli.scene_path, sizeof(g_cli.scene_path), "%s", arg);
	}
	if (g_cli.screenshot_mode && !g_cli.debug_flags_set)
		g_cli.debug_flags = DBG_NO_SHADOWS | DBG_HIDE_CHARS | DBG_HIDE_LIGHTS;
	if (g_cli.screenshot_mode && !g_cli.output_path[0])
		snprintf(g_cli.output_path, sizeof(g_cli.output_path), "%s", "screenshot.png");
}

static void create_app_windows(hinstance_t hinstance) {
#ifdef BUILD_AS_GEM
  g_app->menubar_win = set_app_menu(scener_menubar_proc, kMenus, kNumMenus,
                                    handle_menu_command, hinstance);
  create_main_toolbar_window();
#else
  g_app->chrome_win = create_app_chrome("SimpleSketch3D Chrome",
                                        scener_menubar_proc,
                                        kMenus, kNumMenus,
                                        scener_toolbar_proc,
                                        hinstance);
  g_app->menubar_win      = app_chrome_menubar(g_app->chrome_win);
  g_app->main_toolbar_win = app_chrome_toolbar(g_app->chrome_win);
  scener_sync_main_toolbar();
#endif

  g_app->command_panel_win = create_command_panel_window();
}

static const char *scener_file_types[] = { ".blks", NULL };

#ifndef BUILD_AS_GEM
static bool scener_open_file_handler(const char *path) {
  return scener_open_file_path(path);
}
#endif

static bool scener_write_screenshot(scene_doc_t *doc, const char *path) {
	if (!doc || !path || !path[0]) return false;
	int width = g_cli.width, height = g_cli.height;

	Scene *scene = &doc->scene;
	scene->camFov = scene->camFov > 0 ? scene->camFov : 60;
	if (g_cli.camera_name[0]) scene_select_camera(scene, g_cli.camera_name);

	vec3 dir = vsub(scene->camLook, scene->camPos);
	if (vlen(dir) < 0.0001f) dir = v3(0, 0, -1);
	dir = vnorm(dir);
	mat4 proj = mat4_perspective(scene->camFov, (float)width / (float)height, 0.1f, 1000.0f);
	mat4 view = mat4_lookat(scene->camPos, scene->camLook, v3(0, 1, 0));

	GLuint fbo = 0, color = 0, depth = 0;
	glGenFramebuffers(1, &fbo);
	glGenTextures(1, &color);
	glGenRenderbuffers(1, &depth);
	glBindTexture(GL_TEXTURE_2D, color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	ui_begin_frame();
	glViewport(0, 0, width, height);
	glScissor(0, 0, width, height);
	glEnable(GL_SCISSOR_TEST);
	render_frame(scene, width, height, proj, view, scene->camPos, dir, g_cli.debug_flags);

	size_t bytes = (size_t)width * (size_t)height * 4;
	uint8_t *pixels = malloc(bytes);
	bool ok = pixels && capture_framebuffer_rgba(width, height, pixels) && save_image_png(path, pixels, width, height);
	free(pixels);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(1, &color);
	glDeleteRenderbuffers(1, &depth);
	return ok;
}

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
  cli_parse(argc, argv);

  g_app = calloc(1, sizeof(app_state_t));
  if (!g_app) return false;

  g_app->hinstance   = hinstance;
  g_app->debug_flags  = g_cli.debug_flags;

#ifndef BUILD_AS_GEM
  ui_register_open_file_handler(scener_open_file_handler);
#endif

  srand((unsigned int)time(NULL));
  register_commctl_classes();
  shader_init();

  if (!g_cli.screenshot_mode)
    create_app_windows(hinstance);

  g_app->accel = load_accelerators(kAccelEntries, kAccelCount);
  g_app->navigation_accel = load_accelerators(kNavigationAccelEntries, kNavigationAccelCount);
  if (g_app->menubar_win)
    send_message(g_app->menubar_win, kMenuBarMessageSetAccelerators, 0, g_app->accel);

  scene_doc_t *doc = create_document_ex(g_cli.scene_path[0] ? g_cli.scene_path : NULL,
                                        !g_cli.screenshot_mode);
  if (!doc) return false;

  if (g_cli.camera_name[0])
    scene_select_camera(&doc->scene, g_cli.camera_name);

  if (g_cli.screenshot_mode) {
    if (!scener_write_screenshot(doc, g_cli.output_path)) return false;
    ui_request_quit();
  }

  return true;
}

void gem_shutdown(void) {
  if (!g_app) return;

  free_accelerators(g_app->accel);
  free_accelerators(g_app->navigation_accel);
  g_app->accel = NULL;
  g_app->navigation_accel = NULL;

  if (g_app->command_panel_win && is_window(g_app->command_panel_win))
    destroy_window(g_app->command_panel_win);
  g_app->command_panel_win = NULL;

  if (g_app->chrome_win && is_window(g_app->chrome_win))
    destroy_window(g_app->chrome_win);
  g_app->chrome_win = g_app->menubar_win = g_app->main_toolbar_win = NULL;

  while (g_app->docs)
    close_document(g_app->docs);

  shader_deinit();

  free(g_app);
  g_app = NULL;
}

GEM_DEFINE("SimpleSketch3D", "1.0", gem_init, gem_shutdown, scener_file_types)

#ifndef BUILD_AS_GEM
int main(int argc, char *argv[]) {
  cli_parse(argc, argv);
  int flags = g_cli.screenshot_mode ? UI_INIT_HIDDEN : UI_INIT_DESKTOP;
  if (!ui_init_graphics(flags, "SimpleSketch3D", g_cli.width, g_cli.height)) return 1;
  if (!gem_init(argc, argv, 0)) {
    ui_shutdown_graphics();
    return 1;
  }
  if (!g_cli.screenshot_mode) {
    while (ui_is_running()) {
      ui_event_t e;
      while (get_message(&e)) {
        if (!translate_accelerator(g_app ? g_app->menubar_win : NULL, &e, scener_active_accelerators()))
          dispatch_message(&e);
      }
      repost_messages();
    }
  }
  gem_shutdown();
  ui_shutdown_graphics();
  return 0;
}
#endif
