// Git Client — entry point.

#include "gitclient.h"
#include "../../gem_magic.h"
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
  memset(&g_gc_state, 0, sizeof(g_gc_state));
  g_gc = &g_gc_state;

  g_gc->hinstance       = hinstance;
  g_gc->selected_commit = -1;
  g_gc->selected_file   = -1;
  g_gc->right_w         = PANEL_RIGHT_W_DEFAULT;

  // Register database class and create database.
  DB_CLASS(gitclient_db);
  g_gc->db = create_database("gitclient", "gitclient_db", NULL);
  if (!g_gc->db) return false;
  register_database("db", g_gc->db);

  // Register commctl classes (tableview, stack, grid, etc.).
  register_commctl_classes();

  // Menubar + accelerators.
  gc_create_menubar();

  // Calculate initial vsplit_y.
  int sh = ui_get_system_metrics(kSystemMetricScreenHeight);
  int mh = sh - MENUBAR_HEIGHT;
  g_gc->vsplit_y = (int)(mh * PANEL_VSPLIT_FRAC / 100);
  if (g_gc->vsplit_y < 60) g_gc->vsplit_y = 60;

  // Create main window from form definition.
  g_gc->main_win = create_window_from_form(&gc_main_window_form, 0, 0,
                                           NULL, gc_main_proc,
                                           hinstance, NULL);
  if (!g_gc->main_win) return false;
  show_window(g_gc->main_win, true);

  // If a path was passed on the command line, open it immediately.
  if (argc > 1 && argv[1] && argv[1][0])
    gc_open_repo(argv[1]);

  return true;
}

void gem_shutdown(void) {
  if (g_gc) {
    if (g_gc->db) {
      destroy_database(g_gc->db);
      g_gc->db = NULL;
    }
    if (g_gc->accel)
      free_accelerators(g_gc->accel);
    g_gc = NULL;
  }
}

GEM_DEFINE("Git Client", "1.0", gem_init, gem_shutdown, NULL)

GEM_STANDALONE_MAIN("Git Client", UI_INIT_DESKTOP, SCREEN_W, SCREEN_H,
                    g_gc->menubar_win, g_gc->accel)
