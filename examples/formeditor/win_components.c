// Auto-layout tool palette for the form editor.
// The top-level "Components" window is a wrapper; the actual icon grid lives
// in a child icongrid control so scrolling and hit-testing stay inside the
// grid view, the same way Filter Gallery does in ImageEditor.

#include "formeditor.h"
#include "../../commctl/commctl.h"
#include "../../commctl/columnview.h"
#include "../../kernel/renderer.h"
#include "../../user/draw.h"
#include "../../user/image.h"
#include "../../user/icons.h"

#define FE_TOOL_ICON_SIZE FE_COMPONENTS_ICON_W
#define FE_DRAG_THRESHOLD 2

static reportview_item_t g_comp_tools[FE_MAX_COMPONENTS + 1];
static int g_comp_tool_count = 0;
#ifdef SHAREDIR
static bitmap_strip_t g_tool_strip = {0};
static bool g_tool_strip_loaded = false;
#endif

typedef struct {
  window_t *list_win;
} components_palette_state_t;

typedef struct {
  window_t *win;
  int tool_ident;
  int icon;
  char text[64];
} palette_drag_ghost_t;

typedef struct {
  bool pending;
  bool dragging;
  int tool_ident;
  ipoint16_t start_local;
} palette_drag_state_t;

#ifdef SHAREDIR
static palette_drag_ghost_t g_ghost = {0};
#endif
static palette_drag_state_t g_drag = {0};

static int components_win_y(void) {
  return MENUBAR_HEIGHT + 4;
}

static int components_item_count(void) {
  int items = 0;
  for (int i = 0; i < fe_component_count(); i++) {
    const fe_component_desc_t *c = fe_component_at(i);
    if (!c) continue;
    if ((c->capabilities & (FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX)) ==
        (FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX))
      items++;
  }
  return items;
}

static int components_win_h(void) {
  int rows = (components_item_count() + FE_COMPONENTS_GRID_COLS - 1) / FE_COMPONENTS_GRID_COLS;
  if (rows < FE_COMPONENTS_MIN_ROWS) rows = FE_COMPONENTS_MIN_ROWS;
  return TITLEBAR_HEIGHT + rows * FE_COMPONENTS_BTN_SIZE + 4;
}

#ifndef SHAREDIR
static void components_hide_ghost(void) {}
static void components_update_ghost(int ident, int sx, int sy) {
  (void)ident;
  (void)sx;
  (void)sy;
}
static void components_load_strip(void) {}
#else
#if FE_DEFAULT_EDIT_MODE == FE_EDIT_MODE_AUTO_LAYOUT
static const reportview_item_t *components_item_by_ident(int ident) {
  for (int i = 0; i < g_comp_tool_count; i++) {
    if ((int)g_comp_tools[i].userdata == ident)
      return &g_comp_tools[i];
  }
  return NULL;
}
#endif

static void components_hide_ghost(void) {
#if FE_DEFAULT_EDIT_MODE == FE_EDIT_MODE_AUTO_LAYOUT
  if (g_ghost.win && window_has_state(g_ghost.win, WINDOW_STATE_VISIBLE))
    show_window(g_ghost.win, false);
#endif
}

#if FE_DEFAULT_EDIT_MODE == FE_EDIT_MODE_AUTO_LAYOUT
static result_t components_drag_ghost_proc(window_t *win, uint32_t msg,
                                           uint32_t wparam, void *lparam) {
  (void)wparam;
  (void)lparam;
  switch (msg) {
    case evCreate:
      win->flags |= WINDOW_NOTABSTOP;
      return true;
    case evPaint: {
      int w = win->frame.w;
      int h = win->frame.h;
      fill_rect(0x663A7DFF, R(0, 0, w, h));
      fill_rect(0xCC3A7DFF, R(0, 0, w, 1));
      fill_rect(0xCC3A7DFF, R(0, h - 1, w, 1));
      fill_rect(0xCC3A7DFF, R(0, 0, 1, h));
      fill_rect(0xCC3A7DFF, R(w - 1, 0, 1, h));

      int px = 4;
      int py = (h - FE_TOOL_ICON_SIZE) / 2;
      if (g_ghost.icon >= SYSICON_BASE) {
        draw_icon16(g_ghost.icon, px, py, 0xFFFFFFFF);
      } else if (g_tool_strip_loaded && g_tool_strip.tex && g_tool_strip.cols > 0) {
        int col_idx = g_ghost.icon % g_tool_strip.cols;
        int row_idx = g_ghost.icon / g_tool_strip.cols;
        float u0 = (float)(col_idx * g_tool_strip.icon_w) / (float)g_tool_strip.sheet_w;
        float v0 = (float)(row_idx * g_tool_strip.icon_h) / (float)g_tool_strip.sheet_h;
        float u1 = u0 + (float)g_tool_strip.icon_w / (float)g_tool_strip.sheet_w;
        float v1 = v0 + (float)g_tool_strip.icon_h / (float)g_tool_strip.sheet_h;
        draw_sprite_region((int)g_tool_strip.tex,
                           R(px, py, g_tool_strip.icon_w, g_tool_strip.icon_h),
                           UV_RECT(u0, v0, u1, v1), 0xFFFFFFFF, 0);
      }
      draw_text(FONT_SMALL, g_ghost.text,
                px + FE_TOOL_ICON_SIZE + 6,
                (h - text_char_height(FONT_SMALL)) / 2,
                0xFFFFFFFF);
      return true;
    }
    default:
      return false;
  }
}
#endif

static void components_update_ghost(int ident, int sx, int sy) {
#if FE_DEFAULT_EDIT_MODE == FE_EDIT_MODE_AUTO_LAYOUT
  const reportview_item_t *item = components_item_by_ident(ident);
  if (!item)
    return;

  if (!g_ghost.win) {
    g_ghost.win = create_window("",
        WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_ALWAYSONTOP |
        WINDOW_NOTRAYBUTTON | WINDOW_NOFILL | WINDOW_NOACTIVATE |
        WINDOW_TRANSPARENT,
        MAKERECT(0, 0, 10, 10),
        NULL, components_drag_ghost_proc, g_app ? g_app->hinstance : 0, NULL);
    if (!g_ghost.win)
      return;
    show_window(g_ghost.win, false);
  }

  g_ghost.tool_ident = ident;
  g_ghost.icon = item->icon;
  snprintf(g_ghost.text, sizeof(g_ghost.text), "%s", item->text ? item->text : "");

  int text_w = text_strwidth(FONT_SMALL, g_ghost.text);
  int w = FE_TOOL_ICON_SIZE + 14 + text_w;
  int h = MAX(FE_TOOL_ICON_SIZE + 8, FONT_SIZE_SMALL + 10);
  if (w < 48) w = 48;
  if (h < 24) h = 24;
  move_window(g_ghost.win, sx - w / 2, sy - h / 2);
  resize_window(g_ghost.win, w, h);
  show_window(g_ghost.win, true);
  invalidate_window(g_ghost.win);
#else
  (void)ident;
  (void)sx;
  (void)sy;
#endif
}

static void components_load_strip(void) {
  if (g_tool_strip_loaded)
    return;
  char icon_path[512];
  int n = snprintf(icon_path, sizeof(icon_path), "%s/" SHAREDIR "/controls-icons-48.png",
                   ui_get_exe_dir());
  if (n <= 0 || (size_t)n >= sizeof(icon_path))
    return;

  int w = 0;
  int h = 0;
  uint8_t *pixels = load_image(icon_path, &w, &h);
  if (!pixels)
    return;
  if (w < FE_TOOL_ICON_SIZE || h < FE_TOOL_ICON_SIZE ||
      (w % FE_TOOL_ICON_SIZE) != 0 || (h % FE_TOOL_ICON_SIZE) != 0) {
    image_free(pixels);
    return;
  }

  uint32_t tex = R_CreateTextureRGBA(w, h, pixels, R_FILTER_NEAREST, R_WRAP_CLAMP);
  image_free(pixels);
  if (!tex)
    return;

  g_tool_strip.tex = tex;
  g_tool_strip.icon_w = FE_TOOL_ICON_SIZE;
  g_tool_strip.icon_h = FE_TOOL_ICON_SIZE;
  g_tool_strip.cols = w / FE_TOOL_ICON_SIZE;
  g_tool_strip.sheet_w = w;
  g_tool_strip.sheet_h = h;
  g_tool_strip_loaded = true;
}
#endif

static void comp_build_tool_items(void) {
  g_comp_tool_count = 0;
  for (int i = 0; i < fe_component_count() && g_comp_tool_count < FE_MAX_COMPONENTS + 1; i++) {
    const fe_component_desc_t *c = fe_component_at(i);
    if (!c) continue;
    if ((c->capabilities & (FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX)) !=
        (FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX))
      continue;
    g_comp_tools[g_comp_tool_count++] = (reportview_item_t){
        .text = c->class_name,
        .icon = c->toolbox_icon,
        .color = get_sys_color(brTextNormal),
        .userdata = (uint32_t)i,
    };
  }
}

static void comp_select_tool_by_ident(window_t *win, int ident) {
  if (g_app) {
    g_app->current_tool = ident;
    if (g_app->doc && g_app->doc->canvas_win)
      invalidate_window(g_app->doc->canvas_win);
    if (g_app->menubar_win)
      send_message(g_app->menubar_win, evCommand,
                   MAKEDWORD((uint16_t)ident, btnClicked),
                   win);
    else
      handle_menu_command((uint16_t)ident);
  }
}

static void populate_tool_list(window_t *win) {
  if (!win)
    return;
  comp_build_tool_items();
  send_message(win, RVM_SETREDRAW, 0, NULL);
  send_message(win, RVM_CLEAR, 0, NULL);
  for (int i = 0; i < g_comp_tool_count; i++)
    send_message(win, RVM_ADDITEM, 0, &g_comp_tools[i]);
  send_message(win, RVM_SETREDRAW, 1, NULL);

  int current = g_app ? g_app->current_tool : ID_TOOL_SELECT;
  for (int i = 0; i < g_comp_tool_count; i++) {
    if ((int)g_comp_tools[i].userdata == current) {
      send_message(win, RVM_SETSELECTION, (uint32_t)i, NULL);
      break;
    }
  }
}

static int components_tool_ident_at(window_t *win, uint32_t wparam) {
  int idx = (int)send_message(win, RVM_HITTEST, wparam, NULL);
  if (idx < 0)
    return -1;
  reportview_item_t item = {0};
  if (!send_message(win, RVM_GETITEMDATA, (uint32_t)idx, &item))
    return -1;
  return (int)item.userdata;
}

static void components_palette_sync_list(window_t *win) {
  components_palette_state_t *st = win ? (components_palette_state_t *)win->userdata : NULL;
  if (!st || !st->list_win)
    return;
  // Let the large-icon view recalculate the grid from its current width so the
  // components palette reflows when the window is resized.
  send_message(st->list_win, RVM_SETLARGEICONCOLS, 0, NULL);
  send_message(st->list_win, RVM_SETCOLUMNWIDTH, FE_COMPONENTS_BTN_SIZE, NULL);
  send_message(st->list_win, RVM_SETICONSIZE, FE_TOOL_ICON_SIZE, NULL);
  components_load_strip();
#ifdef SHAREDIR
  send_message(st->list_win, RVM_SETICONSTRIP, 0, g_tool_strip_loaded ? &g_tool_strip : NULL);
#else
  send_message(st->list_win, RVM_SETICONSTRIP, 0, NULL);
#endif
  send_message(st->list_win, RVM_SETPRESERVEICONCOLORS, 1, NULL);
  send_message(st->list_win, RVM_SETCOLUMNTITLESVISIBLE, 0, NULL);
  populate_tool_list(st->list_win);
}

static ipoint16_t window_local_point_to_screen(window_t *win, int lx, int ly) {
  if (!win)
    return (ipoint16_t){0, 0};
  return (ipoint16_t){
      (int16_t)(window_screen_x(win) + lx - win->hscroll.pos),
      (int16_t)(window_screen_y(win) + ly - win->vscroll.pos),
  };
}

window_t *formeditor_create_components_palette(hinstance_t hinstance) {
  window_t *tp = create_window(
      "Components",
      WINDOW_ALWAYSONTOP | WINDOW_NOTRAYBUTTON,
      MAKERECT(PALETTE_WIN_X, components_win_y(), PALETTE_WIN_W, components_win_h()),
      NULL, win_components_proc, hinstance, NULL);
  if (tp) show_window(tp, true);
  return tp;
}

void formeditor_rebuild_tool_palette(void) {
  if (!g_app) return;
  if (g_app->tool_win) {
    destroy_window(g_app->tool_win);
    g_app->tool_win = NULL;
  }
  g_app->current_tool = ID_TOOL_SELECT;
  g_drag = (palette_drag_state_t){0};
  components_hide_ghost();
#if FE_DEFAULT_EDIT_MODE == FE_EDIT_MODE_AUTO_LAYOUT
  g_app->tool_win = formeditor_create_components_palette(g_app->hinstance);
#else
  g_app->tool_win = formeditor_create_legacy_toolpalette(g_app->hinstance);
#endif
}

result_t win_components_proc(window_t *win, uint32_t msg,
                              uint32_t wparam, void *lparam) {
  components_palette_state_t *st = (components_palette_state_t *)win->userdata;
  switch (msg) {
    case evCreate:
      st = allocate_window_data(win, sizeof(components_palette_state_t));
      if (!st)
        return false;
      win->userdata = st;
      {
        irect16_t cr = get_client_rect(win);
        st->list_win = create_window(
            "", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_NORESIZE | WINDOW_VSCROLL,
            MAKERECT(0, 0, cr.w, cr.h),
            win, win_icongrid, win->hinstance, NULL);
        if (!st->list_win)
          return false;
      }
      components_palette_sync_list(win);
      return true;

    case evResize:
      if (st && st->list_win) {
        irect16_t cr = get_client_rect(win);
        resize_window(st->list_win, cr.w, cr.h);
      }
      return true;

    case evDestroy:
      if (g_app && g_app->tool_win == win)
        g_app->tool_win = NULL;
      return false;

    case evCommand:
      if ((lparam == st->list_win) &&
          (HIWORD(wparam) == RVN_SELCHANGE || HIWORD(wparam) == RVN_DBLCLK)) {
        reportview_item_t item = {0};
        if (send_message(st->list_win, RVM_GETITEMDATA, LOWORD(wparam), &item)) {
          comp_select_tool_by_ident(win, (int)item.userdata);
          return true;
        }
      }
      return false;

    case evParentNotify: {
      if (!st || !st->list_win || !lparam)
        return false;
      parent_notify_t *pn = (parent_notify_t *)lparam;
      if (pn->child != st->list_win)
        return false;

      switch (pn->child_msg) {
        case evLeftButtonDown: {
          int ident = components_tool_ident_at(st->list_win, pn->child_wparam);
          if (ident < 0)
            return false;
          g_drag = (palette_drag_state_t){
            .pending = true,
            .dragging = false,
            .tool_ident = ident,
            .start_local = {(int16_t)LOWORD(pn->child_wparam), (int16_t)HIWORD(pn->child_wparam)},
          };
          if (g_app)
            g_app->current_tool = ident;
          set_capture(st->list_win);
          return false;
        }
        case evMouseMove:
          if (!g_drag.pending)
            return false;
          if (!g_drag.dragging) {
            int lx = (int16_t)LOWORD(pn->child_wparam);
            int ly = (int16_t)HIWORD(pn->child_wparam);
            int dx = lx - g_drag.start_local.x;
            int dy = ly - g_drag.start_local.y;
            if ((dx < 0 ? -dx : dx) < FE_DRAG_THRESHOLD &&
                (dy < 0 ? -dy : dy) < FE_DRAG_THRESHOLD)
              return false;
            g_drag.dragging = true;
          }
          {
            int lx = (int16_t)LOWORD(pn->child_wparam);
            int ly = (int16_t)HIWORD(pn->child_wparam);
            ipoint16_t screen = window_local_point_to_screen(st->list_win, lx, ly);
            window_t *target = canvas_find_component_drop_target(g_app ? g_app->doc : NULL,
                                                                 g_drag.tool_ident,
                                                                 screen.x, screen.y);
            if (g_app && g_app->doc) {
              canvas_set_component_drag_hover(g_app->doc, target != NULL, target);
            }
            components_update_ghost(g_drag.tool_ident, screen.x, screen.y);
          }
          return false;
        case evLeftButtonUp:
          if (!g_drag.pending)
            return false;
          if (g_drag.dragging) {
            int lx = (int16_t)LOWORD(pn->child_wparam);
            int ly = (int16_t)HIWORD(pn->child_wparam);
            ipoint16_t screen = window_local_point_to_screen(st->list_win, lx, ly);
            int sx = screen.x;
            int sy = screen.y;
            window_t *target = canvas_find_component_drop_target(g_app ? g_app->doc : NULL,
                                                                 g_drag.tool_ident,
                                                                 sx, sy);
            if (g_app && g_app->doc && target)
              canvas_drop_component_to_target(g_app->doc, g_drag.tool_ident, target, sx, sy);
            if (g_app && g_app->doc)
              canvas_set_component_drag_hover(g_app->doc, false, NULL);
            if (g_app) {
              g_app->current_tool = ID_TOOL_SELECT;
              if (g_app->tool_win)
                send_message(g_app->tool_win, bxSetActiveItem, (uint32_t)ID_TOOL_SELECT, NULL);
            }
          }
          components_hide_ghost();
          g_drag = (palette_drag_state_t){0};
          set_capture(NULL);
          return false;
        default:
          return false;
      }
    }

    default:
      return false;
  }
}
