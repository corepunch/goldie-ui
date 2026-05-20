// Common controls library initialization
// Registers all commctl window classes with the user.dll window system

#include "commctl.h"
#include "../../examples/formeditor/controls-icons.h"

// Layout containers (stack, grid, flow, column)
extern result_t win_stack(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_stackview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_grid(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_gridview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_flow(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_flowview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_column(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Other controls
extern result_t win_menubar(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_scrollbar(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_slider(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_gradient(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_toolbox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_splitter(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_tableview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_column_browser(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

#define CLASS_DESC(class_name_lit, display_name_lit, name_prefix_lit, toolbox_ident_val, toolbox_icon_val, \
                   default_size_w, default_size_h, default_layout_w, default_layout_h, \
                   default_flags_val, capabilities_val, proc_fn) \
  { \
    .class_name = class_name_lit, \
    .display_name = display_name_lit, \
    .name_prefix = name_prefix_lit, \
    .toolbox_ident = toolbox_ident_val, \
    .toolbox_icon = toolbox_icon_val, \
    .default_size = {default_size_w, default_size_h}, \
    .capabilities = capabilities_val, \
    .proc = proc_fn, \
    .default_layout_size = {default_layout_w, default_layout_h}, \
    .default_flags = default_flags_val, \
    .default_h_align = LAYOUT_ALIGN_STRETCH, \
    .default_v_align = LAYOUT_ALIGN_STRETCH, \
  }

static const fe_component_desc_t k_commctl_classes[] = {
  CLASS_DESC("button", "Button", "IDC_BTN", 205, IC_BUTTON,
             75, 23, 0, 19, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_button),
  CLASS_DESC("checkbox", "CheckBox", "IDC_CHK", 206, IC_CHECKBOX,
             97, 17, 0, 13, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_checkbox),
  CLASS_DESC("label", "Label", "IDC_LBL", 202, IC_TEXT,
             65, 13, 0, 13, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_label),
  CLASS_DESC("textedit", "TextBox", "IDC_EDT", 203, IC_TEXT_FIELD,
             121, 20, 0, 13, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_textedit),
  CLASS_DESC("list", "ListBox", "IDC_LST", 209, IC_LIST_VIEW,
             121, 60, 0, 100, WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_list),
  CLASS_DESC("combobox", "ComboBox", "IDC_CMB", 208, IC_COMBO_BOX,
             121, 20, 0, 13, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_combobox),
  CLASS_DESC("slider", "Slider", "IDC_SLD", 210, IC_SLIDER,
             121, 17, 0, 16, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_slider),
  CLASS_DESC("gradient", "Gradient", "IDC_GRD", 211, IC_PROGRESS_BAR,
             121, 8, 0, 8, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_gradient),
  CLASS_DESC("column", "Column", "IDC_COL", 213, IC_PANEL,
             120, 80, 0, 0, WINDOW_LAYOUT_CONTAINER,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_column),
  CLASS_DESC("stack", "StackView", "IDC_STK", 217, IC_DOCUMENT_STACK,
             120, 80, 0, 0, WINDOW_LAYOUT_CONTAINER,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_stack),
  CLASS_DESC("grid", "GridView", "IDC_GRD", 218, IC_GRID_LAYOUT,
             120, 80, 0, 0, WINDOW_LAYOUT_CONTAINER,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_grid),
  CLASS_DESC("flowview", "FlowView", "IDC_FLOW", 216, IC_DOCUMENT_STACK,
             120, 80, 0, 0, WINDOW_LAYOUT_CONTAINER,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_flow),
  CLASS_DESC("reportview", "ReportView", "IDC_RVW", 212, IC_DETAILS_VIEW,
             160, 120, 0, 100, WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_FLEXSPACE,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_reportview),
  CLASS_DESC("separator", "Separator", "IDC_SEP", 214, IC_PANEL,
             80, 8, 0, 1, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_separator),
  CLASS_DESC("space", "Space", "IDC_SPC", 215, IC_PANEL,
             80, 40, 0, 0, WINDOW_FLEXSPACE,
             FE_COMPONENT_PLACEABLE, win_space),
  CLASS_DESC("tableview", "TableView", "IDC_TBL", 0, 0,
             160, 120, 0, 100, WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_FLEXSPACE,
             0, win_tableview),
  CLASS_DESC("multiedit", "MultiEdit", "IDC_MED", 0, 0,
             160, 120, 0, 100, WINDOW_VSCROLL | WINDOW_FLEXSPACE,
             0, win_multiedit),
  CLASS_DESC("toolbar_button", "ToolbarButton", "IDC_TBB", 0, 0,
             19, 19, 0, 19, 0,
             0, win_toolbar_button),
  CLASS_DESC("image", "Image", "IDC_IMG", 0, 0,
             0, 0, 0, 0, 0,
             0, win_image),
  CLASS_DESC("console", "Console", "IDC_CON", 0, 0,
             160, 120, 0, 100, WINDOW_VSCROLL,
             0, win_console),
  CLASS_DESC("filelist", "FileList", "IDC_FLT", 0, 0,
             160, 120, 0, 100, WINDOW_VSCROLL,
             0, win_filelist),
  CLASS_DESC("terminal", "Terminal", "IDC_TRM", 0, 0,
             160, 120, 0, 100, WINDOW_VSCROLL,
             0, win_terminal),
  CLASS_DESC("menubar", "MenuBar", "IDC_MNB", 0, 0,
             0, TITLEBAR_HEIGHT, 0, TITLEBAR_HEIGHT, 0,
             0, win_menubar),
  CLASS_DESC("scrollbar", "ScrollBar", "IDC_SCB", 0, 0,
             8, 8, 8, 8, 0,
             0, win_scrollbar),
  CLASS_DESC("toolbox", "Toolbox", "IDC_TBX", 0, 0,
             120, 300, 0, 0, 0,
             0, win_toolbox),
  CLASS_DESC("splitter", "Splitter", "IDC_SPL", 0, 0,
             6, 6, 0, 0, 0,
             0, win_splitter),
  CLASS_DESC("column_browser", "ColumnBrowser", "IDC_CBR", 0, 0,
             160, 200, 0, 200, 0,
             0, win_column_browser),

  // Compatibility aliases
  CLASS_DESC("stackview", "StackViewAlias", "IDC_STK", 0, 0,
             120, 80, 0, 0, WINDOW_LAYOUT_CONTAINER,
             0, win_stack),
  CLASS_DESC("flow", "Flow", "IDC_FLW", 0, 0,
             120, 80, 0, 0, WINDOW_LAYOUT_CONTAINER,
             0, win_flow),
  CLASS_DESC("gridview", "GridViewAlias", "IDC_GRD", 0, 0,
             120, 80, 0, 0, WINDOW_LAYOUT_CONTAINER,
             0, win_grid),
};

#undef CLASS_DESC

int get_num_classes(void) {
  return (int)ARRAY_LEN(k_commctl_classes);
}

const fe_component_desc_t *get_class_at_index(int index) {
  if (index < 0 || index >= get_num_classes())
    return NULL;
  return &k_commctl_classes[index];
}

// Register all common controls with the window system.
// Called once during framework initialization.
void register_commctl_classes(void) {
  int n = get_num_classes();
  for (int i = 0; i < n; i++) {
    const fe_component_desc_t *desc = get_class_at_index(i);
    if (desc)
      register_window_class(desc);
  }
}
