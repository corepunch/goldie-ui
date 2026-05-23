// Legacy tool palette for the form editor.
// This is the classic VB-style toolbox: a real toolbox control, not a grid.

#include "formeditor.h"
#include "../../commctl/commctl.h"
#include "../../kernel/renderer.h"
#include "../../user/icons.h"
#include "../../user/image.h"

static toolbox_item_t g_tools[FE_MAX_COMPONENTS + 1];
static int g_tool_count = 0;

static void build_tool_items(void) {
  g_tool_count = 0;
  g_tools[g_tool_count++] = (toolbox_item_t){
      .ident = ID_TOOL_SELECT,
      .icon = sysicon_cursor,
      .tooltip = "Select",
  };

  for (int i = 0; i < fe_component_count() && g_tool_count < FE_MAX_COMPONENTS + 1; i++) {
    const fe_component_desc_t *c = fe_component_at(i);
    if (!c) continue;
    if ((c->capabilities & (FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX)) !=
        (FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX))
      continue;
    g_tools[g_tool_count++] = (toolbox_item_t){
        .ident = i,
        .icon = c->toolbox_icon,
        .tooltip = c->class_name,
    };
  }
}

static void select_tool_by_ident(window_t *win, int ident) {
  if (g_app) {
    g_app->current_tool = ident;
    window_t *doc = g_app->active_form;
    if (doc && doc->children)
      invalidate_window(doc->children);
    if (g_app->windows[FE_WIN_MENUBAR])
      send_message(g_app->windows[FE_WIN_MENUBAR], evCommand,
                   MAKEDWORD((uint16_t)ident, btnClicked),
                   win);
    else
      handle_menu_command((uint16_t)ident);
  }
}

static void populate_toolbox(window_t *win) {
  if (!win)
    return;

  build_tool_items();
  send_message(win, bxSetButtonSize, FE_VB_TOOLBOX_BTN_SIZE, NULL);

#ifdef SHAREDIR
  char icon_path[512];
  int n = snprintf(icon_path, sizeof(icon_path), "%s/" SHAREDIR "/controls-icons.png",
                   ui_get_exe_dir());
  if (n > 0 && (size_t)n < sizeof(icon_path))
    send_message(win, bxLoadStrip, FE_TOOLBOX_ICON_W, icon_path);
#endif

  send_message(win, bxSetIconTintBrush, brTextNormal, NULL);
  send_message(win, bxSetItems, (uint32_t)g_tool_count, g_tools);

  int current = g_app ? g_app->current_tool : ID_TOOL_SELECT;
  send_message(win, bxSetActiveItem, (uint32_t)current, NULL);
}

window_t *formeditor_create_legacy_toolpalette(hinstance_t hinstance) {
  build_tool_items();
  int rows = (g_tool_count + TOOLBOX_COLS - 1) / TOOLBOX_COLS;
  if (rows < 2) rows = 2;
  window_t *tp = create_window(
      "Toolbox",
      WINDOW_NOTRAYBUTTON | WINDOW_NORESIZE,
      MAKERECT(PALETTE_WIN_X, MENUBAR_HEIGHT + 4,
               TOOLBOX_COLS * FE_VB_TOOLBOX_BTN_SIZE,
               TITLEBAR_HEIGHT + rows * FE_VB_TOOLBOX_BTN_SIZE),
      NULL, win_tool_palette_proc, hinstance, NULL);
  if (tp) show_window(tp, true);
  return tp;
}

result_t win_tool_palette_proc(window_t *win, uint32_t msg,
                                uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      if (!win_toolbox(win, msg, wparam, lparam))
        return false;
      populate_toolbox(win);
      return true;

    case evCommand:
      if ((lparam == win) && HIWORD(wparam) == bxClicked) {
        select_tool_by_ident(win, (int)LOWORD(wparam));
        return true;
      }
      return false;

    default:
      return win_toolbox(win, msg, wparam, lparam);
  }
}
