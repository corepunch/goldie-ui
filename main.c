#include "scener.h"
#include <orion/gem.h>

app_state_t *g_app = NULL;

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

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
  g_app = calloc(1, sizeof(app_state_t));
  if (!g_app) return false;

  g_app->hinstance    = hinstance;
  g_app->debug_flags  = 0;

#ifndef BUILD_AS_GEM
  ui_register_open_file_handler(scener_open_file_handler);
#endif

  srand((unsigned int)time(NULL));
  register_commctl_classes();
  shader_init();

  create_app_windows(hinstance);

  g_app->accel = load_accelerators(kAccelEntries, kAccelCount);
  g_app->navigation_accel = load_accelerators(kNavigationAccelEntries, kNavigationAccelCount);
  if (g_app->menubar_win)
    send_message(g_app->menubar_win, kMenuBarMessageSetAccelerators, 0, g_app->accel);

  int opened = 0;
  for (int i = 1; i < argc; i++) {
    if (argv[i] && argv[i][0] && scener_open_file_path(argv[i]))
      opened++;
  }
  if (opened == 0)
    create_document(NULL);

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

GEM_STANDALONE_MAIN("SimpleSketch3D", UI_INIT_DESKTOP, 1280, 800,
                    g_app->menubar_win, scener_active_accelerators())
