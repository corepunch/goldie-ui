#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../ui.h"
#include "../../gem_magic.h"
#include "build/generated/examples/vibeoffice/vibeoffice.h"
#include "models.h"
#include "tasks.h"

#define VIBE_SCREEN_W 1024
#define VIBE_SCREEN_H 768
#define INSPECTOR_POLL_MS   100

typedef struct vibe_icon_s vibe_icon_t;

typedef struct {
  vibe_icon_t *icon;
  window_t *win, *desk_label, *status_label, *model, *input, *submit, *output;
} inspector_t;

struct vibe_icon_s {
  int id, model_id;
  const char *title, *filename;
  uint32_t texture;
  int image_w, image_h;
  window_t *win;
  vibe_process_t process;
  inspector_t inspector;
};

static window_t *g_desktop, *g_controller;
static database_t *g_models_db;
static hinstance_t g_hinstance;
static uint32_t g_poll_timer;
static vibe_icon_t g_icons[] = {
  { 1, 1, "Manager",   "manager.png" },
  { 2, 2, "Developer", "developer.png" },
  { 3, 3, "Tester",    "tester.png" },
  { 4, 4, "Desk",      "desk.png" },
};

static vibe_icon_t *icon_from_window(window_t *win) {
  return win ? (vibe_icon_t *)send_message(win, icGetItemData, 0, NULL) : NULL;
}

static int icon_index(vibe_icon_t *icon) {
  return icon ? (int)(icon - g_icons) : -1;
}

static inspector_t *inspector_from_window(window_t *win) {
  window_t *root = win ? get_root_window(win) : NULL;
  return root ? (inspector_t *)root->userdata2 : NULL;
}

static uint32_t task_status_color(vibe_task_status_t status);

static result_t win_readonly_multiedit(window_t *win, uint32_t msg,
                                       uint32_t wparam, void *lparam) {
  switch (msg) {
    case evLeftButtonDown: case evLeftButtonUp: case evLeftButtonDoubleClick:
    case evTextInput: case evKeyDown: case evKeyUp: return true;
    default: return win_multiedit(win, msg, wparam, lparam);
  }
}

static result_t win_status_label(window_t *win, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate: win->flags |= WINDOW_NOTABSTOP; return true;
    case evMeasure: {
      layout_measure_t *m = (layout_measure_t *)lparam;
      if (m) { m->desired_w = text_strwidth(FONT_SMALL, win->title) + 14; m->desired_h = CONTROL_HEIGHT; }
      return true;
    }
    case evPaint: {
      inspector_t *inspector = inspector_from_window(win);
      vibe_icon_t *icon = inspector ? inspector->icon : NULL;
      vibe_task_t task;
      if (icon) vibe_task_read(icon->id, &task); else memset(&task, 0, sizeof(task));
      int dot_y = MAX(0, (win->frame.h - 8) / 2);
      fill_rect(task_status_color(task.status), R(0, dot_y, 8, 8));
      draw_text_small(win->title, 14, MAX(0, (win->frame.h - CONTROL_HEIGHT) / 2),
                      get_sys_color(brTextNormal));
      return true;
    }
    default: return false;
  }
}

static void set_control_text(window_t *win, const char *text) {
  if (!win) return;
  snprintf(win->title, sizeof(win->title), "%s", text ? text : "");
  invalidate_window(win);
}

static bool inspector_bind_controls(inspector_t *inspector) {
  if (!inspector || !inspector->win) return false;
  window_t *win = inspector->win;
  inspector->desk_label = get_window_item(win, ID_INSPECTOR_DESK);
  inspector->status_label = get_window_item(win, ID_INSPECTOR_STATUS);
  inspector->model = get_window_item(win, ID_INSPECTOR_MODEL);
  inspector->input = get_window_item(win, ID_INSPECTOR_INPUT);
  inspector->submit = get_window_item(win, ID_INSPECTOR_SUBMIT);
  inspector->output = get_window_item(win, ID_INSPECTOR_OUTPUT);
  if (!inspector->desk_label || !inspector->status_label || !inspector->model ||
      !inspector->input || !inspector->submit || !inspector->output)
    return false;
  inspector->status_label->proc = win_status_label;
  inspector->output->proc = win_readonly_multiedit;
  return true;
}

static uint32_t task_status_color(vibe_task_status_t status) {
  switch (status) {
    case VIBE_TASK_PENDING: return 0xffd09020;
    case VIBE_TASK_BUSY:    return 0xff3040d8;
    case VIBE_TASK_ERROR:   return 0xff20b8e0;
    default:                return 0xff30a060;
  }
}

static int model_index(int model_id) {
  for (int i = 0; i < vibe_model_count(); i++) {
    const vibe_model_info_t *model = vibe_model_at(i);
    if (model && model->id == model_id) return i;
  }
  return 0;
}

static void refresh_icon_model(vibe_icon_t *icon) {
  const vibe_model_info_t *model = icon ? vibe_model_by_id(icon->model_id) : NULL;
  if (!icon || !icon->win || !model) return;
  icon_badge_t badge = {
    .text = model->name, .background = 0xff705030,
    .foreground = 0xffffffff, .anchor = ICON_BADGE_TOP_RIGHT,
  };
  send_message(icon->win, icSetBadge, 0, &badge);
}

static void refresh_icon_status(vibe_icon_t *icon) {
  if (!icon || !icon->win) return;
  vibe_task_t task;
  vibe_task_read(icon->id, &task);
  icon_badge_t badge = {
    .text = " ", .background = task_status_color(task.status),
    .foreground = 0xffffffff, .anchor = ICON_BADGE_BOTTOM_RIGHT,
  };
  send_message(icon->win, icSetBadge, 1, &badge);
}

static void inspector_refresh(inspector_t *inspector, bool load_input) {
  vibe_icon_t *icon = inspector ? inspector->icon : NULL;
  if (!icon || !inspector_bind_controls(inspector)) return;
  vibe_task_t task;
  vibe_task_read(icon->id, &task);
  char title[128];
  snprintf(title, sizeof(title), "Inspector - %s", icon->title);
  set_control_text(inspector->win, title);
  snprintf(title, sizeof(title), "Desk: %s", icon->title);
  set_control_text(inspector->desk_label, title);
  snprintf(title, sizeof(title), "Status: %s", vibe_task_status_name(task.status));
  set_control_text(inspector->status_label, title);
  send_message(inspector->model, cbSetCurrentSelection, (uint32_t)model_index(icon->model_id), NULL);
  if (load_input) send_message(inspector->input, edSetText, 0, task.exists ? task.input : "");
  const char *output = task.output;
  if (!*output && task.status == VIBE_TASK_BUSY) output = "opencode is working…";
  else if (!*output && task.status == VIBE_TASK_PENDING) output = "Waiting to start opencode…";
  else if (!*output) output = "No response yet.";
  char shown[2048], current[2048];
  snprintf(shown, sizeof(shown), "%s", output);
  send_message(inspector->output, edGetText, sizeof(current), current);
  if (strcmp(shown, current)) send_message(inspector->output, edSetText, 0, shown);
  enable_window(inspector->submit, task.status != VIBE_TASK_PENDING && task.status != VIBE_TASK_BUSY);
  enable_window(inspector->model, task.status != VIBE_TASK_PENDING && task.status != VIBE_TASK_BUSY);
  invalidate_window(inspector->win);
}

static void refresh_from_task_files(void) {
  for (int i = 0; i < (int)ARRAY_LEN(g_icons); i++) {
    refresh_icon_status(&g_icons[i]);
    inspector_t *inspector = &g_icons[i].inspector;
    if (inspector->win && is_window(inspector->win)) inspector_refresh(inspector, false);
  }
}

static void inspector_submit(inspector_t *inspector) {
  vibe_icon_t *icon = inspector ? inspector->icon : NULL;
  if (!icon || !inspector_bind_controls(inspector)) return;
  vibe_task_t task;
  vibe_task_read(icon->id, &task);
  if (task.status == VIBE_TASK_PENDING || task.status == VIBE_TASK_BUSY) return;
  char input[VIBE_TASK_INPUT_MAX + 1], error[256];
  send_message(inspector->input, edGetText, sizeof(input), input);
  if (!input[0]) return;
  const vibe_model_info_t *model = vibe_model_by_id(icon->model_id);
  if (!model || !vibe_task_submit(&icon->process, icon->id, model->opencode_id,
                                  input, error, sizeof(error))) {
    (void)error; // the failure is reflected through the desk task file when possible
  }
  refresh_from_task_files();
}

static result_t win_inspector(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

static window_t *inspector_open(vibe_icon_t *icon) {
  if (!icon) return NULL;
  inspector_t *inspector = &icon->inspector;
  if (inspector->win && is_window(inspector->win)) {
    show_window(inspector->win, true);
    move_to_top(inspector->win);
    set_focus(inspector->win);
    return inspector->win;
  }

  memset(inspector, 0, sizeof(*inspector));
  inspector->icon = icon;
  int i = icon_index(icon);
  int sw = ui_get_system_metrics(kSystemMetricScreenWidth);
  int x = MAX(12, sw - vibeoffice_inspector_form.width - 12 - i * 24);
  int y = 24 + i * 24;
  database_t *previous_db = ui_get_database();
  ui_set_database(g_models_db);
  inspector->win = create_window_from_form(&vibeoffice_inspector_form, x, y, NULL,
                                            win_inspector, g_hinstance, NULL);
  ui_set_database(previous_db);
  if (!inspector->win) { memset(inspector, 0, sizeof(*inspector)); return NULL; }
  inspector->win->userdata2 = inspector;
  if (!inspector_bind_controls(inspector)) {
    destroy_window(inspector->win);
    return NULL;
  }
  inspector_refresh(inspector, true);
  show_window(inspector->win, true);
  move_to_top(inspector->win);
  set_focus(inspector->win);
  return inspector->win;
}

static result_t win_inspector(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  inspector_t *inspector = inspector_from_window(win);
  switch (msg) {
    case evCreate: window_layout_sync(win); return true;
    case evPaint:
      fill_rect(get_sys_color(brWindowBg), R(0, 0, win->frame.w, win->frame.h));
      return false;
    case evResize: window_layout_sync(win); return true;
    case evClose:
      show_window(win, false);
      return true;
    case evCommand:
      if (LOWORD(wparam) == ID_INSPECTOR_MODEL && HIWORD(wparam) == cbSelectionChange) {
        vibe_icon_t *icon = inspector ? inspector->icon : NULL;
        int model_id = kComboBoxError;
        if (inspector) send_message(inspector->model, cbGetCurrentValue, 0, &model_id);
        if (icon && vibe_model_by_id(model_id)) { icon->model_id = model_id; refresh_icon_model(icon); }
        return true;
      }
      if ((LOWORD(wparam) == ID_INSPECTOR_SUBMIT && HIWORD(wparam) == btnClicked) ||
          (LOWORD(wparam) == ID_INSPECTOR_INPUT && HIWORD(wparam) == edUpdate)) {
        inspector_submit(inspector); return true;
      }
      return false;
    case evDestroy:
      if (inspector && inspector->win == win) memset(inspector, 0, sizeof(*inspector));
      return true;
    default: return false;
  }
}

static result_t win_vibe_controller(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  (void)win;
  switch (msg) {
    case evCreate: g_poll_timer = axSetTimer(win, INSPECTOR_POLL_MS, NULL, true); return true;
    case evCommand:
      if (HIWORD(wparam) == icnOpen) return inspector_open(icon_from_window((window_t *)lparam)) != NULL;
      if (HIWORD(wparam) == icnSelectionChange || HIWORD(wparam) == icnClicked) return true;
      return false;
    case evTimer:
      if (wparam != g_poll_timer) return false;
      for (int i = 0; i < (int)ARRAY_LEN(g_icons); i++) vibe_task_poll(&g_icons[i].process);
      refresh_from_task_files();
      return true;
    case evDestroy:
      if (g_poll_timer) axCancelTimer(g_poll_timer);
      g_poll_timer = 0;
      return true;
    default: return false;
  }
}

static uint32_t load_asset_texture(const char *filename, int *out_w, int *out_h) {
  char path[4096];
  uint8_t *pixels = NULL;
  int w = 0, h = 0;
#ifdef SHAREDIR
  int n = snprintf(path, sizeof(path), "%s/" SHAREDIR "/%s", ui_get_exe_dir(), filename);
  if (n >= 0 && (size_t)n < sizeof(path)) pixels = load_image(path, &w, &h);
#endif
  if (!pixels) {
    int n = snprintf(path, sizeof(path), "examples/vibeoffice/share/%s", filename);
    if (n >= 0 && (size_t)n < sizeof(path)) pixels = load_image(path, &w, &h);
  }
  if (!pixels) return 0;
  uint32_t texture = R_CreateTextureRGBA(w, h, pixels, R_FILTER_LINEAR, R_WRAP_CLAMP);
  image_free(pixels);
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
  return texture;
}

static void layout_icons(void) {
  int sw = ui_get_system_metrics(kSystemMetricScreenWidth);
  int sh = ui_get_system_metrics(kSystemMetricScreenHeight);
  int count = (int)ARRAY_LEN(g_icons);
  int gap = 10, margin = 12;
  int cell_w = MAX(96, (sw - margin * 2 - gap * (count - 1)) / count);
  int icon_w = MIN(140, cell_w);
  int icon_h = MIN(160, MAX(120, sh - 80));
  int used_w = count * icon_w + (count - 1) * gap;
  int x = MAX(margin, (sw - used_w) / 2);
  int y = MAX(30, (sh - icon_h) / 2 - 20);
  for (int i = 0; i < count; i++, x += icon_w + gap) {
    if (!g_icons[i].win) continue;
    move_window(g_icons[i].win, x, y);
    resize_window(g_icons[i].win, icon_w, icon_h);
  }
}

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
  (void)argc; (void)argv;
  g_hinstance = hinstance;
  g_desktop = get_desktop_window();
  if (!g_desktop) return false;
  // Generated .orion forms resolve controls by registered class name.
  register_commctl_classes();
  g_models_db = vibe_models_create();
  if (!g_models_db) return false;
  register_database("models", g_models_db);
  for (int i = 0; i < (int)ARRAY_LEN(g_icons); i++) {
    vibe_task_recover_stale(g_icons[i].id);
    vibe_task_t task; vibe_task_read(g_icons[i].id, &task);
    const vibe_model_info_t *model = vibe_model_by_opencode_id(task.model);
    if (model) g_icons[i].model_id = model->id;
  }
  g_controller = create_window("", WINDOW_HIDDEN | WINDOW_NOTITLE | WINDOW_NORESIZE |
                               WINDOW_NOACTIVATE | WINDOW_NOTRAYBUTTON,
                               MAKERECT(0, 0, 1, 1), NULL,
                               win_vibe_controller, hinstance, NULL);
  if (!g_controller) {
    destroy_database(g_models_db); g_models_db = NULL;
    return false;
  }
  for (int i = 0; i < (int)ARRAY_LEN(g_icons); i++) {
    vibe_icon_t *item = &g_icons[i];
    item->texture = load_asset_texture(item->filename, &item->image_w, &item->image_h);
    icon_params_t params = {
      .image = { item->texture, item->image_w, item->image_h },
      .item_data = item,
      .draggable = true,
      .notify_window = g_controller,
    };
    item->win = create_window(item->title,
                              WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_TRANSPARENT | WINDOW_NOFILL,
                              MAKERECT(0, 0, 128, 144), g_desktop,
                              win_icon, hinstance, &params);
    if (!item->win) continue;
    refresh_icon_model(item);
    refresh_icon_status(item);
  }
  layout_icons();
  invalidate_window(g_desktop);
  return true;
}

void gem_shutdown(void) {
  for (int i = 0; i < (int)ARRAY_LEN(g_icons); i++) {
    inspector_t *inspector = &g_icons[i].inspector;
    if (inspector->win && is_window(inspector->win)) destroy_window(inspector->win);
    vibe_task_abort(&g_icons[i].process, "VibeOffice closed while opencode was running.");
    if (g_icons[i].win && is_window(g_icons[i].win)) destroy_window(g_icons[i].win);
    R_DeleteTexture(g_icons[i].texture);
    g_icons[i].texture = 0;
    g_icons[i].win = NULL;
  }
  if (g_controller && is_window(g_controller)) destroy_window(g_controller);
  g_controller = NULL;
  destroy_database(g_models_db); g_models_db = NULL;
  g_desktop = NULL; g_hinstance = 0;
}

GEM_DEFINE("Vibe Office", "0.1", gem_init, gem_shutdown, NULL)
GEM_STANDALONE_MAIN("Vibe Office", UI_INIT_DESKTOP, VIBE_SCREEN_W, VIBE_SCREEN_H, NULL, NULL)
