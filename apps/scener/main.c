#include "scener.h"
#include <orion/gem.h>
#include <orion/ui.h>
#include <orion/user/gl_compat.h>
#include <orion/user/bmp_icon_loader.h>
#include <orion/user/image.h>
#include <platform/platform.h>
#include <ctype.h>
#include <float.h>
#include <math.h>

#define DEFAULT_FOV   60.0f
#define PERSP_NEAR    0.1f
#define PERSP_FAR     1000.0f
#define SCENER_VERSION "1.0"

typedef struct {
	bool screenshot_mode;
	bool list_cameras, show_help, show_version;
	bool layout_mode;
	float layout_scale;
	char scene_path[512];
	char output_dir[1024];
	char camera_name[32];
	char format[8];
	int width, height;
	int debug_flags;
	bool invalid_format;
} scener_cli_t;

app_state_t *g_app = NULL;
static scener_cli_t g_cli;

static bool scener_save_screenshot(const char *path, const uint8_t *pixels,
                                   int width, int height) {
	const char *ext = strrchr(path, '.');
	if (ext && (!strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg")))
		return save_image_jpg(path, pixels, width, height, 90);
	if (ext && !strcasecmp(ext, ".png"))
		return save_image_png(path, pixels, width, height);
	fprintf(stderr, "[scener] unsupported screenshot format: %s\n", path);
	fflush(stderr);
	return false;
}

// Navigation uses a reduced context-specific table while the viewport is
// being dragged. The default command accelerators are generated from the
// menu declarations in scener.orion below.
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
	g_cli.width = 1024;
	g_cli.height = 768;
	snprintf(g_cli.output_dir, sizeof(g_cli.output_dir), "%s", "render");
	snprintf(g_cli.format, sizeof(g_cli.format), "%s", "png");
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
		if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
			g_cli.show_help = true;
			continue;
		}
		if (!strcmp(arg, "--version") || !strcmp(arg, "-V")) {
			g_cli.show_version = true;
			continue;
		}
		if (!strcmp(arg, "--render")) {
			g_cli.screenshot_mode = true;
			continue;
		}
		if (!strcmp(arg, "--layout")) {
			g_cli.layout_mode = true;
			g_cli.screenshot_mode = true;
			continue;
		}
		if (!strcmp(arg, "--scale")) {
			if (i + 1 < argc) g_cli.layout_scale = (float)atof(argv[++i]);
			continue;
		}
		if (!strcmp(arg, "--list-cameras")) {
			g_cli.list_cameras = true;
			continue;
		}
		if (!strcmp(arg, "--output-dir") || !strcmp(arg, "-o")) {
			if (i + 1 < argc)
				snprintf(g_cli.output_dir, sizeof(g_cli.output_dir), "%s", argv[++i]);
			continue;
		}
		if (!strcmp(arg, "--camera") || !strcmp(arg, "--cam") || !strcmp(arg, "-cam")) {
			if (i + 1 < argc) snprintf(g_cli.camera_name, sizeof(g_cli.camera_name), "%s", argv[++i]);
			continue;
		}
		if (!strcmp(arg, "--size")) {
			if (i + 1 < argc) cli_parse_size(argv[++i]);
			continue;
		}
		if (!strcmp(arg, "--format")) {
			if (i + 1 < argc) {
				const char *format = argv[++i];
				if (!strcasecmp(format, "jpg") || !strcasecmp(format, "jpeg"))
					snprintf(g_cli.format, sizeof(g_cli.format), "%s", "jpg");
				else if (!strcasecmp(format, "png"))
					snprintf(g_cli.format, sizeof(g_cli.format), "%s", "png");
				else
					g_cli.invalid_format = true;
			}
			continue;
		}
		if (!strcmp(arg, "-d")) {
			if (i + 1 < argc) {
				g_cli.debug_flags = atoi(argv[++i]);
			}
			continue;
		}
		if (!strcmp(arg, "-no-shadows")) {
			g_cli.debug_flags |= DBG_NO_SHADOWS;
			g_cli.screenshot_mode = true;
			continue;
		}
		if (!strcmp(arg, "-wireframe")) {
			g_cli.debug_flags |= DBG_WIRE_SHADOWVOL;
			g_cli.screenshot_mode = true;
			continue;
		}
		if (arg[0] == '-') continue;
		if (!g_cli.scene_path[0]) snprintf(g_cli.scene_path, sizeof(g_cli.scene_path), "%s", arg);
	}
}

#ifndef BUILD_AS_GEM
static void cli_print_help(void) {
	printf("Usage:\n");
	printf("  scener SCENE.blks\n");
	printf("  scener --render SCENE.blks [OPTIONS]\n\n");
	printf("Render options:\n");
	printf("  --size WIDTHxHEIGHT    Output resolution (default: 1024x768)\n");
	printf("  --camera NAME          Render one camera (default: all cameras)\n");
	printf("  --output-dir DIR       Output directory (default: render/)\n");
	printf("  --format png|jpg       Output format (default: png)\n");
	printf("  --list-cameras         List scene cameras and exit\n");
	printf("  --layout               Render orthographic top-down plan (auto-size from scene bounds)\n");
	printf("  --scale N              Pixels per cm for --layout (default: 2)\n");
	printf("  -no-shadows            Keep filled lighting but disable stencil shadows\n");
	printf("  -wireframe             Overlay red shadow-volume wireframes\n\n");
	printf("Default rendering uses filled materials, all scene lights, and each\n");
	printf("light's castShadows setting. The -wireframe mode is a shadow-volume\n");
	printf("diagnostic; it does not replace scene geometry with wireframe. Red\n");
	printf("lines only appear in that diagnostic. Black triangular streaks in a\n");
	printf("default render indicate an invalid or open shadow-casting mesh; compare\n");
	printf("the same camera with -no-shadows, then repair the caster topology.\n\n");
	printf("General options:\n");
	printf("  -h, --help             Show this help and exit\n");
	printf("  -V, --version          Show version and exit\n");
}

static bool cli_print_cameras(const char *path) {
	Scene scene;
	if (!load_scene(path, &scene)) return false;
	for (int i = 0; i < scene.ncameras; i++)
		printf("%s\t%s\n", scene.cameras[i].name, scene.cameras[i].comment);
	scene_free(&scene);
	return true;
}
#endif

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
  g_app->property_browser_win = create_property_browser_window();
}

static const char *scener_file_types[] = { ".blks", NULL };

#ifndef BUILD_AS_GEM
static bool scener_open_file_handler(const char *path) {
  return scener_open_file_path(path);
}
#endif

static void scener_camera_filename(const char *name, char *filename, size_t size) {
	size_t offset = 0;
	for (const unsigned char *p = (const unsigned char *)name; *p && offset + 1 < size; p++)
		filename[offset++] = isalnum(*p) || *p == '-' || *p == '_' ? (char)*p : '_';
	if (!offset && size > 1) filename[offset++] = 'c';
	filename[offset] = '\0';
}

static bool scener_write_camera(scene_doc_t *doc, const char *camera_name) {
	char filename[128], path[1200];
	if (!doc || !camera_name || !camera_name[0]) return false;
	scener_camera_filename(camera_name, filename, sizeof(filename));
	int n = snprintf(path, sizeof(path), "%s/%s.%s", g_cli.output_dir, filename,
	                 g_cli.format);
	if (n < 0 || (size_t)n >= sizeof(path)) return false;
	bool found = false;
	for (int i = 0; i < doc->scene.ncameras; i++)
		if (!strcmp(doc->scene.cameras[i].name, camera_name)) { found = true; break; }
	if (!found) {
		fprintf(stderr, "[scener] camera not found: %s\n", camera_name);
		return false;
	}
	scene_select_camera(&doc->scene, camera_name);
	int width = g_cli.width, height = g_cli.height;

	Scene *scene = &doc->scene;
	scene->camFov = scene->camFov > 0 ? scene->camFov : DEFAULT_FOV;
	vec3 dir = vsub(scene->camLook, scene->camPos);
	if (vlen(dir) < DIR_EPSILON) dir = v3(0, 0, -1);
	dir = vnorm(dir);
	mat4 proj = mat4_perspective(scene->camFov, (float)width / (float)height, PERSP_NEAR, PERSP_FAR);
	mat4 view = mat4_lookat(scene->camPos, scene->camLook, v3(0, 1, 0));

	ui_begin_frame();
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
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "[scener] cannot create render target: %dx%d\n", width, height);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &fbo);
		glDeleteTextures(1, &color);
		glDeleteRenderbuffers(1, &depth);
		return false;
	}

	glViewport(0, 0, width, height);
	glScissor(0, 0, width, height);
	glEnable(GL_SCISSOR_TEST);
	int flags = g_cli.debug_flags | DBG_HIDE_CHARS | DBG_HIDE_LIGHTS | DBG_HIDE_GIZMOS;
	render_frame(scene, width, height, proj, view, scene->camPos, dir, flags);

	size_t bytes = (size_t)width * (size_t)height * 4;
	uint8_t *pixels = malloc(bytes);
	bool ok = pixels && capture_framebuffer_rgba(width, height, pixels) &&
		scener_save_screenshot(path, pixels, width, height);
	free(pixels);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(1, &color);
	glDeleteRenderbuffers(1, &depth);
	if (ok) fprintf(stderr, "[scener] rendered camera=%s output=%s size=%dx%d\n",
	                camera_name, path, width, height);
	return ok;
}

static bool scener_write_layout(scene_doc_t *doc) {
	Scene *scene = &doc->scene;
	if (scene->nobjs == 0) { fprintf(stderr, "[scener] --layout: scene has no objects\n"); return false; }

	/* compute XYZ bounds over all objects */
	float xmin=FLT_MAX,xmax=-FLT_MAX,ymin=FLT_MAX,ymax=-FLT_MAX,zmin=FLT_MAX,zmax=-FLT_MAX;
	for (int i = 0; i < scene->nobjs; i++) {
		vec3 omin, omax;
		scene_get_obj_bounds(scene, i, &omin, &omax);
		if (omin.x<xmin) xmin=omin.x; if (omax.x>xmax) xmax=omax.x;
		if (omin.y<ymin) ymin=omin.y; if (omax.y>ymax) ymax=omax.y;
		if (omin.z<zmin) zmin=omin.z; if (omax.z>zmax) zmax=omax.z;
	}
	float wx = xmax - xmin, wz = zmax - zmin;
	if (wx < 0.01f || wz < 0.01f) { fprintf(stderr, "[scener] --layout: degenerate bounds\n"); return false; }

	/* scene uses meters internally (cm/100 at parse time); scale is px/cm */
	float scale = g_cli.layout_scale > 0.0f ? g_cli.layout_scale : 2.0f;
	int width  = (int)ceilf(wx * 100.0f * scale);
	int height = (int)ceilf(wz * 100.0f * scale);

	/* orthographic camera inside the room at 80% height — ceiling is above = behind camera */
	float cx = (xmin + xmax) * 0.5f, cz = (zmin + zmax) * 0.5f;
	float camH = ymin + (ymax - ymin) * 0.80f;
	vec3 camPos  = v3(cx, camH, cz);
	vec3 camLook = v3(cx, ymin, cz - 0.01f); /* tiny Z offset for stable up vector */
	vec3 camUp   = v3(0, 0, -1);             /* north-up: -Z world = top of plan */
	float hw = wx * 0.5f, hd = wz * 0.5f;
	mat4 view = mat4_lookat(camPos, camLook, camUp);
	float zdepth = camH - ymin + 1.0f;       /* objects in front of camera; ceiling behind */
	mat4 proj = mat4_ortho(-hw, hw, -hd, hd, 0.01f, zdepth);

	char path[1200];
	int n = snprintf(path, sizeof(path), "%s/layout.%s", g_cli.output_dir, g_cli.format);
	if (n < 0 || (size_t)n >= sizeof(path)) return false;

	ui_begin_frame();
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
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "[scener] cannot create render target: %dx%d\n", width, height);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &color); glDeleteRenderbuffers(1, &depth);
		return false;
	}
	glViewport(0, 0, width, height);
	glScissor(0, 0, width, height);
	glEnable(GL_SCISSOR_TEST);
	int flags = g_cli.debug_flags | DBG_HIDE_CHARS | DBG_HIDE_LIGHTS | DBG_HIDE_GIZMOS;
	vec3 dir = vnorm(vsub(camLook, camPos));
	render_frame(scene, width, height, proj, view, camPos, dir, flags);

	size_t bytes = (size_t)width * (size_t)height * 4;
	uint8_t *pixels = malloc(bytes);
	bool ok = pixels && capture_framebuffer_rgba(width, height, pixels) &&
		scener_save_screenshot(path, pixels, width, height);
	free(pixels);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &color); glDeleteRenderbuffers(1, &depth);
	if (ok) fprintf(stderr, "[scener] layout output=%s size=%dx%d scale=%.1fpx/cm\n",
		path, width, height, scale);
	return ok;
}

static bool scener_render_scene(scene_doc_t *doc) {
	if (!axMkDir(g_cli.output_dir) && !axPathExists(g_cli.output_dir)) {
		fprintf(stderr, "[scener] cannot create output directory: %s\n", g_cli.output_dir);
		return false;
	}
	if (g_cli.layout_mode) return scener_write_layout(doc);
	if (g_cli.camera_name[0]) return scener_write_camera(doc, g_cli.camera_name);
	for (int i = 0; i < doc->scene.ncameras; i++)
		if (!scener_write_camera(doc, doc->scene.cameras[i].name)) return false;
	return true;
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
	if (g_cli.screenshot_mode)
		ui_begin_frame();
  shader_init();

  {
		char icons_path[4096];
		int n = snprintf(icons_path, sizeof(icons_path), "%s/../share/scener/icons",
                     ui_get_exe_dir());
    if (n > 0 && (size_t)n < sizeof(icons_path))
			bmp_add_icons_dir(icons_path);
  }

  if (!g_cli.screenshot_mode)
    create_app_windows(hinstance);

  g_app->accel = load_accelerators(scener_default_accels, scener_default_accel_count);
  g_app->navigation_accel = load_accelerators(kNavigationAccelEntries, kNavigationAccelCount);
  if (g_app->menubar_win)
    send_message(g_app->menubar_win, kMenuBarMessageSetAccelerators, 0, g_app->accel);

  scene_doc_t *doc = create_document_ex(g_cli.scene_path[0] ? g_cli.scene_path : NULL,
                                        !g_cli.screenshot_mode);
  if (!doc) return false;

  if (g_cli.camera_name[0])
    scene_select_camera(&doc->scene, g_cli.camera_name);

  if (g_cli.screenshot_mode) {
		if (!scener_render_scene(doc)) return false;
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

  if (g_app->property_browser_win && is_window(g_app->property_browser_win))
    destroy_window(g_app->property_browser_win);
  g_app->property_browser_win = NULL;

  if (g_app->chrome_win && is_window(g_app->chrome_win))
    destroy_window(g_app->chrome_win);
  g_app->chrome_win = g_app->menubar_win = g_app->main_toolbar_win = NULL;

  while (g_app->docs)
    close_document(g_app->docs);

  shader_deinit();

  free(g_app);
  g_app = NULL;
}

GEM_DEFINE("SimpleSketch3D", SCENER_VERSION, gem_init, gem_shutdown, scener_file_types)

#ifndef BUILD_AS_GEM
int main(int argc, char *argv[]) {
  cli_parse(argc, argv);
	if (g_cli.show_help) { cli_print_help(); return 0; }
	if (g_cli.show_version) { printf("scener %s\n", SCENER_VERSION); return 0; }
	if (g_cli.invalid_format) {
		fprintf(stderr, "scener: --format must be png or jpg\n");
		return 2;
	}
	if ((g_cli.screenshot_mode || g_cli.list_cameras) && !g_cli.scene_path[0]) {
		fprintf(stderr, "scener: a .blks or .blk file is required\n");
		return 2;
	}
	if (g_cli.list_cameras) return cli_print_cameras(g_cli.scene_path) ? 0 : 1;
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
