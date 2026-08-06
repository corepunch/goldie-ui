#include "../../ui.h"
#include "../../gem.h"
#include "../../platform/platform.h"

#include "../../examples/socialfeed/socialfeed.h"

#ifndef SHAREDIR
#define SHAREDIR "."
#endif

#define SF_PLUGIN_DB_PATH_MAX 1024

static database_t *g_socialfeed_db = NULL;

static bool resolve_socialfeed_seed_path(char *out, size_t out_sz) {
  if (!out || out_sz == 0) return false;
  out[0] = '\0';

  char candidate[SF_PLUGIN_DB_PATH_MAX];
  snprintf(candidate, sizeof(candidate), "%s/socialfeed_seed.xml", SHAREDIR);
  if (axPathExists(candidate)) {
    snprintf(out, out_sz, "%s", candidate);
    return true;
  }

  const char *exe_dir = ui_get_exe_dir();
  if (!exe_dir || !*exe_dir)
    return false;

  snprintf(candidate, sizeof(candidate), "%s/%s/socialfeed_seed.xml", exe_dir, SHAREDIR);
  if (axPathExists(candidate)) {
    snprintf(out, out_sz, "%s", candidate);
    return true;
  }

  snprintf(candidate, sizeof(candidate),
           "%s/../share/socialfeed/socialfeed_seed.xml", exe_dir);
  if (axPathExists(candidate)) {
    snprintf(out, out_sz, "%s", candidate);
    return true;
  }

  snprintf(out, out_sz, "%s/../share/socialfeed/socialfeed_seed.xml", exe_dir);
  return false;
}

GEM_EXPORT bool fe_plugin_init(void) {
  if (g_socialfeed_db)
    return true;

  DB_CLASS(db_simple_xml);

  char db_path[SF_PLUGIN_DB_PATH_MAX];
  resolve_socialfeed_seed_path(db_path, sizeof(db_path));

  g_socialfeed_db = create_database("socialfeed", "db_simple_xml", db_path);
  if (!g_socialfeed_db)
    return false;

  // Expose both short and explicit names for declarative bindings.
  register_database("db", g_socialfeed_db);
  register_database("socialfeed", g_socialfeed_db);
  ui_set_database(g_socialfeed_db);
  return true;
}

GEM_EXPORT void fe_plugin_shutdown(void) {
  if (!g_socialfeed_db)
    return;

  if (ui_get_database() == g_socialfeed_db)
    ui_set_database(NULL);

  destroy_database(g_socialfeed_db);
  g_socialfeed_db = NULL;
}

GEM_EXPORT int fe_plugin_class_count(void) {
  return 0;
}

GEM_EXPORT const fe_component_desc_t *fe_plugin_class_desc(int i) {
  (void)i;
  return NULL;
}

GEM_EXPORT const char *fe_plugin_description(void) {
  return "SocialFeed database bridge for FormEditor";
}

GEM_EXPORT uint32_t fe_plugin_version(void) {
  return FE_PLUGIN_VERSION;
}
