#include "user.h"
#include "icons.h"

#include <ctype.h>

typedef struct {
  fe_component_desc_t desc;
} window_class_t;

static window_class_t g_window_classes[MAX_WINDOW_CLASSES];
static int g_window_class_count = 0;

static bool is_valid_toolbox_icon(int icon) {
  if (icon >= SYSICON_BASE && icon <= sysicon_yield_add)
    return true;
  return false;
}

#define FE_MAX_COMPONENT_PLUGINS 32

static void *g_component_plugin_handles[FE_MAX_COMPONENT_PLUGINS];
static int g_component_plugin_count = 0;

static bool reg_streq(const char *a, const char *b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
      return false;
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

bool register_window_class(const fe_component_desc_t *desc) {
  if (!desc || !desc->class_name || !*desc->class_name || !desc->proc) return false;
  for (int i = 0; i < g_window_class_count; i++) {
    if (reg_streq(g_window_classes[i].desc.class_name, desc->class_name))
      return true;  // already registered — idempotent on all platforms
  }
  if (g_window_class_count >= MAX_WINDOW_CLASSES) return false;
  g_window_classes[g_window_class_count++].desc = *desc;
  return true;
}

int get_num_window_classes(void) {
  return g_window_class_count;
}

const fe_component_desc_t *get_window_class_at_index(int index) {
  if (index < 0 || index >= g_window_class_count)
    return NULL;
  return &g_window_classes[index].desc;
}

const fe_component_desc_t *find_window_class_desc_by_proc(winproc_t proc) {
  if (!proc)
    return NULL;
  for (int i = 0; i < g_window_class_count; i++) {
    if (g_window_classes[i].desc.proc == proc)
      return &g_window_classes[i].desc;
  }
  return NULL;
}

winproc_t find_window_class_proc(const char *class_name) {
  if (!class_name || !*class_name)
    return NULL;
  const fe_component_desc_t *desc = find_window_class_desc(class_name);
  return desc ? desc->proc : NULL;
}

const fe_component_desc_t *find_window_class_desc(const char *class_name) {
  if (!class_name || !*class_name)
    return NULL;
  for (int i = 0; i < g_window_class_count; i++) {
    if (reg_streq(g_window_classes[i].desc.class_name, class_name))
      return &g_window_classes[i].desc;
  }
  return NULL;
}

isize16_t get_class_default_size(const char *class_name) {
  const fe_component_desc_t *desc = find_window_class_desc(class_name);
  return desc ? desc->default_layout_size : (isize16_t){0, 0};
}

flags_t get_class_default_flags(const char *class_name) {
  const fe_component_desc_t *desc = find_window_class_desc(class_name);
  return desc ? desc->default_flags : 0;
}

uint8_t get_class_default_h_align(const char *class_name) {
  const fe_component_desc_t *desc = find_window_class_desc(class_name);
  return desc ? desc->default_h_align : LAYOUT_ALIGN_STRETCH;
}

uint8_t get_class_default_v_align(const char *class_name) {
  const fe_component_desc_t *desc = find_window_class_desc(class_name);
  return desc ? desc->default_v_align : LAYOUT_ALIGN_STRETCH;
}

int fe_component_count(void) {
  return get_num_window_classes();
}

const fe_component_desc_t *fe_component_at(int index) {
  return get_window_class_at_index(index);
}

const fe_component_desc_t *fe_component_by_id(int id) {
  return fe_component_at(id);
}

int fe_component_id_of(const fe_component_desc_t *desc) {
  if (!desc)
    return -1;
  for (int i = 0; i < fe_component_count(); i++) {
    if (fe_component_at(i) == desc)
      return i;
  }
  return -1;
}

const fe_component_desc_t *fe_component_by_class_name(const char *class_name) {
  if (!class_name || !*class_name)
    return NULL;
  return find_window_class_desc(class_name);
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
    if (!d)
      continue;
    fe_component_desc_t stored = *d;
    if (!is_valid_toolbox_icon(stored.toolbox_icon))
      stored.toolbox_icon = sysicon_puzzle;
    register_window_class(&stored);
  }

  g_component_plugin_handles[g_component_plugin_count++] = handle;
  return true;
}

void fe_unload_component_plugins(void) {
  for (int i = g_component_plugin_count - 1; i >= 0; i--)
    axDynlibClose(g_component_plugin_handles[i]);
  g_component_plugin_count = 0;
}
