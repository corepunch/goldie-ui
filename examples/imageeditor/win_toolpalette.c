// Tool palette window for the image editor.
//
// The palette root is an auto-layout stack:
//   - a flow container of icon buttons that wraps to two columns
//   - a FG/BG swatch child below it
//
// The buttons are real toolbar-button children managed by the flow container.
// The swatch row is a separate child that the stack positions automatically
// after the flow container.

#include "imageeditor.h"
#include "../../commctl/commctl.h"
#include "../../kernel/renderer.h"
#include "../../user/image.h"

// Image editor atlas tile size (all icons are square).
#define ICON_W  TOOL_ICON_W

typedef struct {
  window_t      *flow_win;
} palette_state_t;

static bitmap_strip_t g_tool_strip = {0};
static bool g_tool_strip_loaded = false;

// Tool palette layout with ident, icon index, and fallback text.
static const toolbox_item_t k_tools[NUM_TOOLS] = {
  { ID_TOOL_SELECT,        IE_RECTANGULAR_MARQUEE,           "" },
  { ID_TOOL_MOVE,          IE_MOVE,                          "" },
  { ID_TOOL_MAGIC_WAND,    IE_MAGIC_WAND,                    "" },
  { ID_TOOL_CROP,          IE_CROP,                          "" },
  { ID_TOOL_HAND,          IE_HAND,                          "" },
  { ID_TOOL_EYEDROPPER,    IE_EYEDROPPER,                    "" },
  { ID_TOOL_ZOOM,          IE_ZOOM_IN,                       "" },
  { ID_TOOL_PENCIL,        IE_PENCIL,                        "" },
  { ID_TOOL_BRUSH,         IE_BRUSH,                         "" },
  { ID_TOOL_SPRAY,         IE_SPRAY_CAN,                     "" },
  { ID_TOOL_FILL,          IE_PAINT_BUCKET,                  "" },
  { ID_TOOL_ERASER,        IE_ERASER,                        "" },
  { ID_TOOL_LINE,          IE_LINE,                          "" },
  { ID_TOOL_TEXT,          IE_TEXT,                          "" },
  { ID_TOOL_RECT,          IE_RECTANGLE,                     "" },
  { ID_TOOL_ELLIPSE,       IE_ELLIPSE,                       "" },
  { ID_TOOL_ROUNDED_RECT,  IE_ROUNDED_RECTANGLE,             "" },
  { ID_TOOL_POLYGON,       IE_POLYGON_LASSO,                 "" },
  { ID_TOOL_MAGNIFIER,     IE_ZOOM_OUT,                      "" },
};

static palette_state_t *palette_state(window_t *win) {
  return win ? (palette_state_t *)win->userdata : NULL;
}

static window_t *palette_flow(window_t *win) {
  palette_state_t *st = palette_state(win);
  return st ? st->flow_win : NULL;
}

static void palette_set_active_tool(window_t *win, int ident) {
  window_t *flow = palette_flow(win);
  if (!flow) return;
  for (window_t *child = flow->children; child; child = child->next) {
    send_message(child, btnSetCheck,
                 (child->id == (uint32_t)ident) ? btnStateChecked : btnStateUnchecked,
                 NULL);
  }
}

static bool palette_load_strip(window_t *win) {
  (void)win;
  if (g_tool_strip_loaded)
    return true;
  memset(&g_tool_strip, 0, sizeof(g_tool_strip));

#ifdef SHAREDIR
  char icon_path[512];
  int n = snprintf(icon_path, sizeof(icon_path), "%s/" SHAREDIR "/image-editor.png",
                   ui_get_exe_dir());
  if (n <= 0 || (size_t)n >= sizeof(icon_path))
    return false;

  int w = 0, h = 0;
  uint8_t *pixels = load_image(icon_path, &w, &h);
  if (!pixels)
    return false;
  if (w < ICON_W || h < ICON_W || (w % ICON_W) != 0 || (h % ICON_W) != 0) {
    image_free(pixels);
    return false;
  }

  uint32_t tex = R_CreateTextureRGBA(w, h, pixels, R_FILTER_NEAREST, R_WRAP_CLAMP);
  image_free(pixels);
  if (!tex)
    return false;

  g_tool_strip.tex = tex;
  g_tool_strip.icon_w = ICON_W;
  g_tool_strip.icon_h = ICON_W;
  g_tool_strip.cols = w / ICON_W;
  g_tool_strip.sheet_w = w;
  g_tool_strip.sheet_h = h;
  g_tool_strip_loaded = true;
  return true;
#else
  return false;
#endif
}

static void palette_assign_strip_to_buttons(window_t *flow, palette_state_t *st) {
  if (!flow || !st || !g_tool_strip_loaded)
    return;
  for (window_t *child = flow->children; child; child = child->next) {
    for (int i = 0; i < NUM_TOOLS; i++) {
      if ((uint32_t)k_tools[i].ident == child->id) {
        send_message(child, btnSetImage, (uint32_t)k_tools[i].icon, &g_tool_strip);
        break;
      }
    }
  }
}

static result_t palette_swatch_proc(window_t *win, uint32_t msg,
                                    uint32_t wparam, void *lparam) {
  (void)wparam;
  switch (msg) {
    case evMeasure: {
      layout_measure_t *m = (layout_measure_t *)lparam;
      if (m) {
        m->desired_w = PALETTE_WIN_W;
        m->desired_h = SWATCH_CLIENT_H;
      }
      return true;
    }
    case evPaint: {
      irect16_t inner_box = rect_inset(get_client_rect(win), 2);
      int chip_side = inner_box.w - inner_box.w / 3;
      int reset_side = inner_box.w / 3;
      irect16_t fg_outer = rect_split_left(rect_split_top(inner_box, chip_side), chip_side);
      irect16_t bg_outer = rect_split_right(rect_split_bottom(inner_box, chip_side), chip_side);
      irect16_t reset_outer = rect_split_left(rect_split_bottom(inner_box, reset_side), reset_side);
      irect16_t bg_inner = rect_inset(bg_outer, 1);
      irect16_t fg_inner = rect_inset(fg_outer, 1);
      irect16_t reset_inner = rect_inset(reset_outer, 1);
      irect16_t reset_black = rect_inset(rect_offset(reset_inner, 1, 1), 1);

      fill_rect(get_sys_color(brDarkEdge), bg_outer);
      fill_rect(g_app ? g_app->bg_color : 0xFF000000, bg_inner);

      fill_rect(get_sys_color(brDarkEdge), fg_outer);
      fill_rect(g_app ? g_app->fg_color : 0xFFFFFFFF, fg_inner);

      fill_rect(get_sys_color(brDarkEdge), reset_outer);
      fill_rect(0xFFFFFFFF, reset_inner);
      fill_rect(0xFF000000, reset_black);
      return true;
    }
    default:
      return false;
  }
}

static result_t palette_root_proc(window_t *win, uint32_t msg,
                                  uint32_t wparam, void *lparam) {
  palette_state_t *st = palette_state(win);
  switch (msg) {
    case evCreate: {
      st = allocate_window_data(win, sizeof(palette_state_t));
      if (!st) return false;

      win->auto_layout = true;
      win->layout_kind = "stack";
      win->layout_spacing = 4;
      win->layout_padding = (irect16_t){0, 0, 0, 0};
      win->layout_margin = (irect16_t){0, 0, 0, 0};
      win->layout_orientation = 0;

      layout_view_config_t flow_cfg = {
        .layout_kind = "flow",
        .orientation = 0,
        .spacing = 0,
        .padding = (irect16_t){0, 0, 0, 0},
        .margin = (irect16_t){0, 0, 0, 0},
      };
      irect16_t flow_frame = {0, 0, PALETTE_WIN_W, 0};

      window_t *flow = create_window(
          "",
          WINDOW_NOTITLE | WINDOW_NOFILL,
          &flow_frame,
          win, "flowview", win->hinstance, &flow_cfg);
      if (!flow) return false;
      st->flow_win = flow;

      for (int i = 0; i < NUM_TOOLS; i++) {
        irect16_t btn_frame = {0, 0, TOOL_PALETTE_BTN_SIZE, TOOL_PALETTE_BTN_SIZE};
        window_t *btn = create_window(
            "",
            WINDOW_NOTITLE | WINDOW_NOFILL | BUTTON_PUSHLIKE,
            &btn_frame,
            flow, win_toolbar_button, win->hinstance, NULL);
        if (!btn) return false;
        btn->id = k_tools[i].ident;
      }

      palette_load_strip(win);
      palette_assign_strip_to_buttons(flow, st);

      irect16_t swatch_frame = {0, 0, PALETTE_WIN_W, SWATCH_CLIENT_H};
      window_t *swatch = create_window(
          "",
          WINDOW_NOTITLE | WINDOW_NOFILL,
          &swatch_frame,
          win, palette_swatch_proc, win->hinstance, NULL);
      if (!swatch) return false;

      palette_set_active_tool(win, ID_TOOL_SELECT);
      window_layout_sync(win);
      return true;
    }

    case bxSetActiveItem:
      palette_set_active_tool(win, (int)(uint32_t)wparam);
      return true;

    case tbButtonClick:
      if (g_app)
        handle_menu_command((uint16_t)wparam);
      return true;

    case evResize:
      window_layout_sync(win);
      return true;

    case evDestroy:
      if (g_app && g_app->tool_win == win)
        g_app->tool_win = NULL;
      return false;

    default:
      return false;
  }
}

result_t win_tool_palette_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam) {
  return palette_root_proc(win, msg, wparam, lparam);
}
