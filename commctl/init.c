// Common controls library initialization
// Registers all commctl window classes with the user.dll window system

#include "commctl.h"
#include "../examples/formeditor/controls-icons.h"

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
extern result_t win_splitview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_tableview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_column_browser(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern result_t win_tray(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

#define CLASS_DESC(class_name_lit, name_prefix_lit, toolbox_icon_val, \
                   default_size_w, default_size_h, default_layout_w, default_layout_h, \
                   default_flags_val, capabilities_val, proc_fn) \
  { \
    .class_name = class_name_lit, \
    .name_prefix = name_prefix_lit, \
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
  CLASS_DESC("Button", "IDC_BTN", IC_BUTTON,
             75, 23, 0, 19, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_button),
  CLASS_DESC("CheckBox", "IDC_CHK", IC_CHECKBOX,
             97, 17, 0, 13, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_checkbox),
  CLASS_DESC("Label", "IDC_LBL", IC_TEXT,
             65, 13, 0, 13, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_label),
  CLASS_DESC("TextEdit", "IDC_EDT", IC_TEXT_FIELD,
             121, 20, 0, 13, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_textedit),
  CLASS_DESC("TextBox", "IDC_EDT", IC_TEXT_FIELD,
             121, 20, 0, 13, 0,
             0, win_textedit),
  CLASS_DESC("ListBox", "IDC_LST", IC_LIST_VIEW,
             121, 60, 0, 100, WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_list),
  CLASS_DESC("ComboBox", "IDC_CMB", IC_COMBO_BOX,
             121, 20, 0, 13, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_combobox),
  CLASS_DESC("Slider", "IDC_SLD", IC_SLIDER,
             121, 17, 0, 16, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_slider),
  CLASS_DESC("Gradient", "IDC_GRD", IC_PROGRESS_BAR,
             121, 8, 0, 8, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_gradient),
  CLASS_DESC("Column", "IDC_COL", IC_PANEL,
             120, 80, 0, 0, WINDOW_LAYOUT_CONTAINER,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_column),
  CLASS_DESC("StackView", "IDC_STK", IC_DOCUMENT_STACK,
             120, 80, 0, 0, WINDOW_LAYOUT_CONTAINER,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_stack),
  CLASS_DESC("GridView", "IDC_GRD", IC_GRID_LAYOUT,
             120, 80, 0, 0, WINDOW_LAYOUT_CONTAINER,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_grid),
  CLASS_DESC("FlowView", "IDC_FLOW", IC_DOCUMENT_STACK,
             120, 80, 0, 0, WINDOW_LAYOUT_CONTAINER,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_flow),
  CLASS_DESC("ReportView", "IDC_RVW", IC_DETAILS_VIEW,
             160, 120, 0, 100, WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_FLEXSPACE,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_reportview),
  CLASS_DESC("IconGrid", "IDC_IGD", IC_GRID_VIEW,
             160, 120, 0, 100, WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_FLEXSPACE,
             0, win_icongrid),
  CLASS_DESC("Icon", "IDC_ICO", IC_GRID_VIEW,
             128, 128, 128, 128, WINDOW_NOTITLE | WINDOW_NORESIZE,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_icon),
  CLASS_DESC("Separator", "IDC_SEP", IC_PANEL,
             80, 8, 0, 1, 0,
             FE_COMPONENT_PLACEABLE | FE_COMPONENT_SHOW_TOOLBOX, win_separator),
  CLASS_DESC("Space", "IDC_SPC", IC_PANEL,
             80, 40, 0, 0, WINDOW_FLEXSPACE,
             FE_COMPONENT_PLACEABLE, win_space),
  CLASS_DESC("TableView", "IDC_TBL", 0,
             160, 120, 0, 100, WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_FLEXSPACE,
             0, win_tableview),
  CLASS_DESC("MultiEdit", "IDC_MED", 0,
             160, 120, 0, 100, WINDOW_VSCROLL | WINDOW_FLEXSPACE,
             0, win_multiedit),
  CLASS_DESC("ToolbarButton", "IDC_TBB", 0,
             19, 19, 0, 19, 0,
             0, win_toolbar_button),
  CLASS_DESC("Image", "IDC_IMG", 0,
             0, 0, 0, 0, 0,
             0, win_image),
  CLASS_DESC("Console", "IDC_CON", 0,
             160, 120, 0, 100, WINDOW_VSCROLL,
             0, win_console),
  CLASS_DESC("FileList", "IDC_FLT", 0,
             160, 120, 0, 100, WINDOW_VSCROLL,
             0, win_filelist),
  CLASS_DESC("MenuBar", "IDC_MNB", 0,
             0, TITLEBAR_HEIGHT, 0, TITLEBAR_HEIGHT, 0,
             0, win_menubar),
  CLASS_DESC("Tray", "IDC_TRAY", 0,
             0, 0, 0, 0, WINDOW_NOTITLE | WINDOW_NOTRAYBUTTON,
             0, win_tray),
  CLASS_DESC("ScrollBar", "IDC_SCB", 0,
             8, 8, 8, 8, 0,
             0, win_scrollbar),
  CLASS_DESC("Toolbox", "IDC_TBX", 0,
             120, 300, 0, 0, 0,
             0, win_toolbox),
  CLASS_DESC("Splitter", "IDC_SPL", 0,
             6, 6, 0, 0, 0,
             0, win_splitter),
  CLASS_DESC("SplitView", "IDC_SPV", 0,
             200, 200, 0, 0, WINDOW_LAYOUT_CONTAINER,
             FE_COMPONENT_PLACEABLE, win_splitview),
  CLASS_DESC("ColumnBrowser", "IDC_CBR", 0,
             160, 200, 0, 200, 0,
             0, win_column_browser),
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
void register_commctl_classes(void) {
  int n = get_num_classes();
  for (int i = 0; i < n; i++) {
    const fe_component_desc_t *desc = get_class_at_index(i);
    if (desc)
      register_window_class(desc);
  }
}
