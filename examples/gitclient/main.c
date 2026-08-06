// Git Client — entry point.

#include "gitclient.h"
#include "../../user/gem_magic.h"
#include "../../commctl/commctl.h"

// ============================================================
// Module-level application state
// ============================================================

static gc_state_t g_gc_state;
gc_state_t *g_gc = NULL;

// ============================================================
// gem_init / gem_shutdown
// ============================================================

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
#if GITCLIENT_DEBUG
  {
    char log_path[1024];
    int n = snprintf(log_path, sizeof(log_path), "%s/gitclient.log",
                     axSettingsDirectory());
    if (n > 0 && (size_t)n < sizeof(log_path) && axSetLogFile(log_path))
      GC_LOG("logging initialized: %s", axGetLogFile());
  }
#endif

  memset(&g_gc_state, 0, sizeof(g_gc_state));
  g_gc = &g_gc_state;

  g_gc->hinstance       = hinstance;
  g_gc->selected_commit = -1;
  g_gc->selected_file   = -1;
  g_gc->unified_diff    = true;

  // Register database class and create database.
  DB_CLASS(gitclient_db);
  g_gc->db = create_database("gitclient", "gitclient_db", NULL);
  if (!g_gc->db) return false;
  register_database("db", g_gc->db);
  ui_set_database(g_gc->db);
  GC_LOG("database ready: db=%p global=%p",
         (void *)g_gc->db, (void *)ui_get_database());

  // Register commctl classes (tableview, stack, grid, etc.).
  register_commctl_classes();

  // Load gitclient component plugin (DiffView, etc.).
  {
    char path[4096];
    int n = snprintf(path, sizeof(path), "%s/../lib/gitclient_components%s",
                     ui_get_exe_dir(), AX_DYNLIB_EXT);
    if (n > 0 && (size_t)n < sizeof(path))
      fe_load_component_plugin(path);
  }

  // Menubar + accelerators.
  gc_create_menubar();

  // Create main window from form definition.
  g_gc->main_win = create_window_from_form(&gc_main_window_form, 16, 32,
                                           NULL, gc_main_proc,
                                           hinstance, NULL);
  if (!g_gc->main_win) return false;
  show_window(g_gc->main_win, true);

  // Open an explicit repository, or use the launch directory when it is one.
  if (argc > 1 && argv[1] && argv[1][0]) {
    gc_open_repo(argv[1]);
  } else {
    git_repo_t *cwd_repo = git_repo_open(".");
    if (cwd_repo) {
      git_repo_close(cwd_repo);
      gc_open_repo(".");
    } else {
      GC_LOG("startup directory is not a repository; waiting for Open Repository");
    }
  }

  return true;
}

void gem_shutdown(void) {
  fe_unload_component_plugins();
  if (g_gc) {
    if (g_gc->db) {
      if (ui_get_database() == g_gc->db)
        ui_set_database(NULL);
      destroy_database(g_gc->db);
      g_gc->db = NULL;
    }
    if (g_gc->accel)
      free_accelerators(g_gc->accel);
    g_gc = NULL;
  }
#if GITCLIENT_DEBUG
  GC_LOG("logging shutdown");
  axSetLogFile(NULL);
#endif
}

GEM_DEFINE("Git Client", "1.0", gem_init, gem_shutdown, NULL)

GEM_STANDALONE_MAIN("Git Client", UI_INIT_DESKTOP, SCREEN_W, SCREEN_H,
                    g_gc->menubar_win, g_gc->accel)
