#ifndef __FORMEDITOR_H__
#define __FORMEDITOR_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "../../ui.h"

// ============================================================
// Layout constants
// ============================================================

#define SCREEN_W          800
#define SCREEN_H          600

// Default form dimensions
#define FORM_DEFAULT_W    320
#define FORM_DEFAULT_H    240

// Editing mode:
//   - FE_EDIT_MODE_VB_STYLE     = fixed layout with absolute x/y placement
//   - FE_EDIT_MODE_AUTO_LAYOUT  = layout-managed forms with drop targeting
//
// Override FE_DEFAULT_EDIT_MODE at build time to switch the editor's default
// behaviour for new documents.
#define FE_EDIT_MODE_VB_STYLE     0
#define FE_EDIT_MODE_AUTO_LAYOUT   1
typedef uint8_t fe_edit_mode_t;

#ifndef FE_DEFAULT_EDIT_MODE
#define FE_DEFAULT_EDIT_MODE FE_EDIT_MODE_AUTO_LAYOUT
#endif

static inline bool fe_default_auto_layout_enabled(void) {
  return FE_DEFAULT_EDIT_MODE == FE_EDIT_MODE_AUTO_LAYOUT;
}

// Tool palettes.
// Legacy VB-style toolbox keeps the original 24px strip.
#define FE_TOOLBOX_ICON_W   24   // icon tile size in the legacy shared strip
#define FE_VB_TOOLBOX_BTN_SIZE (FE_TOOLBOX_ICON_W + 6)  // legacy toolbox button size

// The new auto-layout components palette uses the 48px strip.
#define FE_COMPONENTS_ICON_W 48   // icon tile size in the new shared strip
#define FE_COMPONENTS_GRID_COLS 4  // palette wraps after four items per row
#define FE_COMPONENTS_BTN_SIZE 56  // large-icon grid cell width/height
#define FE_COMPONENTS_MIN_ROWS 5   // default palette height shows a little over 4.5 icons

#include "controls-icons.h"

// Palette window dimensions.
#define PALETTE_WIN_X     4
#define PALETTE_WIN_W     197

// Property browser window.  This is intentionally a reportview-backed
// inspector: close to VB1's simple property sheet, without inline editing yet.
#define PROPBROWSER_WIN_X (SCREEN_W - 184)
#define PROPBROWSER_WIN_W 180
#define PROPBROWSER_WIN_H 180

// Project forms browser.
#define FORMS_WIN_X       PROPBROWSER_WIN_X
#define FORMS_WIN_Y       (MENUBAR_HEIGHT + 4)
#define FORMS_WIN_W       PROPBROWSER_WIN_W
#define FORMS_WIN_H       180

#define PROPBROWSER_WIN_Y (FORMS_WIN_Y + FORMS_WIN_H + 4)

// Project plugins browser.
#define PLUGINS_WIN_X     PROPBROWSER_WIN_X
#define PLUGINS_WIN_Y     (PROPBROWSER_WIN_Y + PROPBROWSER_WIN_H + 4)
#define PLUGINS_WIN_W     PROPBROWSER_WIN_W
#define PLUGINS_WIN_H     (SCREEN_H - PLUGINS_WIN_Y - 4)

// Document window initial position
// frame.y is the window top; place it 8px below the menu bar.
#define DOC_START_X       (PALETTE_WIN_X + PALETTE_WIN_W + 10)
#define DOC_START_Y       (MENUBAR_HEIGHT + 8)

// ============================================================
// Menu item IDs
// ============================================================

#define ID_FILE_NEW     1
#define ID_FILE_OPEN    2
#define ID_FILE_SAVE    3
#define ID_FILE_SAVEAS  4
#define ID_FILE_QUIT    5

#define ID_EDIT_DELETE  10
#define ID_EDIT_PROPS   11

#define ID_VIEW_GRID    20

#define ID_HELP_ABOUT   100

// Tool command IDs (VB3 toolbox slot numbers map to strip indices)
// Strip order: 0=Pointer, 1=Picture(skip), 2=Label, 3=TextBox,
//              4=Frame(skip), 5=Button, 6=CheckBox, 7=Option(skip),
//              8=ComboBox, 9=ListBox, ...
#define ID_TOOL_SELECT    200
#define ID_TOOL_LABEL     202
#define ID_TOOL_TEXTEDIT  203
#define ID_TOOL_BUTTON    205
#define ID_TOOL_CHECKBOX  206
#define ID_TOOL_COMBOBOX  208
#define ID_TOOL_LIST      209

// ============================================================
// Limits
// ============================================================

#define MAX_ELEMENTS  256
#define FE_MAX_DOCS   64
#define CTRL_ID_BASE  1001

// Built-in component indices as registered by formeditor_components.
// Kept as compatibility aliases for tests and older form editor code; project
// files should use component tokens/class names instead.
#define CTRL_BUTTON    0
#define CTRL_CHECKBOX  1
#define CTRL_LABEL     2
#define CTRL_TEXTEDIT  3
#define CTRL_LIST      4
#define CTRL_COMBOBOX  5

// ============================================================
// Types
// ============================================================

typedef enum {
  FE_LAYOUT_NONE = 0,
  FE_LAYOUT_STACK = 1,
  FE_LAYOUT_GRID = 2,
} fe_layout_type_t;

typedef struct form_doc_t {
  window_t *elements[MAX_ELEMENTS];
  int    element_count;
  isize16_t form_size;
  uint32_t flags;       // form/window flags exported in form_def_t
  fe_layout_type_t layout_type;
  uint8_t grid_columns; // grid columns (0 = default)
  uint8_t spacing;      // spacing between direct children; 0 = default
  irect16_t padding;    // inner padding for auto-layout content
  irect16_t margin;     // outer margin for the form when serialized
  bool   modified;
  char   form_id[64];
  char   form_title[128];
  char   required_plugin[64];
  int    next_id;                      // next numeric control ID
  int    type_counters[FE_MAX_COMPONENTS]; // per-component name counter
  window_t *canvas_win;
  window_t *doc_win;
  // Grid settings
  int    grid_size;       // dot spacing in form pixels (default 8)
  bool   show_grid;       // paint grid dots on the form surface
  bool   snap_to_grid;    // snap moves/resizes to grid
} form_doc_t;

typedef struct {
  char name[64];
} form_plugin_ref_t;

#define FE_MAX_PROJECT_PLUGINS 32

typedef struct {
  char filename[512];
  char name[64];
  char title[128];
  char root[256];
  char menus_xml[16384];
  form_plugin_ref_t plugins[FE_MAX_PROJECT_PLUGINS];
  int plugin_count;
  bool loaded;
  bool modified;
} form_project_t;

typedef struct {
  form_doc_t  *docs[FE_MAX_DOCS];
  int          doc_count;
  int          active_doc_index;
  window_t    *windows[5];
  hinstance_t  hinstance;  // owning app instance
  int          current_tool;
  accel_table_t *accel;
  form_project_t project;
} app_state_t;

typedef enum {
  FE_WIN_MENUBAR = 0,
  FE_WIN_TOOLBOX = 1,
  FE_WIN_PROPERTIES = 2,
  FE_WIN_FORMS = 3,
  FE_WIN_PLUGINS = 4,
  FE_WIN_COUNT = 5,
} fe_window_role_t;

// ============================================================
// Drag mode for the canvas window
// ============================================================

typedef enum {
  DRAG_NONE,
  DRAG_MOVE,
  DRAG_RESIZE,
  DRAG_RUBBERBND,
} drag_mode_t;

typedef struct {
  int16_t x, y;
} canvas_pt_t;

typedef struct {
  int16_t x, y;
} form_pt_t;

typedef struct {
  drag_mode_t mode;
  union {
    struct {
      canvas_pt_t start;
      irect16_t   frame;
    } move;
    struct {
      canvas_pt_t start;
      irect16_t   frame;
      int         handle;
    } resize;
    struct {
      canvas_pt_t start;
      irect16_t   band;       // rubber-band in form coords
      int         ctrl_type;  // CTRL_* being placed
    } place;
  };
} drag_state_t;

// ============================================================
// Canvas window state (stored in canvas_win->userdata)
// ============================================================

typedef struct {
  form_doc_t *doc;
  window_t   *preview_win;
  int         preview_type;
  ipoint16_t  pan;
  int         selected_idx;   // -1 = no selection
  int         hover_layout_idx; // auto-layout node under placement drag, -1 = form itself
  irect16_t   hover_layout_rc;  // form-space rect for hover highlight
  bool        external_component_drag; // true while toolbox drag hovers the canvas
  drag_state_t drag;
} canvas_state_t;

// ============================================================
// Globals
// ============================================================

extern app_state_t *g_app;
window_t *app_get_window(fe_window_role_t role);
void app_set_window(fe_window_role_t role, window_t *win);
form_doc_t *app_active_doc(void);
int app_doc_count(void);
form_doc_t *app_doc_at(int idx);
int app_doc_index(form_doc_t *doc);
bool app_add_doc(form_doc_t *doc);
void app_remove_doc_at(int idx);
bool app_set_active_doc_index(int idx);
window_t *formeditor_find_window_by_id(window_t *root, uint32_t id);

// ============================================================
// Window procedures
// ============================================================

result_t editor_menubar_proc(window_t *win, uint32_t msg,
                              uint32_t wparam, void *lparam);
result_t win_canvas_proc(window_t *win, uint32_t msg,
                          uint32_t wparam, void *lparam);
result_t win_components_proc(window_t *win, uint32_t msg,
                              uint32_t wparam, void *lparam);
result_t win_tool_palette_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam);
result_t win_property_browser_proc(window_t *win, uint32_t msg,
                                    uint32_t wparam, void *lparam);
result_t win_forms_browser_proc(window_t *win, uint32_t msg,
                                uint32_t wparam, void *lparam);
result_t win_plugins_browser_proc(window_t *win, uint32_t msg,
                                  uint32_t wparam, void *lparam);
void canvas_rebuild_live_controls(form_doc_t *doc);
void canvas_sync_live_controls(form_doc_t *doc);
void canvas_set_component_drag_hover(form_doc_t *doc, bool active, window_t *target);
window_t *canvas_find_component_drop_target(form_doc_t *doc, int type,
                                            int canvas_x, int canvas_y);
void form_doc_auto_layout_reflow(form_doc_t *doc);
bool canvas_drop_component(form_doc_t *doc, int type, int canvas_x, int canvas_y);
bool canvas_drop_component_to_target(form_doc_t *doc, int type, window_t *target,
                                     int screen_x, int screen_y);
int canvas_add_element(form_doc_t *doc, int type, irect16_t frame,
                       int insert_index, uint32_t parent_id);
void formeditor_rebuild_tool_palette(void);
window_t *formeditor_create_components_palette(hinstance_t hinstance);
window_t *formeditor_create_legacy_toolpalette(hinstance_t hinstance);
window_t *property_browser_create(hinstance_t hinstance);
void property_browser_refresh(form_doc_t *doc);
window_t *forms_browser_create(hinstance_t hinstance);
void forms_browser_refresh(void);
window_t *plugins_browser_create(hinstance_t hinstance);
void plugins_browser_refresh(void);

// ============================================================
// Document helpers
// ============================================================

form_doc_t *create_form_doc(int w, int h);
void        close_form_doc(form_doc_t *doc);
void        form_doc_update_title(form_doc_t *doc);
void        form_doc_activate(form_doc_t *doc);
void        form_doc_show_only(form_doc_t *doc);

// ============================================================
// Project I/O
// ============================================================

bool form_project_load(const char *path);
bool form_project_save(const char *path);

// ============================================================
// Menu dispatch
// ============================================================

extern menu_def_t  kMenus[];
extern const int   kNumMenus;
void handle_menu_command(uint16_t id);

// ============================================================
// Dialogs
// ============================================================

void show_about_dialog(window_t *parent);
bool show_props_dialog(window_t *parent, form_doc_t *doc, window_t *el);

#endif // __FORMEDITOR_H__
