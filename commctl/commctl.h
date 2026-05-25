#ifndef __UI_COMMCTL_H__
#define __UI_COMMCTL_H__

#include "../user/user.h"
#include "columnview.h"
#include "menubar.h"
#include "filelist.h"

// Forward declarations for types from other subsystems
typedef struct database_s database_t;

// Register all common controls with the window system.
void register_commctl_classes(void);

// Built-in commctl class list APIs used by FormEditor/apps to register
// classes from this library.
int get_num_classes(void);
const fe_component_desc_t *get_class_at_index(int index);

// bitmap_strip_t is defined in user/user.h and available via the include above.
// Kept here as a comment for documentation purposes:
// A fixed-size-tile bitmap strip used with btnSetImage (wparam=index, lparam=bitmap_strip_t*).

// Scrollbar info structure (WinAPI SCROLLINFO analogue).
// Used with sbSetInfo.
typedef struct {
  int min_val; // minimum scroll position
  int max_val; // maximum scroll position (content size)
  int page;    // visible page size (viewport dimension)
  int pos;     // current scroll position
} scrollbar_info_t;

// Slider range structure (WinAPI trackbar style min/max range).
typedef struct {
  int min_val;
  int max_val;
} slider_range_t;

// Common control window procedures
lresult_t win_button(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_toolbar_button(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_checkbox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_reportview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_iconview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_icongrid(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
// Combobox creation parameters — for database-driven dropdowns.
// When source/display/value attributes are present in .orion forms,
// orionc generates a combobox_params_t structure and passes it via lparam.
typedef struct {
  database_t *db;            // Database instance (NULL = populate manually)
  int table_id;              // TABLE_* enum value for source table
  uint32_t display_field_id; // Field id to show in dropdown (e.g. ID_DB_*_NAME)
  uint32_t value_field_id;   // Field id for actual value (e.g. ID_DB_*_ID)
} combobox_params_t;

// Combobox internal state (shared with list control for dropdown)
#define MAX_COMBOBOX_STRINGS MAX_LIST_ITEMS
typedef char combobox_string_t[64];
typedef struct {
  combobox_params_t params;  // Copy of creation params
  combobox_string_t *texts;  // Display strings
  int *values;               // Value field data (e.g., IDs) for foreign keys
} combobox_state_t;

lresult_t win_combobox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_textedit(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_multiedit(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_label(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_image(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_list(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_console(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_space(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_separator(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_filelist(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_terminal(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_menubar(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_scrollbar(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_slider(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_gradient(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Column browser (NXBrowser/NSBrowser-style multi-column hierarchical navigation).
// The browser owns one single-column reportview per visible column. A passive
// delegate supplies row counts and cell contents, like NEXTSTEP's NXBrowser
// delegate methods.
typedef struct {
  void *userdata;
  int  (*number_of_rows)(void *ctx, window_t *browser, int column);
  bool (*load_cell)(void *ctx, window_t *browser, int column, int row,
                    reportview_item_t *item);
  bool (*is_leaf)(void *ctx, window_t *browser, int column, int row);
  const char *(*title_of_column)(void *ctx, window_t *browser, int column);
  int  (*width_of_column)(void *ctx, window_t *browser, int column);
  bool (*column_is_valid)(void *ctx, window_t *browser, int column);
  bool (*load_drag_payload)(void *ctx, window_t *browser, int column, int row,
                            const reportview_item_t *item,
                            ui_drag_item_payload_t *payload);
  void (*did_select)(void *ctx, window_t *browser, int column, int row);
  void (*did_scroll)(void *ctx, window_t *browser);
} column_browser_delegate_t;

enum {
  CBM_SETDELEGATE = evUser + 300,   // lparam = const column_browser_delegate_t*
  CBM_LOADCOLUMNZERO,               // load/reset column zero; unloads later columns
  CBM_ADDCOLUMN,                    // append a column to the right of lastColumn
  CBM_RELOADCOLUMN,                 // wparam = column
  CBM_DISPLAYCOLUMN,                // wparam = column; layouts and scrolls into range
  CBM_DISPLAYALLCOLUMNS,            // layouts all loaded columns
  CBM_SETLASTCOLUMN,                // wparam = last loaded column; unloads later columns
  CBM_GETLASTCOLUMN,                // returns last loaded column or -1
  CBM_GETSELECTEDCOLUMN,            // returns last column with selected row or -1
  CBM_GETSELECTION,                 // wparam = column; returns selected row or -1
  CBM_GETCOLUMNWINDOW,              // wparam = column; returns window_t*
  CBM_SETCOLUMNWIDTH,               // wparam = column; lparam = (void*)(intptr_t)width
  CBM_GETCOLUMNWIDTH,               // wparam = column; returns width
  CBM_SETMINCOLUMNWIDTH,            // wparam = width
  CBM_GETMINCOLUMNWIDTH,            // returns width
  CBM_SETMAXVISIBLECOLUMNS,         // wparam = count
  CBM_GETMAXVISIBLECOLUMNS,         // returns count
  CBM_GETCOLUMNCOUNT,               // returns loaded column count
  CBM_VALIDATEVISIBLECOLUMNS,       // invokes delegate validation and reloads invalid columns
};

enum {
  CBN_SELCHANGE = 260,
  CBN_DBLCLK,
  CBN_SCROLL,
  CBN_BEGINDRAG,
};

lresult_t win_column_browser(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Database-backed table view — automatically populates from database API.
// See commctl/tableview.c for full documentation and usage examples.
enum {
  tvRefresh = evUser + 260,
  tvSetFilter,
};

typedef struct {
  database_t *db;              // Database instance
  int table_id;                // TABLE_* enum value
  int filter_field;            // Field to filter by (0 = fetch all)
  intptr_t filter_value;       // Value to match
  const uint32_t *field_ids;   // Column field ids
  const char **column_titles;  // Column display titles
  const int *column_widths;    // Column widths (0 = flex, NULL = all 0)
  int column_count;            // Number of table columns
} tableview_params_t;

lresult_t win_tableview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_reportcolumn(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Auto-layout container windows.
typedef struct {
  flags_t orientation;       // WINDOW_STACK_HORIZONTAL bit flag; 0 = vertical
  uint8_t spacing;           // spacing between direct children (0 = default)
  irect16_t padding;         // inner padding for the container
  irect16_t margin;          // outer margin when nested in a parent layout
} layout_view_config_t;

lresult_t win_stack(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_grid(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_flow(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
// Backward-compatible aliases for legacy call sites.
lresult_t win_stackview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_gridview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_flowview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
lresult_t win_column(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
void layout_stack_measure_window(window_t *win, layout_measure_t *m);
void layout_stack_arrange_window(window_t *win, const irect16_t *rect);
void layout_grid_measure_window(window_t *win, layout_measure_t *m);
void layout_grid_arrange_window(window_t *win, const irect16_t *rect);
void layout_flow_measure_window(window_t *win, layout_measure_t *m);
void layout_flow_arrange_window(window_t *win, const irect16_t *rect);
void layout_flow_horizontal(window_t *first, int start_x, int gap);

// Toolbox — 2-column grid of icon buttons (Photoshop / VB3 / Paint style).
// See commctl/toolbox.c for the full API and usage examples.
lresult_t win_toolbox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Splitter — thin draggable divider bar between two sibling panels.
// Orientation is set via lparam at create time:
//   (void *)SPLIT_VERT  → vertical bar (narrow column, drag left/right)
//   (void *)SPLIT_HORZ  → horizontal bar (narrow row, drag up/down)
//
// On left-click the splitter sends evCommand(MAKEDWORD(id, spnDragStart)) to
// its parent.  lparam packs the hit point in parent-local coordinates
// as MAKEDWORD(uint16_t px, uint16_t py).  The parent should:
//   1. Call set_capture(parent_win) to receive subsequent mouse events.
//   2. Track evMouseMove to recompute the layout.
//   3. Call set_capture(NULL) + stop tracking on evLeftButtonUp.
// See examples/gitclient/view_main.c for the canonical usage pattern.
#define SPLIT_VERT 0
#define SPLIT_HORZ 1
lresult_t win_splitter(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
int      win_splitter_orientation(window_t *win);

// Returns the height (in client pixels) that win_toolbox occupies for its
// button grid.  Call from a wrapping proc to find where custom content starts.
int toolbox_grid_height(window_t *win);

// Terminal API functions
const char* terminal_get_buffer(window_t *win);

// Console API functions
void init_console(void);
void conprintf(const char* format, ...);
void draw_console(void);
void shutdown_console(void);
void toggle_console(void);

#endif
