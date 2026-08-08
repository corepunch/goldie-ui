#include "scener.h"
#include <orion/user/draw.h>

enum {
  CP_WIDTH = 240,
  CP_TAB_HEIGHT = 22,
  CP_HEADER_HEIGHT = 26,
};

typedef struct {
  int active_tab;
  window_t *create_content;
  window_t *modify_content;
} cp_state_t;

static const char *kTabNames[] = { "Create", "Modify", "Hierarchy", "Motion", "Display", "Utilities" };
static const int   kTabIds[]   = { ID_CP_TAB_CREATE, ID_CP_TAB_MODIFY, ID_CP_TAB_HIERARCHY,
                                   ID_CP_TAB_MOTION, ID_CP_TAB_DISPLAY, ID_CP_TAB_UTILITIES };
#define CP_TAB_COUNT (int)(sizeof(kTabNames) / sizeof(kTabNames[0]))

static void cp_switch_tab(window_t *win, cp_state_t *st, int tab) {
  if (tab == st->active_tab) return;
  st->active_tab = tab;
  if (st->create_content)
    window_set_state(st->create_content, WINDOW_STATE_VISIBLE, tab == 0);
  if (st->modify_content)
    window_set_state(st->modify_content, WINDOW_STATE_VISIBLE, tab == 1);
  invalidate_window(win);
}

static void cp_draw_tabs(window_t *win, cp_state_t *st) {
  irect16_t cr = get_client_rect(win);
  int tab_w = cr.w / CP_TAB_COUNT;
  for (int i = 0; i < CP_TAB_COUNT; i++) {
    irect16_t r = { i * tab_w, 0, (int16_t)tab_w, CP_TAB_HEIGHT };
    bool active = (i == st->active_tab);
    if (active) {
      fill_rect(get_sys_color(brWindowBg), r);
      draw_text_small(kTabNames[i], r.x + 4, r.y + 4, get_sys_color(brTextNormal));
    } else {
      fill_rect(get_sys_color(brButtonBg), r);
      draw_text_small(kTabNames[i], r.x + 4, r.y + 4, get_sys_color(brTextDisabled));
    }
    fill_rect(get_sys_color(brDarkEdge), R(r.x, r.y + r.h - 1, r.w, 1));
  }
}

// ============================================================
// Create tab content
// ============================================================

enum {
  CP_CREATE_BOX,
  CP_CREATE_SPHERE,
  CP_CREATE_CYLINDER,
  CP_CREATE_CONE,
  CP_CREATE_TORUS,
  CP_CREATE_PRISM,
  CP_CREATE_CAPSULE,
  CP_CREATE_ARCH,
  CP_CREATE_POINT_LIGHT,
  CP_CREATE_DIR_LIGHT,
  CP_CREATE_CAMERA,
};

typedef struct { const char *label; int id; } cp_create_item_t;

static const cp_create_item_t kCreateGeometry[] = {
  { "Box",       ID_CREATE_BOX },
  { "Sphere",    ID_CREATE_SPHERE },
  { "Cylinder",  ID_CREATE_CYLINDER },
  { "Cone",      ID_CREATE_CONE },
  { "Torus",     ID_CREATE_TORUS },
  { "Prism",     ID_CREATE_PRISM },
  { "Capsule",   ID_CREATE_CAPSULE },
  { "Arch",      ID_CREATE_ARCH },
};

static const cp_create_item_t kCreateLights[] = {
  { "Point Light",       ID_CREATE_POINT_LIGHT },
  { "Directional Light", ID_CREATE_DIRECTIONAL_LIGHT },
};

static const cp_create_item_t kCreateOther[] = {
  { "Camera from View",  ID_CREATE_CAMERA },
};

static void cp_draw_section_header(const char *title, int x, int *y, int w) {
  draw_text_small(title, x + 4, *y + 2, get_sys_color(brTextNormal));
  *y += CP_HEADER_HEIGHT;
  fill_rect(get_sys_color(brDarkEdge), R(x, *y - 2, w, 1));
}

static void cp_draw_create_buttons(const cp_create_item_t *items, int count,
                                   int x, int *y, int w) {
  int btn_h = 20;
  int btn_w = (w - 8) / 2;
  for (int i = 0; i < count; i++) {
    int col = i % 2;
    int bx = x + 4 + col * (btn_w + 2);
    irect16_t r = { bx, *y, btn_w, btn_h };
    draw_button(r, 1, 1, false);
    int tw = strwidth(items[i].label);
    int tx = r.x + (r.w - tw) / 2;
    int ty = r.y + (r.h - CHAR_HEIGHT) / 2;
    draw_text_small(items[i].label, tx, ty, get_sys_color(brTextNormal));
    if (col == 1 || i == count - 1) *y += btn_h + 2;
  }
}

result_t win_cp_create(window_t *win, uint32_t msg,
                       uint32_t wparam, void *lparam) {
  (void)wparam; (void)lparam;
  switch (msg) {
    case evPaint: {
      irect16_t cr = get_client_rect(win);
      fill_rect(get_sys_color(brWindowBg), cr);
      int x = 0, y = 4;
      cp_draw_section_header("Geometry", x, &y, cr.w);
      cp_draw_create_buttons(kCreateGeometry,
                             (int)(sizeof(kCreateGeometry)/sizeof(kCreateGeometry[0])),
                             x, &y, cr.w);
      y += 6;
      cp_draw_section_header("Lights", x, &y, cr.w);
      cp_draw_create_buttons(kCreateLights,
                             (int)(sizeof(kCreateLights)/sizeof(kCreateLights[0])),
                             x, &y, cr.w);
      y += 6;
      cp_draw_section_header("Helpers", x, &y, cr.w);
      cp_draw_create_buttons(kCreateOther,
                             (int)(sizeof(kCreateOther)/sizeof(kCreateOther[0])),
                             x, &y, cr.w);
      return true;
    }

    case evLeftButtonDown: {
      int mx = (int16_t)LOWORD(wparam);
      int my = (int16_t)HIWORD(wparam);
      irect16_t cr = get_client_rect(win);
      int btn_h = 20;
      int btn_w = (cr.w - 8) / 2;
      int y = CP_HEADER_HEIGHT + 4;

      // Geometry section
      int ngeo = (int)(sizeof(kCreateGeometry)/sizeof(kCreateGeometry[0]));
      for (int i = 0; i < ngeo; i++) {
        int col = i % 2;
        int bx = 4 + col * (btn_w + 2);
        if (mx >= bx && mx < bx + btn_w && my >= y && my < y + btn_h) {
          handle_menu_command((uint16_t)kCreateGeometry[i].id);
          return true;
        }
        if (col == 1 || i == ngeo - 1) y += btn_h + 2;
      }

      y += 6 + CP_HEADER_HEIGHT;
      // Lights section
      int nlt = (int)(sizeof(kCreateLights)/sizeof(kCreateLights[0]));
      for (int i = 0; i < nlt; i++) {
        int col = i % 2;
        int bx = 4 + col * (btn_w + 2);
        if (mx >= bx && mx < bx + btn_w && my >= y && my < y + btn_h) {
          handle_menu_command((uint16_t)kCreateLights[i].id);
          return true;
        }
        if (col == 1 || i == nlt - 1) y += btn_h + 2;
      }

      y += 6 + CP_HEADER_HEIGHT;
      // Helpers section
      int nother = (int)(sizeof(kCreateOther)/sizeof(kCreateOther[0]));
      for (int i = 0; i < nother; i++) {
        int col = i % 2;
        int bx = 4 + col * (btn_w + 2);
        if (mx >= bx && mx < bx + btn_w && my >= y && my < y + btn_h) {
          handle_menu_command((uint16_t)kCreateOther[i].id);
          return true;
        }
        if (col == 1 || i == nother - 1) y += btn_h + 2;
      }
      return true;
    }

    default:
      return false;
  }
}

// ============================================================
// Modify tab content
// ============================================================

typedef struct { const char *label; int id; } cp_modify_item_t;

static const cp_modify_item_t kModifyModifiers[] = {
  { "Taper",   ID_MODIFY_TAPER },
  { "Twist",   ID_MODIFY_TWIST },
  { "Bend",    ID_MODIFY_BEND },
  { "Stretch", ID_MODIFY_STRETCH },
  { "Skew",    ID_MODIFY_SKEW },
  { "Extrude", ID_MODIFY_EXTRUDE },
  { "Mirror",  ID_MODIFY_MIRROR },
  { "Noise",   ID_MODIFY_NOISE },
  { "Shell",   ID_MODIFY_SHELL },
  { "Array",   ID_MODIFY_ARRAY },
};
#define CP_MODIFY_COUNT (int)(sizeof(kModifyModifiers)/sizeof(kModifyModifiers[0]))

result_t win_cp_modify(window_t *win, uint32_t msg,
                       uint32_t wparam, void *lparam) {
  (void)wparam; (void)lparam;
  switch (msg) {
    case evPaint: {
      irect16_t cr = get_client_rect(win);
      fill_rect(get_sys_color(brWindowBg), cr);
      int x = 0, y = 4;
      cp_draw_section_header("Modifiers", x, &y, cr.w);
      cp_draw_create_buttons((const cp_create_item_t *)kModifyModifiers,
                             CP_MODIFY_COUNT,
                             x, &y, cr.w);
      return true;
    }

    case evLeftButtonDown: {
      int mx = (int16_t)LOWORD(wparam);
      int my = (int16_t)HIWORD(wparam);
      irect16_t cr = get_client_rect(win);
      int btn_h = 20;
      int btn_w = (cr.w - 8) / 2;
      int y = CP_HEADER_HEIGHT + 4;
      int nmod = CP_MODIFY_COUNT;
      for (int i = 0; i < nmod; i++) {
        int col = i % 2;
        int bx = 4 + col * (btn_w + 2);
        if (mx >= bx && mx < bx + btn_w && my >= y && my < y + btn_h) {
          handle_menu_command((uint16_t)kModifyModifiers[i].id);
          return true;
        }
        if (col == 1 || i == nmod - 1) y += btn_h + 2;
      }
      return true;
    }

    default:
      return false;
  }
}

// ============================================================
// Command panel main window
// ============================================================

result_t win_command_panel(window_t *win, uint32_t msg,
                           uint32_t wparam, void *lparam) {
  cp_state_t *st = (cp_state_t *)win->userdata;

  switch (msg) {
    case evCreate: {
      st = calloc(1, sizeof(cp_state_t));
      win->userdata = st;
      st->active_tab = 0;

      int sh = ui_get_system_metrics(kSystemMetricScreenHeight);
      int content_h = sh - MENUBAR_HEIGHT - TOOLBAR_BAND_HEIGHT - CP_TAB_HEIGHT - 40;

      st->create_content = create_window(
          "Create", WINDOW_NOTITLE | WINDOW_NOFILL,
          MAKERECT(0, CP_TAB_HEIGHT, CP_WIDTH, content_h),
          win, win_cp_create, 0, NULL);
      show_window(st->create_content, true);

      st->modify_content = create_window(
          "Modify", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_HIDDEN,
          MAKERECT(0, CP_TAB_HEIGHT, CP_WIDTH, content_h),
          win, win_cp_modify, 0, NULL);

      return true;
    }

    case evPaint: {
      if (!st) return false;
      irect16_t cr = get_client_rect(win);
      fill_rect(get_sys_color(brButtonBg), cr);
      cp_draw_tabs(win, st);
      return true;
    }

    case evLeftButtonDown: {
      if (!st) return false;
      int mx = (int16_t)LOWORD(wparam);
      int my = (int16_t)HIWORD(wparam);
      if (my < CP_TAB_HEIGHT) {
        irect16_t cr = get_client_rect(win);
        int tab_w = cr.w / CP_TAB_COUNT;
        int tab = mx / tab_w;
        if (tab >= 0 && tab < CP_TAB_COUNT) {
          cp_switch_tab(win, st, tab);
        }
      }
      return true;
    }

    case evCommand: {
      uint16_t id = LOWORD(wparam);
      if (id >= ID_CP_TAB_CREATE && id <= ID_CP_TAB_UTILITIES) {
        cp_switch_tab(win, st, id - ID_CP_TAB_CREATE);
        return true;
      }
      return false;
    }

    case evDestroy:
      if (st) {
        free(st);
        win->userdata = NULL;
      }
      if (g_app) g_app->command_panel_win = NULL;
      return false;

    default:
      return false;
  }
}

window_t *create_command_panel_window(void) {
  if (!g_app) return NULL;
  int sw = ui_get_system_metrics(kSystemMetricScreenWidth);
  int sh = ui_get_system_metrics(kSystemMetricScreenHeight);
  int win_h = sh - MENUBAR_HEIGHT - TOOLBAR_BAND_HEIGHT - 40;
  window_t *win = create_window(
      "Command Panel",
      WINDOW_ALWAYSONTOP | WINDOW_NOTRAYBUTTON | WINDOW_NORESIZE,
      MAKERECT(sw - CP_WIDTH, MENUBAR_HEIGHT + TOOLBAR_BAND_HEIGHT,
               CP_WIDTH, win_h),
      NULL, win_command_panel, g_app->hinstance, NULL);
  if (win) show_window(win, true);
  return win;
}
