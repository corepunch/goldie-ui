// Common controls library initialization
// Registers all commctl window classes with the user.dll window system

#include "commctl.h"

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

// Register all common controls with the window system.
// Called once during framework initialization.
void register_commctl_classes(void) {
  // Button control - standard height for buttons
  register_window_class(&(fe_component_desc_t){
    .class_name = "button",
    .proc = win_button,
    .default_width = 0,    // measure content
    .default_height = 19,  // standard button height
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Label control - single line of text
  register_window_class(&(fe_component_desc_t){
    .class_name = "label",
    .proc = win_label,
    .default_width = 0,    // measure content
    .default_height = 13,  // single line height
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Text edit control - single line input
  register_window_class(&(fe_component_desc_t){
    .class_name = "textedit",
    .proc = win_textedit,
    .default_width = 0,    // stretch to fit
    .default_height = 13,  // single line height
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Checkbox control
  register_window_class(&(fe_component_desc_t){
    .class_name = "checkbox",
    .proc = win_checkbox,
    .default_width = 0,    // measure content
    .default_height = 13,  // single line height
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Combobox control
  register_window_class(&(fe_component_desc_t){
    .class_name = "combobox",
    .proc = win_combobox,
    .default_width = 0,    // stretch to fit
    .default_height = 13,  // single line height
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Separator - visual divider line (no expansion)
  register_window_class(&(fe_component_desc_t){
    .class_name = "separator",
    .proc = win_separator,
    .default_width = 0,    // stretch to container width
    .default_height = 1,   // 1px visual line
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Space element - flexible spacer that expands
  register_window_class(&(fe_component_desc_t){
    .class_name = "space",
    .proc = win_space,
    .default_width = 0,
    .default_height = 0,
    .default_flags = WINDOW_FLEXSPACE,  // Always flexible
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Reportview - scrolling list/grid control
  register_window_class(&(fe_component_desc_t){
    .class_name = "reportview",
    .proc = win_reportview,
    .default_width = 0,     // stretch to fit
    .default_height = 100,  // ~6 rows
    .default_flags = WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_FLEXSPACE,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Tableview - database-backed table view
  register_window_class(&(fe_component_desc_t){
    .class_name = "tableview",
    .proc = win_tableview,
    .default_width = 0,     // stretch to fit
    .default_height = 100,  // ~6 rows
    .default_flags = WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_FLEXSPACE,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // List control
  register_window_class(&(fe_component_desc_t){
    .class_name = "list",
    .proc = win_list,
    .default_width = 0,     // stretch to fit
    .default_height = 100,  // ~6 rows
    .default_flags = WINDOW_VSCROLL | WINDOW_NOTITLE | WINDOW_NORESIZE,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Multi-line edit control
  register_window_class(&(fe_component_desc_t){
    .class_name = "multiedit",
    .proc = win_multiedit,
    .default_width = 0,     // stretch to fit
    .default_height = 100,  // multiple lines
    .default_flags = WINDOW_VSCROLL | WINDOW_FLEXSPACE,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Toolbar button control
  register_window_class(&(fe_component_desc_t){
    .class_name = "toolbar_button",
    .proc = win_toolbar_button,
    .default_width = 0,
    .default_height = 19,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Image control
  register_window_class(&(fe_component_desc_t){
    .class_name = "image",
    .proc = win_image,
    .default_width = 0,
    .default_height = 0,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Console control
  register_window_class(&(fe_component_desc_t){
    .class_name = "console",
    .proc = win_console,
    .default_width = 0,
    .default_height = 100,
    .default_flags = WINDOW_VSCROLL,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // File list control
  register_window_class(&(fe_component_desc_t){
    .class_name = "filelist",
    .proc = win_filelist,
    .default_width = 0,
    .default_height = 100,
    .default_flags = WINDOW_VSCROLL,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Terminal control
  register_window_class(&(fe_component_desc_t){
    .class_name = "terminal",
    .proc = win_terminal,
    .default_width = 0,
    .default_height = 100,
    .default_flags = WINDOW_VSCROLL,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Menu bar control
  register_window_class(&(fe_component_desc_t){
    .class_name = "menubar",
    .proc = win_menubar,
    .default_width = 0,
    .default_height = TITLEBAR_HEIGHT,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Scrollbar control
  register_window_class(&(fe_component_desc_t){
    .class_name = "scrollbar",
    .proc = win_scrollbar,
    .default_width = 8,
    .default_height = 8,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Slider control
  register_window_class(&(fe_component_desc_t){
    .class_name = "slider",
    .proc = win_slider,
    .default_width = 0,
    .default_height = 16,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Gradient control
  register_window_class(&(fe_component_desc_t){
    .class_name = "gradient",
    .proc = win_gradient,
    .default_width = 0,
    .default_height = 8,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Toolbox control
  register_window_class(&(fe_component_desc_t){
    .class_name = "toolbox",
    .proc = win_toolbox,
    .default_width = 0,
    .default_height = 0,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Splitter control
  register_window_class(&(fe_component_desc_t){
    .class_name = "splitter",
    .proc = win_splitter,
    .default_width = 0,
    .default_height = 0,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Column layout container
  register_window_class(&(fe_component_desc_t){
    .class_name = "column",
    .proc = win_column,
    .default_width = 0,
    .default_height = 0,
    .default_flags = WINDOW_LAYOUT_CONTAINER,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Stack layout container
  register_window_class(&(fe_component_desc_t){
    .class_name = "stack",
    .proc = win_stack,
    .default_width = 0,
    .default_height = 0,
    .default_flags = WINDOW_LAYOUT_CONTAINER,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Stack layout container (alias)
  register_window_class(&(fe_component_desc_t){
    .class_name = "stackview",
    .proc = win_stack,
    .default_width = 0,
    .default_height = 0,
    .default_flags = WINDOW_LAYOUT_CONTAINER,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Flow layout container
  register_window_class(&(fe_component_desc_t){
    .class_name = "flow",
    .proc = win_flow,
    .default_width = 0,
    .default_height = 0,
    .default_flags = WINDOW_LAYOUT_CONTAINER,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Flow layout container (alias)
  register_window_class(&(fe_component_desc_t){
    .class_name = "flowview",
    .proc = win_flow,
    .default_width = 0,
    .default_height = 0,
    .default_flags = WINDOW_LAYOUT_CONTAINER,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Grid layout container
  register_window_class(&(fe_component_desc_t){
    .class_name = "grid",
    .proc = win_grid,
    .default_width = 0,
    .default_height = 0,
    .default_flags = WINDOW_LAYOUT_CONTAINER,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Grid layout container (alias)
  register_window_class(&(fe_component_desc_t){
    .class_name = "gridview",
    .proc = win_grid,
    .default_width = 0,
    .default_height = 0,
    .default_flags = WINDOW_LAYOUT_CONTAINER,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
}
