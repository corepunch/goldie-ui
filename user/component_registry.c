#include "user.h"
#include "icons.h"
#include "../examples/formeditor/formeditor.h"

static bool class_name_equals(const char *a, const char *b) {
  if (!a || !b)
    return false;
  while (*a && *b) {
    char ca = *a;
    char cb = *b;
    if (ca >= 'A' && ca <= 'Z')
      ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z')
      cb = (char)(cb - 'A' + 'a');
    if (ca != cb)
      return false;
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

static bool is_valid_toolbox_icon(int icon) {
  if (icon >= SYSICON_BASE && icon <= sysicon_yield_add)
    return true;
  if (icon >= 0 && icon < IC_ICON_COUNT)
    return true;
  return false;
}

#define FE_MAX_COMPONENT_PLUGINS 32

typedef struct {
  fe_component_desc_t desc;
} fe_component_entry_t;

static fe_component_entry_t g_components[FE_MAX_COMPONENTS];
static int g_component_count = 0;

static void *g_component_plugin_handles[FE_MAX_COMPONENT_PLUGINS];
static int g_component_plugin_count = 0;

static bool fe_component_exists(const char *class_name) {
  for (int i = 0; i < g_component_count; i++) {
    const fe_component_desc_t *d = &g_components[i].desc;
    if (class_name && d->class_name && class_name_equals(class_name, d->class_name))
      return true;
  }
  return false;
}

bool fe_register_component(const fe_component_desc_t *desc) {
  if (!desc || !desc->class_name || !desc->name_prefix || !desc->proc)
    return false;
  if (g_component_count >= FE_MAX_COMPONENTS)
    return false;
  if (fe_component_exists(desc->class_name))
    return false;
  if (!register_window_class(desc))
    return false;

  fe_component_desc_t stored = *desc;
  if (!is_valid_toolbox_icon(stored.toolbox_icon))
    stored.toolbox_icon = sysicon_puzzle;

  g_components[g_component_count].desc = stored;
  g_component_count++;
  return true;
}

int fe_component_count(void) {
  return g_component_count;
}

const fe_component_desc_t *fe_component_at(int index) {
  if (index < 0 || index >= g_component_count)
    return NULL;
  return &g_components[index].desc;
}

const fe_component_desc_t *fe_component_by_id(int id) {
  return fe_component_at(id);
}

int fe_component_id_of(const fe_component_desc_t *desc) {
  if (!desc)
    return -1;
  for (int i = 0; i < g_component_count; i++) {
    if (&g_components[i].desc == desc)
      return i;
  }
  return -1;
}

const fe_component_desc_t *fe_component_by_class_name(const char *class_name) {
  if (!class_name || !*class_name)
    return NULL;
  for (int i = 0; i < g_component_count; i++) {
    const fe_component_desc_t *d = &g_components[i].desc;
    if (class_name_equals(d->class_name, class_name))
      return d;
  }
  return NULL;
}

bool fe_component_rejects_parent(const fe_component_desc_t *desc, window_t *target) {
  if (!desc || !desc->proc)
    return false;
  // Pass a zeroed dummy window so procs that guard on win != NULL don't crash.
  window_t dummy = {0};
  return desc->proc(&dummy, evCanParent, 0, target);
}

bool fe_load_component_plugin(const char *path) {
  if (!path || !*path)
    return false;
  if (g_component_plugin_count >= FE_MAX_COMPONENT_PLUGINS)
    return false;

  void *handle = axDynlibOpen(path);
  if (!handle)
    return false;

  fe_plugin_class_count_fn count_fn =
      (fe_plugin_class_count_fn)axDynlibSym(handle, "fe_plugin_class_count");
  fe_plugin_class_desc_fn desc_fn =
      (fe_plugin_class_desc_fn)axDynlibSym(handle, "fe_plugin_class_desc");

  if (!count_fn || !desc_fn) {
    axDynlibClose(handle);
    return false;
  }

  int n = count_fn();
  for (int i = 0; i < n; i++) {
    const fe_component_desc_t *d = desc_fn(i);
    if (d) fe_register_component(d);
  }

  g_component_plugin_handles[g_component_plugin_count++] = handle;
  return true;
}

void fe_unload_component_plugins(void) {
  for (int i = g_component_plugin_count - 1; i >= 0; i--)
    axDynlibClose(g_component_plugin_handles[i]);
  g_component_plugin_count = 0;
}
