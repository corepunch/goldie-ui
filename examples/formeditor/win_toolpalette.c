// Tool palette for the form editor.
// Uses an icon grid (reportview large-icon mode) and supports drag/drop
// placement onto the active form canvas.

#include "formeditor.h"
#include "../../commctl/commctl.h"
#include "../../commctl/columnview.h"
#include "../../user/icons.h"

#define FE_TOOL_GRID_COLS 4
#define FE_TOOL_ICON_SIZE 16
#define FE_DRAG_THRESHOLD 2

static reportview_item_t g_tools[FE_MAX_COMPONENTS + 1];
static int g_tool_count = 0;

typedef struct {
  bool pending;
  bool dragging;
  bool canvas_dragging;
  int tool_ident;
  ipoint16_t start_local;
} palette_drag_state_t;

static palette_drag_state_t g_drag = {0};

static int palette_win_y(void) {
  return MENUBAR_HEIGHT + 4;
}

static int palette_item_count(void) {
  int items = 1;  // Select tool
  for (int i = 0; i < fe_component_count(); i++) {
    const fe_component_desc_t *c = fe_component_at(i);
    if (!c) continue;
    if ((c->capabilities & (FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX)) ==
        (FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX))
      items++;
  }
  return items;
}

static int palette_win_h(void) {
  int rows = (palette_item_count() + FE_TOOL_GRID_COLS - 1) / FE_TOOL_GRID_COLS;
  if (rows < 2) rows = 2;
  return TITLEBAR_HEIGHT + rows * FE_TOOLBOX_BTN_SIZE + 4;
}

static window_t *create_tool_palette(hinstance_t hinstance) {
  window_t *tp = create_window(
      "Components",
      WINDOW_ALWAYSONTOP | WINDOW_NOTRAYBUTTON | WINDOW_NORESIZE,
      MAKERECT(PALETTE_WIN_X, palette_win_y(), PALETTE_WIN_W, palette_win_h()),
      NULL, win_tool_palette_proc, hinstance, NULL);
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
  g_app->tool_win = create_tool_palette(g_app->hinstance);
}

static void build_tool_items(void) {
  g_tool_count = 0;
  g_tools[g_tool_count++] = (reportview_item_t){
      .text = "Select",
      .icon = sysicon_cursor,
      .color = get_sys_color(brTextNormal),
      .userdata = ID_TOOL_SELECT,
  };
  for (int i = 0; i < fe_component_count() && g_tool_count < FE_MAX_COMPONENTS + 1; i++) {
    const fe_component_desc_t *c = fe_component_at(i);
    if (!c) continue;
    if ((c->capabilities & (FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX)) !=
        (FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX))
      continue;
    g_tools[g_tool_count++] = (reportview_item_t){
        .text = c->display_name,
        .icon = c->toolbox_icon,
        .color = get_sys_color(brTextNormal),
        .userdata = (uint32_t)c->toolbox_ident,
    };
  }
}

static void select_tool_by_ident(window_t *win, int ident) {
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
  build_tool_items();
  send_message(win, RVM_SETREDRAW, 0, NULL);
  send_message(win, RVM_CLEAR, 0, NULL);
  for (int i = 0; i < g_tool_count; i++)
    send_message(win, RVM_ADDITEM, 0, &g_tools[i]);
  send_message(win, RVM_SETREDRAW, 1, NULL);

  int current = g_app ? g_app->current_tool : ID_TOOL_SELECT;
  for (int i = 0; i < g_tool_count; i++) {
    if ((int)g_tools[i].userdata == current) {
      send_message(win, RVM_SETSELECTION, (uint32_t)i, NULL);
      break;
    }
  }
}

static int palette_selected_tool_ident(window_t *win) {
  int idx = (int)send_message(win, RVM_GETSELECTION, 0, NULL);
  if (idx < 0) return -1;
  reportview_item_t item = {0};
  if (!send_message(win, RVM_GETITEMDATA, (uint32_t)idx, &item))
    return -1;
  return (int)item.userdata;
}

static ipoint16_t window_client_origin(window_t *win) {
  int x = 0;
  int y = 0;
  if (!win) return (ipoint16_t){0, 0};
  for (window_t *it = win; it; it = it->parent) {
    x += it->frame.x;
    y += it->frame.y;
    if (!it->parent) {
      y += titlebar_height(it);
      break;
    }
  }
  return (ipoint16_t){(int16_t)x, (int16_t)y};
}

static bool point_in_window_client(window_t *win, int sx, int sy) {
  if (!win) return false;
  irect16_t cr = get_client_rect(win);
  ipoint16_t o = window_client_origin(win);
  int lx = sx - o.x;
  int ly = sy - o.y;
  return lx >= cr.x && ly >= cr.y && lx < cr.x + cr.w && ly < cr.y + cr.h;
}

static uint32_t window_point_to_local_wparam(window_t *win, int sx, int sy) {
  ipoint16_t o = window_client_origin(win);
  int lx = sx - o.x + win->scroll[0];
  int ly = sy - o.y + win->scroll[1];
  return MAKEDWORD((uint16_t)(int16_t)lx, (uint16_t)(int16_t)ly);
}

static void palette_drag_forward_to_canvas(int sx, int sy, uint32_t msg) {
  if (!g_app || !g_app->doc || !g_app->doc->canvas_win || g_drag.tool_ident < 0)
    return;
  window_t *canvas = g_app->doc->canvas_win;

  if (!g_drag.canvas_dragging) {
    if (!point_in_window_client(canvas, sx, sy))
      return;
    g_drag.canvas_dragging = true;
    g_app->current_tool = g_drag.tool_ident;
    send_message(canvas, evLeftButtonDown,
                 window_point_to_local_wparam(canvas, sx, sy), NULL);
    return;
  }

  send_message(canvas, msg, window_point_to_local_wparam(canvas, sx, sy), NULL);
}

result_t win_tool_palette_proc(window_t *win, uint32_t msg,
                                uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      win_reportview(win, msg, wparam, lparam);
      send_message(win, RVM_SETVIEWMODE, RVM_VIEW_LARGE_ICON, NULL);
      send_message(win, RVM_SETCOLUMNWIDTH, FE_TOOLBOX_BTN_SIZE, NULL);
      send_message(win, RVM_SETICONSIZE, FE_TOOL_ICON_SIZE, NULL);
      send_message(win, RVM_SETPRESERVEICONCOLORS, 1, NULL);
      send_message(win, RVM_SETCOLUMNTITLESVISIBLE, 0, NULL);
      populate_tool_list(win);
      return true;

    case evCommand:
      if ((lparam == win) &&
          (HIWORD(wparam) == RVN_SELCHANGE || HIWORD(wparam) == RVN_DBLCLK)) {
        reportview_item_t item = {0};
        if (send_message(win, RVM_GETITEMDATA, LOWORD(wparam), &item))
          select_tool_by_ident(win, (int)item.userdata);
        return true;
      }
      return false;

    case evLeftButtonDown: {
      win_reportview(win, msg, wparam, lparam);
      int ident = palette_selected_tool_ident(win);
      if (ident < 0)
        return true;
      g_drag = (palette_drag_state_t){
        .pending = true,
        .dragging = false,
        .canvas_dragging = false,
        .tool_ident = ident,
        .start_local = {(int16_t)LOWORD(wparam), (int16_t)HIWORD(wparam)},
      };
      set_capture(win);
      return true;
    }

    case evMouseMove:
      if (!g_drag.pending)
        return win_reportview(win, msg, wparam, lparam);
      if (!g_drag.dragging) {
        int lx = (int16_t)LOWORD(wparam);
        int ly = (int16_t)HIWORD(wparam);
        int dx = lx - g_drag.start_local.x;
        int dy = ly - g_drag.start_local.y;
        if ((dx < 0 ? -dx : dx) < FE_DRAG_THRESHOLD &&
            (dy < 0 ? -dy : dy) < FE_DRAG_THRESHOLD)
          return true;
        g_drag.dragging = true;
      }
      {
        ipoint16_t o = window_client_origin(win);
        int sx = o.x + (int16_t)LOWORD(wparam);
        int sy = o.y + (int16_t)HIWORD(wparam);
        palette_drag_forward_to_canvas(sx, sy, evMouseMove);
      }
      return true;

    case evLeftButtonUp: {
      if (!g_drag.pending)
        return win_reportview(win, msg, wparam, lparam);
      ipoint16_t o = window_client_origin(win);
      int sx = o.x + (int16_t)LOWORD(wparam);
      int sy = o.y + (int16_t)HIWORD(wparam);
      if (g_drag.dragging && g_drag.canvas_dragging) {
        palette_drag_forward_to_canvas(sx, sy, evLeftButtonUp);
      } else if (g_drag.tool_ident >= 0) {
        select_tool_by_ident(win, g_drag.tool_ident);
      }
      g_drag = (palette_drag_state_t){0};
      set_capture(NULL);
      return true;
    }

    default:
      return win_reportview(win, msg, wparam, lparam);
  }
}
