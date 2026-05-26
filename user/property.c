#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "user.h"

static const ui_prop_desc_t k_window_props[] = {
  UI_PROP_CLASS(UI_PROP_WINDOW_CLASS, "class", proc),
  UI_PROP_UNSIGNED(UI_PROP_WINDOW_ID, "id", id),
  UI_PROP_STR(UI_PROP_WINDOW_TITLE, "title", title),
  UI_PROP_INT(UI_PROP_WINDOW_X, "x", frame.x),
  UI_PROP_INT(UI_PROP_WINDOW_Y, "y", frame.y),
  UI_PROP_INT(UI_PROP_WINDOW_WIDTH, "width", frame.w),
  UI_PROP_INT(UI_PROP_WINDOW_HEIGHT, "height", frame.h),
  UI_PROP_STYLE(UI_PROP_WINDOW_FLAGS, "style", flags),
  UI_PROP_INT(UI_PROP_WINDOW_LAYOUT_W, "layout_width", layout.layout_fixed_w),
  UI_PROP_INT(UI_PROP_WINDOW_LAYOUT_H, "layout_height", layout.layout_fixed_h),
};

static uint16_t ui_prop_kind_for_type(uint16_t type) {
  switch ((ui_property_type_t)type) {
    case UI_PROP_TYPE_STRING:
    case UI_PROP_TYPE_CLASS_PROC:
      return UI_PROP_KIND_STRING;
    case UI_PROP_TYPE_INT:
    case UI_PROP_TYPE_UNSIGNED:
      return UI_PROP_KIND_INT;
    case UI_PROP_TYPE_STYLE:
      return UI_PROP_KIND_STYLE;
    case UI_PROP_TYPE_BOOL:
      return UI_PROP_KIND_BOOL;
  }
  return UI_PROP_KIND_STRING;
}

static int64_t ui_prop_read_int(const char *base, size_t size) {
  switch (size) {
    case 1: return *(const int8_t *)base;
    case 2: return *(const int16_t *)base;
    case 4: return *(const int32_t *)base;
    case 8: return *(const int64_t *)base;
  }
  return 0;
}

static uint64_t ui_prop_read_unsigned(const char *base, size_t size) {
  switch (size) {
    case 1: return *(const uint8_t *)base;
    case 2: return *(const uint16_t *)base;
    case 4: return *(const uint32_t *)base;
    case 8: return *(const uint64_t *)base;
  }
  return 0;
}

static void ui_prop_format_value(window_t *win, const ui_prop_desc_t *d, char *buf, size_t sz) {
  if (!buf || sz == 0)
    return;
  buf[0] = '\0';
  if (!win || !d)
    return;

  const char *base = (const char *)win + d->offset;
  switch ((ui_property_type_t)d->type) {
    case UI_PROP_TYPE_STRING: {
      int len = d->size > (size_t)INT_MAX ? INT_MAX : (int)d->size;
      snprintf(buf, sz, "%.*s", len, base);
      break;
    }
    case UI_PROP_TYPE_INT:
      snprintf(buf, sz, "%lld", (long long)ui_prop_read_int(base, d->size));
      break;
    case UI_PROP_TYPE_UNSIGNED:
      snprintf(buf, sz, "%llu", (unsigned long long)ui_prop_read_unsigned(base, d->size));
      break;
    case UI_PROP_TYPE_STYLE: {
      uint64_t value = ui_prop_read_unsigned(base, d->size);
      if (d->size <= 2)
        snprintf(buf, sz, "0x%04llX", (unsigned long long)value);
      else if (d->size <= 4)
        snprintf(buf, sz, "0x%08llX", (unsigned long long)value);
      else
        snprintf(buf, sz, "0x%016llX", (unsigned long long)value);
      break;
    }
    case UI_PROP_TYPE_BOOL:
      snprintf(buf, sz, "%s", ui_prop_read_unsigned(base, d->size) ? "true" : "false");
      break;
    case UI_PROP_TYPE_CLASS_PROC: {
      winproc_t proc = *(const winproc_t *)base;
      const fe_component_desc_t *desc = find_window_class_desc_by_proc(proc);
      snprintf(buf, sz, "%s", desc && desc->class_name ? desc->class_name : "window");
      break;
    }
  }
}

int ui_query_props(window_t *win, const ui_prop_desc_t *descs, int desc_count, uint32_t capacity, ui_property_entry_t *out, int start) {
  int cap = (int)capacity;
  if (desc_count < 0)
    desc_count = 0;

  for (int i = 0; i < desc_count; i++) {
    int out_index = start + i;
    if (!out || out_index < 0 || out_index >= cap)
      continue;

    const ui_prop_desc_t *d = &descs[i];
    ui_property_entry_t *e = &out[out_index];
    char value[128] = {0};
    memset(e, 0, sizeof(*e));
    e->id = d->id;
    e->kind = ui_prop_kind_for_type(d->type);
    e->flags = d->flags;
    e->size = d->size;
    snprintf(e->name, sizeof(e->name), "%s", d->name ? d->name : "");
    ui_prop_format_value(win, d, value, sizeof(value));
    snprintf(e->value, sizeof(e->value), "%s", value);
  }
  return start + desc_count;
}

int ui_query_window_props(window_t *win, uint32_t capacity, ui_property_entry_t *out) {
  return ui_query_props(win, k_window_props, ARRAY_LEN(k_window_props), capacity, out, 0);
}
