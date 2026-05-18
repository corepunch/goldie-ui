// Window management implementation
// Extracted from mapview/window.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#include "user.h"
#include "messages.h"
#include "draw.h"
#include "../commctl/commctl.h"

typedef struct {
  fe_component_desc_t desc;
} window_class_t;

static window_class_t g_window_classes[MAX_WINDOW_CLASSES];
static int g_window_class_count = 0;

// NeXTSTEP-style database singleton
static database_t *g_app_database = NULL;

void ui_set_database(database_t *db) {
  g_app_database = db;
}

database_t *ui_get_database(void) {
  return g_app_database;
}

static bool streq(const char *a, const char *b) {
  return a && b && strcmp(a, b) == 0;
}

static bool is_stack_or_flow_proc(const window_t *win) {
  return win && (win->proc == win_stack || win->proc == win_stackview ||
                 win->proc == win_flow || win->proc == win_flowview);
}

static bool is_layout_container_proc(const window_t *win) {
  if (!win) return false;
  // Auto-layout dialogs/panels act as layout containers
  if (win->flags & WINDOW_AUTO_LAYOUT) return true;
  // Explicit layout container procs
  return is_stack_or_flow_proc(win) || win->proc == win_grid ||
         win->proc == win_gridview || win->proc == win_column;
}

static bool layout_child_flex_affects_parent(const window_t *parent, const window_t *child) {
  if (!parent || !child || !(child->flags & WINDOW_FLEXSPACE)) return false;

  bool parent_is_stack_like = is_stack_or_flow_proc(parent);

  // Horizontal action rows often contain local flex spacers.  If that row is
  // nested in a vertical container, keep the spacer local to the row instead of
  // promoting the whole row to a vertically flexible child.
  if (!is_layout_container_proc(child) && parent_is_stack_like &&
      (parent->flags & WINDOW_STACK_HORIZONTAL) &&
      parent->parent && is_layout_container_proc(parent->parent) &&
      !(parent->parent->flags & WINDOW_STACK_HORIZONTAL)) {
    return false;
  }

  // Horizontal stack/flow rows use flex spacers locally.  They should not make
  // an orthogonal parent stack claim extra vertical room.
  if (is_stack_or_flow_proc(child)) {
    bool child_horizontal  = (child->flags & WINDOW_STACK_HORIZONTAL) != 0;
    bool parent_horizontal = (parent->flags & WINDOW_STACK_HORIZONTAL) != 0;
    if (child_horizontal != parent_horizontal) return false;
  }

  return true;
}

bool register_window_class(const fe_component_desc_t *desc) {
  if (!desc || !desc->class_name || !*desc->class_name || !desc->proc) return false;
  for (int i = 0; i < g_window_class_count; i++) {
    if (streq(g_window_classes[i].desc.class_name, desc->class_name))
      return true;  // already registered — idempotent on all platforms
  }
  if (g_window_class_count >= MAX_WINDOW_CLASSES) return false;
  g_window_classes[g_window_class_count++].desc = *desc;
  return true;
}

bool register_window_class_once(const fe_component_desc_t *desc) {
  return register_window_class(desc);
}

winproc_t find_window_class_proc(const char *class_name) {
  if (!class_name || !*class_name) return NULL;
  for (int i = 0; i < g_window_class_count; i++) {
    if (streq(g_window_classes[i].desc.class_name, class_name))
      return g_window_classes[i].desc.proc;
  }
  return NULL;
}

const fe_component_desc_t *find_window_class_desc(const char *class_name) {
  if (!class_name || !*class_name) return NULL;
  for (int i = 0; i < g_window_class_count; i++) {
    if (streq(g_window_classes[i].desc.class_name, class_name))
      return &g_window_classes[i].desc;
  }
  return NULL;
}

int16_t get_class_default_width(const char *class_name) {
  const fe_component_desc_t *desc = find_window_class_desc(class_name);
  return desc ? desc->default_width : 0;
}

int16_t get_class_default_height(const char *class_name) {
  const fe_component_desc_t *desc = find_window_class_desc(class_name);
  return desc ? desc->default_height : 0;
}

flags_t get_class_default_flags(const char *class_name) {
  const fe_component_desc_t *desc = find_window_class_desc(class_name);
  return desc ? desc->default_flags : 0;
}

uint8_t get_class_default_h_align(const char *class_name) {
  const fe_component_desc_t *desc = find_window_class_desc(class_name);
  return desc ? desc->default_h_align : LAYOUT_ALIGN_STRETCH;
}

uint8_t get_class_default_v_align(const char *class_name) {
  const fe_component_desc_t *desc = find_window_class_desc(class_name);
  return desc ? desc->default_v_align : LAYOUT_ALIGN_STRETCH;
}

void register_builtin_window_classes(void) {
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
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Stack layout container
  register_window_class(&(fe_component_desc_t){
    .class_name = "stack",
    .proc = win_stack,
    .default_width = 0,
    .default_height = 0,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Stack layout container (alias)
  register_window_class(&(fe_component_desc_t){
    .class_name = "stackview",
    .proc = win_stack,
    .default_width = 0,
    .default_height = 0,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Flow layout container
  register_window_class(&(fe_component_desc_t){
    .class_name = "flow",
    .proc = win_flow,
    .default_width = 0,
    .default_height = 0,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Flow layout container (alias)
  register_window_class(&(fe_component_desc_t){
    .class_name = "flowview",
    .proc = win_flow,
    .default_width = 0,
    .default_height = 0,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Grid layout container
  register_window_class(&(fe_component_desc_t){
    .class_name = "grid",
    .proc = win_grid,
    .default_width = 0,
    .default_height = 0,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
  
  // Grid layout container (alias)
  register_window_class(&(fe_component_desc_t){
    .class_name = "gridview",
    .proc = win_grid,
    .default_width = 0,
    .default_height = 0,
    .default_flags = 0,
    .default_h_align = LAYOUT_ALIGN_STRETCH,
    .default_v_align = LAYOUT_ALIGN_STRETCH,
  });
}

// Global window state
ui_runtime_state_t g_ui_runtime = {
  .running = false,
  .windows = NULL,
  .focused = NULL,
  .tracked = NULL,
  .captured = NULL,
  .dragging = NULL,
  .resizing = NULL,
  .toolbar_down_win = NULL,
  .modal_overlay_parent = NULL,
  .default_window_x = 20,
  .default_window_y = 20,
};

// Forward declarations
extern void post_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
extern int titlebar_height(window_t const *win);
extern int statusbar_height(window_t const *win);

// Window list management
void push_window(window_t *win, window_t **windows) {
  if (!*windows) {
    *windows = win;
  } else {
    window_t *p = *windows;
    while (p->next) p = p->next;
    p->next = win;
  }
}

static uint32_t next_child_id(window_t const *parent) {
  if (!parent) return 0;
  uint32_t max_id = 0;
  for (window_t *c = parent->children; c; c = c->next) {
    if (c->id > max_id)
      max_id = c->id;
  }
  toolbar_state_t *tb = window_toolbar_state((window_t *)parent);
  for (window_t *c = tb ? tb->children : NULL; c; c = c->next) {
    if (c->id > max_id)
      max_id = c->id;
  }
  if (max_id == UINT32_MAX)
    return UINT32_MAX;
  return max_id + 1;
}

// Internal: allocate and register a window without sending evCreate.
// Callers are responsible for sending evCreate (and invalidating if needed).
static window_t *alloc_window(char const *title, flags_t flags, irect16_t const *frame,
                               window_t *parent, winproc_t proc, hinstance_t hinstance) {
  window_t *win = malloc(sizeof(window_t));
  if (!win) return NULL;
  memset(win, 0, sizeof(window_t));
  win->frame = *frame;
  win->layout.layout_fixed_w = frame ? frame->w : 0;
  win->layout.layout_fixed_h = frame ? frame->h : 0;
  win->proc = proc;
  // Child controls participate in client-area layout, so they should not
  // reserve a title bar unless a caller explicitly creates a root window.
  if (parent)
    flags |= WINDOW_NOTITLE;
  
  // Phase 3: Merge class defaults with instance flags.
  // Find the class descriptor by proc and OR in default_flags.
  // This replaces the old hardcoded `if (proc == win_space)` check.
  for (int i = 0; i < g_window_class_count; i++) {
    if (g_window_classes[i].desc.proc == proc) {
      flags |= g_window_classes[i].desc.default_flags;
      break;
    }
  }
  
  win->flags = flags;
  window_set_state(win, WINDOW_STATE_VISIBLE, (flags & WINDOW_HIDDEN) == 0);
  window_set_state(win, WINDOW_STATE_DISABLED, false);
  window_set_state(win, WINDOW_STATE_EDITING, false);
  window_set_state(win, WINDOW_STATE_PRESSED, false);
  window_set_state(win, WINDOW_STATE_HOVERED, false);
  // Inherit hinstance from parent for child windows; use supplied value for roots.
  win->hinstance = parent ? parent->hinstance : hinstance;
  if (parent) {
    win->id = next_child_id(parent);
  } else {
    bool used[256]={0};
    for (window_t *w = g_ui_runtime.windows; w; w = w->next) {
      used[w->id] = true;
    }
    for (int i = 1; i < 256; i++) {
      if (!used[i]) {
        win->id = i;
      }
    }
    if (win->id == 0) {
      printf("Too many windows open\n");
    }
  }
  win->parent = parent;
  strncpy(win->title, title, sizeof(win->title));
  // Default built-in scrollbar visibility to auto so set_scroll_info() can
  // auto-show / auto-hide them.  Without this, memset(0) would leave
  // visible_mode == SB_VIS_HIDE and the bars would never appear.
  if (flags & WINDOW_HSCROLL) win->hscroll.visible_mode = SB_VIS_AUTO;
  if (flags & WINDOW_VSCROLL) win->vscroll.visible_mode = SB_VIS_AUTO;
  g_ui_runtime.focused = win;
  push_window(win, parent ? &parent->children : &g_ui_runtime.windows);
  return win;
}

// Create a new window.
// Delegates to create_window_from_form() so that both creation paths share a
// single implementation.  create_window_from_form() is declared in user.h and
// defined later in this file; the declaration makes the call valid here.
window_t* create_window_proc(char const *title,
                             flags_t flags,
                             irect16_t const *frame,
                             window_t *parent,
                             winproc_t proc,
                             hinstance_t hinstance,
                             void *lparam)
{
  form_def_t def = {
    .name        = title,
    .width       = frame ? frame->w : 0,
    .height      = frame ? frame->h : 0,
    .flags       = flags,
    .children    = NULL,
    .child_count = 0,
    .layout_spacing = 4,
  };
  int x = frame ? frame->x : 0;
  int y = frame ? frame->y : 0;
  return create_window_from_form(&def, x, y, parent, proc, hinstance, lparam);
}

window_t* create_window_class(char const *title,
                              flags_t flags,
                              irect16_t const *frame,
                              window_t *parent,
                              const char *class_name,
                              hinstance_t hinstance,
                              void *lparam)
{
  winproc_t proc = find_window_class_proc(class_name);
  if (!proc) return NULL;
  return create_window_proc(title, flags, frame, parent, proc, hinstance, lparam);
}

void *allocate_window_data(window_t *win, size_t size) {
  void *data = malloc(size);
  memset(data, 0, size);
  if (win->userdata) {
    free(win->userdata);
  }
  win->userdata = data;
  return data;
}

// Check if two windows overlap, including their non-client areas (title bar, status bar)
bool do_windows_overlap(const window_t *a, const window_t *b) {
  if (!window_has_state(a, WINDOW_STATE_VISIBLE) ||
      !window_has_state(b, WINDOW_STATE_VISIBLE))
    return false;
  int border = 1;
  int a_x1 = a->frame.x - border,              a_y1 = a->frame.y - border;
  int a_x2 = a->frame.x + a->frame.w + border, a_y2 = a->frame.y + a->frame.h + border;
  int b_x1 = b->frame.x - border,              b_y1 = b->frame.y - border;
  int b_x2 = b->frame.x + b->frame.w + border, b_y2 = b->frame.y + b->frame.h + border;
  return a_x1 < b_x2 && a_x2 > b_x1 && a_y1 < b_y2 && a_y2 > b_y1;
}

// Invalidate overlapping windows
static void invalidate_overlaps(window_t *win) {
  for (window_t *t = g_ui_runtime.windows; t; t = t->next) {
    if (t != win && do_windows_overlap(t, win)) {
      invalidate_window(t);
    }
  }
}

// Move window to new position
void move_window(window_t *win, int x, int y) {
  post_message(win, evResize, 0, NULL);
  post_message(win, evRefreshStencil, 0, NULL);

  invalidate_overlaps(win);
  invalidate_window(win);

  win->frame.x = x;
  win->frame.y = y;

  invalidate_overlaps(win);
}

// Resize window
void resize_window(window_t *win, int new_w, int new_h) {
  // Update dimensions first so every subsequent call (including the
  // synchronous evResize delivery below) sees the new size.
  win->frame.w = new_w > 0 ? new_w : win->frame.w;
  win->frame.h = new_h > 0 ? new_h : win->frame.h;

  // Notify the window synchronously so child-window resize chains
  // (e.g. doc → canvas) propagate their frames before any queued
  // paint message runs.  Using send_message here prevents a one-frame
  // lag where a child's vertical scrollbar still uses the previous
  // dimensions while the parent's border has already moved.
  send_message(win, evResize, 0, NULL);
  window_layout_sync(win);

  post_message(win, evRefreshStencil, 0, NULL);

  invalidate_overlaps(win);
  invalidate_window(win);
}

void set_default_window_position(int x, int y) {
  g_ui_runtime.default_window_x = x;
  g_ui_runtime.default_window_y = y;
}

// Remove window from global window list
static void remove_from_global_list(window_t *win) {
  if (win == g_ui_runtime.windows) {
    g_ui_runtime.windows = win->next;
  } else if (g_ui_runtime.windows) {
    for (window_t *w=g_ui_runtime.windows->next,*p=g_ui_runtime.windows;w;p=w,w=w->next) {
      if (w == win) {
        p->next = w->next;
        break;
      }
    }
  }
}

static void remove_from_parent_child_list(window_t *win) {
  if (!win || !win->parent) return;

  toolbar_state_t *parent_tb = window_toolbar_state(win->parent);

  window_t **lists[] = {
    &win->parent->children,
    parent_tb ? &parent_tb->children : NULL,
  };

  for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); i++) {
    window_t **link = lists[i];
    while (*link) {
      if (*link == win) {
        *link = win->next;
        win->next = NULL;
        return;
      }
      link = &(*link)->next;
    }
  }
}

// Remove window hooks
extern void remove_from_global_hooks(window_t *win);

// Remove window from message queue
extern void remove_from_global_queue(window_t *win);

// Clear all toolbar child windows
void clear_toolbar_children(window_t *win) {
  toolbar_state_t *tb = window_toolbar_state(win);
  while (tb && tb->children) {
    window_t *tc   = tb->children;
    window_t *next = tc->next;
    // Detach from parent list before destroy so that any re-entrant traversal
    // (e.g. is_valid_window_ptr, evDestroy) sees only still-live nodes.
    tb->children = next;
    tc->next = NULL;
    destroy_window(tc);
  }
}

// Clear all child windows
void clear_window_children(window_t *win) {
  for (window_t *item = win->children, *next = item ? item->next : NULL;
       item; item = next, next = next?next->next:NULL) {
    destroy_window(item);
  }
  win->children = NULL;
}

// Destroy a window
void destroy_window(window_t *win) {
  window_t *root = get_root_window(win);
  post_message((window_t*)1, evRefreshStencil, 0, NULL);
  invalidate_overlaps(win);
  send_message(win, evDestroy, 0, NULL);
  if (g_ui_runtime.focused == win) set_focus(NULL);
  if (g_ui_runtime.captured == win) set_capture(NULL);
  if (g_ui_runtime.tracked == win) track_mouse(NULL);
  if (g_ui_runtime.dragging == win) g_ui_runtime.dragging = NULL;
  if (g_ui_runtime.resizing == win) g_ui_runtime.resizing = NULL;
  if (g_ui_runtime.toolbar_down_win == win) g_ui_runtime.toolbar_down_win = NULL;
  if (win->parent && win->parent->toolbar == win)
    win->parent->toolbar = NULL;
  if (win->parent)
    remove_from_parent_child_list(win);
  else
    remove_from_global_list(win);
  remove_from_global_hooks(win);
  remove_from_global_queue(win);
  clear_toolbar_children(win);
  clear_window_children(win);
  free(win);

  post_message((window_t*)1, evRefreshStencil, 0, NULL);
  if (root && root != win && is_window(root) && window_has_state(root, WINDOW_STATE_VISIBLE)) {
    invalidate_window(root);
  }
  for (window_t *w = g_ui_runtime.windows; w; w = w->next) {
    if (window_has_state(w, WINDOW_STATE_VISIBLE))
      invalidate_window(w);
  }
}

// Find window at coordinates
#define CONTAINS(x, y, x1, y1, w1, h1) \
((x1) <= (x) && (y1) <= (y) && (x1) + (w1) > (x) && (y1) + (h1) > (y))

extern int titlebar_height(window_t const *win);
extern int statusbar_height(window_t const *win);

window_t *find_window(int x, int y) {
  window_t *last = NULL;
  for (window_t *win = g_ui_runtime.windows; win; win = win->next) {
    if (!window_has_state(win, WINDOW_STATE_VISIBLE)) continue;
    if (CONTAINS(x, y, win->frame.x, win->frame.y, win->frame.w, win->frame.h)) {
      last = win;
      int t = titlebar_height(win);
      if (!window_has_state(win, WINDOW_STATE_DISABLED)) {
        send_message(win, evHitTest, MAKEDWORD(x - win->frame.x, y - win->frame.y - t), &last);
      }
    }
  }
  return last;
}

// Get root window
window_t *get_root_window(window_t *window) {
  return window->parent ? get_root_window(window->parent) : window;
}

int window_screen_x(window_t const *win) {
  if (!win) return 0;
  if (!win->parent) return win->frame.x;
  return window_screen_x(win->parent) + win->frame.x;
}

int window_screen_y(window_t const *win) {
  if (!win) return 0;
  if (!win->parent) return win->frame.y;
  return window_screen_y(win->parent) + titlebar_height(win->parent) + win->frame.y;
}

irect16_t center_window_rect(irect16_t frame_rect, window_t const *owner) {
  int sw = ui_get_system_metrics(kSystemMetricScreenWidth);
  int sh = ui_get_system_metrics(kSystemMetricScreenHeight);
  int top_padding = 40; // Minimum padding from the top of the screen to avoid overlapping with system UI elements
  window_t *root = owner ? get_root_window((window_t *)owner) : NULL;

  if (root) {
    frame_rect.x = root->frame.x + (root->frame.w - frame_rect.w) / 2;
    frame_rect.y = root->frame.y + (root->frame.h - frame_rect.h) / 2;
  } else if (sw > 0 && sh > 0) {
    frame_rect.x = (sw - frame_rect.w) / 2;
    frame_rect.y = (sh - frame_rect.h) / 2;
  } else {
    frame_rect.x = 0;
    frame_rect.y = 0;
  }

  if (sw > 0) {
    int max_x = MAX(0, sw - frame_rect.w);
    frame_rect.x = MAX(0, MIN(frame_rect.x, max_x));
  }

  if (sh > 0) {
    int min_y = (sh - frame_rect.h >= top_padding) ? top_padding : 0;
    int max_y = MAX(0, sh - frame_rect.h);
    if (min_y > max_y) min_y = max_y;
    frame_rect.y = MAX(min_y, MIN(frame_rect.y, max_y));
  }

  return frame_rect;
}

// Find the first descendant (depth-first) with BUTTON_DEFAULT set.
// Analogous to DM_GETDEFID in WinAPI dialog management.
window_t *find_default_button(window_t *win) {
  for (window_t *child = win ? win->children : NULL; child; child = child->next) {
    if (child->flags & BUTTON_DEFAULT) return child;
    window_t *found = find_default_button(child);
    if (found) return found;
  }
  return NULL;
}

// Track mouse over window
void track_mouse(window_t *win) {
  if (g_ui_runtime.tracked == win)
    return;
  window_t *prev = g_ui_runtime.tracked;
  g_ui_runtime.tracked = win;
  if (prev) {
    send_message(prev, evMouseLeave, 0, win);
    if (is_window(prev))
      invalidate_window(prev);
  }
}

// Set window capture
void set_capture(window_t *win) {
  g_ui_runtime.captured = win;
}

// Set focused window
void set_focus(window_t* win) {
  if (win == g_ui_runtime.focused)
    return;
  if (g_ui_runtime.focused) {
    window_set_state(g_ui_runtime.focused, WINDOW_STATE_EDITING, false);
    post_message(g_ui_runtime.focused, evKillFocus, 0, win);
    invalidate_window(g_ui_runtime.focused);
  }
  if (win) {
    post_message(win, evSetFocus, 0, g_ui_runtime.focused);
    invalidate_window(win);
  }
  g_ui_runtime.focused = win;
}

// Invalidate window (request repaint).
// Always routes to the root window so that evNCPaint
// redraws the panel background (via draw_panel), erasing stale pixels from
// the previous state before evPaint redraws the content.
// For root windows get_root_window() returns win itself, so behaviour is
// identical to the previous implementation.  For child windows the root is
// invalidated, which clears the background and repaints all children —
// necessary to erase, e.g., a stale selection highlight in a child control.
//
// A evRefreshStencil is posted before the paint messages so that
// if the paint messages end up deferred to a later repost_messages() call
// (because they were added during the current processing cycle, beyond the
// captured write index), the stencil is always rebuilt at the current window
// positions before the non-client paint runs.  Without this, a move between
// two repost_messages() calls would leave NonClientPaint using a stale stencil
// from the previous frame, causing the focused border to fail the stencil test
// and not be drawn for that frame.
void invalidate_window(window_t *win) {
  window_t *root = get_root_window(win);
  post_message(root, evRefreshStencil, 0, NULL);
  post_message(root, evNCPaint, 0, NULL);
  post_message(root, evPaint, 0, NULL);
}

// Returns true when the absolute screen Y coordinate 'sy' falls within the
// draggable title-bar row of 'win'.  For windows with WINDOW_TOOLBAR the
// toolbar rows sit below the title bar and must NOT initiate a drag.
// Windows without a toolbar are entirely draggable above client area.
// Windows with WINDOW_NOTITLE have no title row; their toolbar area is the
// only non-client space and may be dragged from freely (e.g. tool palettes).
bool window_in_drag_area(window_t const *win, int sy) {
  if (win->parent) return false;
  int t = titlebar_height(win);
  if (sy < win->frame.y || sy >= win->frame.y + t) return false;
  if (!(win->flags & WINDOW_TOOLBAR) || (win->flags & WINDOW_NOTITLE)) return true;
  // Has both title bar and toolbar: only the title bar row (top TITLEBAR_HEIGHT px) is draggable.
  return sy < win->frame.y + TITLEBAR_HEIGHT;
}

// Get child window by ID
window_t *get_window_item(window_t const *win, uint32_t id) {
  toolbar_state_t *tb = window_toolbar_state((window_t *)win);
  for (window_t *item = win->children; item; item = item->next) {
    if (item->id == id) {
      return item;
    }
    window_t *child = get_window_item(item, id);
    if (child) return child;
  }
  for (window_t *tc = tb ? tb->children : NULL; tc; tc = tc->next) {
    if (tc->id == id) return tc;
  }
  return NULL;
}

// Set window item text
void set_window_item_text(window_t *win, uint32_t id, const char *fmt, ...) {
  window_t *item = get_window_item(win, id);
  if (!item) return;
  va_list args;
  va_start(args, fmt);
  vsnprintf(item->title, sizeof(item->title), fmt, args);
  va_end(args);
  invalidate_window(item);
}

// Returns the client area of win in client coordinates {0, 0, client_w, client_h}.
// Analogous to WinAPI GetClientRect.
irect16_t get_client_rect(window_t const *win) {
  int t = titlebar_height(win);
  int s = statusbar_height(win);
  bool has_h = (win->flags & WINDOW_HSCROLL) && win->hscroll.visible;
  bool has_v = (win->flags & WINDOW_VSCROLL) && win->vscroll.visible;
  bool h_merged = has_h && (win->flags & WINDOW_STATUSBAR);
  int hstrip = (has_h && !h_merged) ? SCROLLBAR_WIDTH : 0;
  int vstrip = has_v ? SCROLLBAR_WIDTH : 0;
  int cw = win->frame.w - vstrip;
  int ch = win->frame.h - t - s - hstrip;
  if (cw < 0) cw = 0;
  if (ch < 0) ch = 0;
  return (irect16_t){0, 0, cw, ch};
}

// Adjusts *r (initially a desired client rect) to include the non-client area.
// Analogous to WinAPI AdjustWindowRectEx (without menu support).
// After the call, r->x/y are the window-top-left offsets relative to the
// desired client origin (r->x is 0, r->y is -titlebar_height), and
// r->w/r->h are the total window dimensions.
// Accounts for: title bar, toolbar (minimum one row), status bar, and
// scrollbar strips indicated by WINDOW_HSCROLL / WINDOW_VSCROLL.
// Note: WINDOW_HSCROLL merged with WINDOW_STATUSBAR does not add extra height
// (the bar is drawn inside the status-bar row in that case).
void adjust_window_rect(irect16_t *r, flags_t flags) {
  if (!r) return;
  // Compute non-client heights for the given flags.
  int t = 0;
  if (!(flags & WINDOW_NOTITLE)) t += TITLEBAR_HEIGHT;
  if (flags & WINDOW_TOOLBAR)    t += TB_SPACING + 2 * TOOLBAR_PADDING;  // minimum one toolbar row
  int s = (flags & WINDOW_STATUSBAR) ? STATUSBAR_HEIGHT : 0;
  // Horizontal scrollbar: adds SCROLLBAR_WIDTH to the bottom unless it is
  // merged with the status bar (WINDOW_STATUSBAR also set).
  bool hscroll_standalone = (flags & WINDOW_HSCROLL) && !(flags & WINDOW_STATUSBAR);
  int hstrip = hscroll_standalone ? SCROLLBAR_WIDTH : 0;
  // Vertical scrollbar: adds SCROLLBAR_WIDTH to the right.
  int vstrip = (flags & WINDOW_VSCROLL) ? SCROLLBAR_WIDTH : 0;
  r->y -= t;
  r->w += vstrip;
  r->h += t + s + hstrip;
}

window_t *create_window2(windef_t const *def, irect16_t const *r, window_t *parent) {
  irect16_t rect = {r->x, r->y, def->w, def->h};
  window_t *win = create_window(def->text, def->flags, &rect, parent, def->class_name, 0, NULL);
  win->id = def->id;
  return win;
}

// Load child windows from definition array
void load_window_children(window_t *win, windef_t const *def) {
  int x = WINDOW_PADDING;
  int y = WINDOW_PADDING;
  for (; def->class_name; def++) {
    int w = def->w == -1 ? win->frame.w - WINDOW_PADDING*2 : def->w;
    int h = def->h == 0 ? CONTROL_HEIGHT : def->h;
    if (x + w > win->frame.w - WINDOW_PADDING || streq(def->class_name, "space")) {
      x = WINDOW_PADDING;
      for (window_t *child = win->children; child; child = child->next) {
        y = MAX(y, child->frame.y + child->frame.h);
      }
      y += LINE_PADDING;
    }
    if (streq(def->class_name, "space"))
      continue;
    window_t *item = create_window2(def, MAKERECT(x, y, w, h), win);
    if (item) {
      x += item->frame.w + LINE_PADDING;
    }
  }
}

// Create a window from a form_def_t, instantiating all child controls from
// def->children before firing evCreate on the parent.
// This allows the window proc to find its children already in place during
// evCreate, analogous to WinAPI CreateDialogIndirect behaviour.
static void create_form_children(window_t *parent, const form_ctrl_def_t *children,
                                 int child_count);

static bool form_children_use_parent_links(const form_ctrl_def_t *children, int child_count) {
  if (!children || child_count <= 0) return false;
  for (int i = 0; i < child_count; i++) {
    if (children[i].parent != 0)
      return true;
  }
  return false;
}

static bool form_children_have_parent(const form_ctrl_def_t *children, int child_count,
                                      uint32_t parent_id) {
  if (!children || child_count <= 0 || parent_id == 0) return false;
  for (int i = 0; i < child_count; i++) {
    if (children[i].parent == parent_id)
      return true;
  }
  return false;
}

static void create_form_children_flat(window_t *parent, const form_ctrl_def_t *children,
                                      int child_count, uint32_t parent_id) {
  if (!parent || !children || child_count <= 0) return;

  for (int i = 0; i < child_count; i++) {
    const form_ctrl_def_t *cd = &children[i];
    if (cd->parent != parent_id) {
      continue;
    }

    winproc_t cp = find_window_class_proc(cd->class_name);
    if (!cp) continue;

    void *param = NULL;
    layout_view_config_t cfg = {
      .orientation = cd->flags & WINDOW_STACK_HORIZONTAL,
      .spacing = cd->layout_spacing,
      .padding = cd->padding,
      .margin = cd->margin,
    };
    if (cp == win_stack || cp == win_grid || cp == win_flow ||
        cp == win_stackview || cp == win_gridview || cp == win_flowview ||
        cp == win_column) {
      param = &cfg;
    }
    label_create_params_t label_cfg = {
      .color_index = cd->color,
      .font = cd->font_set ? cd->font : FONT_SMALL,
      .color_set = cd->color_set,
    };
    if (cp == win_label)
      param = &label_cfg;
    if (cp == win_tableview && cd->lparam)
      param = (void *)cd->lparam;

    // Apply class defaults for dimensions and flags
    const fe_component_desc_t *class_desc = find_window_class_desc(cd->class_name);
    int16_t child_w = cd->size.w;
    int16_t child_h = cd->size.h;
    flags_t child_flags = cd->flags;
    uint8_t child_h_align = cd->h_align;
    uint8_t child_v_align = cd->v_align;
    
    if (class_desc) {
      // Apply default dimensions if not explicitly specified
      if (child_w == 0 && class_desc->default_width > 0)
        child_w = class_desc->default_width;
      if (child_h == 0 && class_desc->default_height > 0)
        child_h = class_desc->default_height;
      
      // Merge class default flags with instance flags
      child_flags |= class_desc->default_flags;

      // Use class default alignment if not explicitly set
      if (child_h_align == 0)
        child_h_align = class_desc->default_h_align;
      if (child_v_align == 0)
        child_v_align = class_desc->default_v_align;
    }

    irect16_t child_frame = {0, 0, child_w, child_h};
    window_t *child = create_window(cd->text ? cd->text : "", child_flags,
                                    &child_frame, parent, cp, 0, param);
    if (!child) continue;
    
    child->id = cd->id;
    child->layout.h_align = child_h_align;
    child->layout.v_align = child_v_align;
    child->layout.layout_margin = cd->margin;

    if (form_children_have_parent(children, child_count, child->id))
      create_form_children_flat(child, children, child_count, child->id);

    if (child->flags & WINDOW_AUTO_LAYOUT)
      window_layout_sync(child);
  }

  // Propagate WINDOW_FLEXSPACE from children to parent
  // This allows flexible controls (like reportview/multiedit) to automatically
  // make their container windows flexible without explicit flags in XML
  if (parent && parent->children) {
    bool any_child_flexspace = false;
    for (window_t *child = parent->children; child; child = child->next) {
      if (layout_child_flex_affects_parent(parent, child)) {
        any_child_flexspace = true;
        break;
      }
    }
    if (any_child_flexspace && !(parent->flags & WINDOW_FLEXSPACE)) {
      parent->flags |= WINDOW_FLEXSPACE;
    }
  }
}

window_t *create_window_from_form(form_def_t const *def, int x, int y,
                                  window_t *parent, winproc_t proc,
                                  hinstance_t hinstance, void *lparam) {
  if (!def || !proc) return NULL;
  

  
  if (!(def->flags & WINDOW_AUTO_LAYOUT) && def->child_count > 0) {
    fprintf(stderr, "create_window_from_form: forms with children require auto_layout=true\n");
    return NULL;
  }

  // Resolve CW_USEDEFAULT for root windows: cascade down from the configured
  // default origin.
  // Loop until we find a position not already occupied by another root window,
  // so that windows always cascade rather than stacking on top of each other.
  if (!parent && (x == CW_USEDEFAULT || y == CW_USEDEFAULT)) {
    int nx = g_ui_runtime.default_window_x;
    int ny = g_ui_runtime.default_window_y;
    bool occupied = true;
    while (occupied) {
      occupied = false;
      for (window_t *w = g_ui_runtime.windows; w; w = w->next) {
        if (!w->parent && w->frame.x == nx && w->frame.y == ny) {
          occupied = true;
          nx += DEFAULT_WINDOW_CASCADE_X;
          ny += DEFAULT_WINDOW_CASCADE_Y;
          break;
        }
      }
    }
    if (x == CW_USEDEFAULT) x = nx;
    if (y == CW_USEDEFAULT) y = ny;
  }

  irect16_t r = {x, y, def->width, def->height};

  // Allocate the parent window without sending evCreate yet.
  window_t *win = alloc_window(def->name ? def->name : "", def->flags, &r, parent, proc, hinstance);
  if (!win) return NULL;
  

  
  if (def->flags & WINDOW_AUTO_LAYOUT)
    win->flags |= WINDOW_AUTO_LAYOUT;
  win->flags &= ~WINDOW_STACK_HORIZONTAL;
  win->layout.layout_spacing    = def->layout_spacing;
  win->layout.layout_padding    = def->padding;
  win->layout.layout_margin     = def->margin;
  // Removed: forced spacing override - respect explicit spacing=0 from forms

  // Instantiate child controls before the parent proc receives evCreate.
  // Children inherit hinstance from the parent (pass 0 = inherit).

  create_form_children(win, def->children, def->child_count);

  // Auto-populate toolbar if defined
  if (def->toolbar_items && def->toolbar_count > 0 && (win->flags & WINDOW_TOOLBAR)) {
    send_message(win, tbSetItems, (uint32_t)def->toolbar_count, (void *)def->toolbar_items);
  }

  // Auto-propagate database to all tableview children
  // Use global database singleton (NeXTSTEP-style)
  database_t *effective_db = g_app_database;
  if (effective_db) {
      for (window_t *child = win->children; child; child = child->next) {
      if (child->proc == win_tableview) {
        send_message(child, tvSetDatabase, 0, effective_db);
      }
      // Recurse into nested containers
      for (window_t *grandchild = child->children; grandchild; grandchild = grandchild->next) {
        if (grandchild->proc == win_tableview) {
          send_message(grandchild, tvSetDatabase, 0, effective_db);
        }
      }
    }
  }

  if (win->flags & WINDOW_AUTO_LAYOUT)
    window_layout_sync(win);

  // Now notify the parent that creation (with children already present) is complete.
  send_message(win, evCreate, 0, lparam);
  if (win->flags & WINDOW_AUTO_LAYOUT)
    window_layout_sync(win);
  // For root windows (no parent), check whether the proc destroyed the window
  // during evCreate (e.g. end_dialog called from within the proc).
  // Child windows are in parent->children, not the global list, so skip the
  // check for them — child self-destruction during create is not a supported pattern.
  if (!parent && !is_window(win)) return NULL;
  if (parent) invalidate_window(win);
  return win;
}

static void create_form_children(window_t *parent, const form_ctrl_def_t *children,
                                 int child_count) {
  if (!parent || !children || child_count <= 0) return;

  if (form_children_use_parent_links(children, child_count)) {
    create_form_children_flat(parent, children, child_count, 0);
    return;
  }
  
  for (int i = 0; i < child_count; i++) {
    const form_ctrl_def_t *cd = &children[i];
    winproc_t cp = find_window_class_proc(cd->class_name);
    if (!cp) continue;

    void *param = NULL;
    layout_view_config_t cfg = {
      .orientation = cd->flags & WINDOW_STACK_HORIZONTAL,
      .spacing = cd->layout_spacing,
      .padding = cd->padding,
      .margin = cd->margin,
    };
    if (cp == win_stack || cp == win_grid || cp == win_flow ||
        cp == win_stackview || cp == win_gridview || cp == win_flowview ||
        cp == win_column) {
      param = &cfg;
    }
    label_create_params_t label_cfg = {
      .color_index = cd->color,
      .font = cd->font_set ? cd->font : FONT_SMALL,
      .color_set = cd->color_set,
    };
    if (cp == win_label)
      param = &label_cfg;
    
    // For tableview, params are pre-generated in the form definition
    if (cp == win_tableview && cd->lparam)
      param = (void *)cd->lparam;

    // Phase 3: Apply class defaults for width/height when form doesn't specify (0).
    // Get class descriptor to check for default dimensions.
    const fe_component_desc_t *class_desc = find_window_class_desc(cd->class_name);
    int16_t effective_w = cd->size.w;
    int16_t effective_h = cd->size.h;
    if (class_desc) {
      if (effective_w == 0 && class_desc->default_width > 0)
        effective_w = class_desc->default_width;
      if (effective_h == 0 && class_desc->default_height > 0)
        effective_h = class_desc->default_height;
    }

    irect16_t child_frame = {0, 0, effective_w, effective_h};
    window_t *child = create_window(cd->text ? cd->text : "", cd->flags,
                                    &child_frame, parent, cp, 0, param);
    if (!child) continue;
    child->id = cd->id;
    child->layout.h_align = cd->h_align;
    child->layout.v_align = cd->v_align;
    child->layout.layout_margin = cd->margin;

    if (cd->children && cd->child_count > 0)
      create_form_children(child, cd->children, cd->child_count);

    if (child->flags & WINDOW_AUTO_LAYOUT)
      window_layout_sync(child);
  }

  // Propagate WINDOW_FLEXSPACE from children to parent
  if (parent && parent->children) {
    bool any_child_flexspace = false;
    for (window_t *child = parent->children; child; child = child->next) {
      if (layout_child_flex_affects_parent(parent, child)) {
        any_child_flexspace = true;
        break;
      }
    }
    if (any_child_flexspace && !(parent->flags & WINDOW_FLEXSPACE)) {
      parent->flags |= WINDOW_FLEXSPACE;
    }
  }
}

// Show or hide window
void show_window(window_t *win, bool visible) {
  post_message(win, evRefreshStencil, 0, NULL);
  if (!visible) {
    invalidate_overlaps(win);
    if (g_ui_runtime.focused == win) set_focus(NULL);
    if (g_ui_runtime.captured == win) set_capture(NULL);
    if (g_ui_runtime.tracked == win) track_mouse(NULL);
  } else {
    move_to_top(win);
    if (!(win->flags & WINDOW_NOACTIVATE))
      set_focus(win);
  }
  window_set_state(win, WINDOW_STATE_VISIBLE, visible);
  post_message(win, evShowWindow, visible, NULL);
}

// Check if pointer is a valid window
bool is_window(window_t *win) {
  for (window_t *w = g_ui_runtime.windows; w; w = w->next) {
    if (w == win) return true;
  }
  return false;
}

// Enable or disable window
void enable_window(window_t *win, bool enable) {
  if (!enable && g_ui_runtime.focused == win) {
    set_focus(NULL);
  }
  window_set_state(win, WINDOW_STATE_DISABLED, !enable);
  invalidate_window(win);
}

// ---- Built-in scrollbar API (WinAPI SetScrollInfo / GetScrollInfo style) ----

// Clamp pos to the valid range [min_val .. max_val-page]
static int sb_clamp_range(win_sb_t const *sb, int pos) {
  int max_pos = sb->max_val - sb->page;
  if (max_pos < sb->min_val) max_pos = sb->min_val;
  if (pos < sb->min_val) return sb->min_val;
  if (pos > max_pos)     return max_pos;
  return pos;
}

// Update one built-in scrollbar from a scroll_info_t.
// Auto-shows the bar when content exceeds the viewport; hides it otherwise.
static void set_scroll_info_one(win_sb_t *sb, scroll_info_t const *info) {
  if (info->fMask & SIF_RANGE) {
    sb->min_val = info->nMin;
    sb->max_val = info->nMax;
  }
  if (info->fMask & SIF_PAGE) {
    sb->page = info->nPage;
  }
  if (info->fMask & SIF_POS) {
    sb->pos = sb_clamp_range(sb, info->nPos);
  }
  // Clamp existing pos whenever range or page changes (even without SIF_POS).
  if (info->fMask & (SIF_RANGE | SIF_PAGE)) {
    sb->pos = sb_clamp_range(sb, sb->pos);
  }
  // Automatic show/hide: hide when the whole content fits in the viewport.
  // Only apply auto logic when not overridden by an explicit show_scroll_bar() call.
  if (sb->visible_mode == SB_VIS_HIDE) {
    sb->visible = false; // forced hidden
  } else if (sb->visible_mode == SB_VIS_SHOW) {
    sb->visible = true;  // forced shown
  } else {
    bool should_show = (sb->page < sb->max_val - sb->min_val);
    sb->visible = should_show;
  }
  if (sb->visible && !sb->enabled) {
    // First time visible: default to enabled.
    sb->enabled = true;
  }
}

void set_scroll_info(window_t *win, int bar, scroll_info_t const *info, bool redraw) {
  if (!win || !info) return;
  if (bar == SB_VERT) {
    set_scroll_info_one(&win->vscroll, info);
  } else if (bar == SB_HORZ) {
    set_scroll_info_one(&win->hscroll, info);
  } else { // SB_BOTH
    set_scroll_info_one(&win->hscroll, info);
    set_scroll_info_one(&win->vscroll, info);
  }
  if (redraw) invalidate_window(win);
}

void get_scroll_info(window_t *win, int bar, scroll_info_t *info) {
  if (!win || !info) return;
  if (bar == SB_BOTH) bar = SB_HORZ; // SB_BOTH reads horizontal by convention
  win_sb_t *sb = (bar == SB_VERT) ? &win->vscroll : &win->hscroll;
  if (info->fMask & SIF_RANGE) {
    info->nMin = sb->min_val;
    info->nMax = sb->max_val;
  }
  if (info->fMask & SIF_PAGE) info->nPage = sb->page;
  if (info->fMask & SIF_POS)  info->nPos  = sb->pos;
}

int get_scroll_pos(window_t *win, int bar) {
  if (!win) return 0;
  if (bar == SB_VERT) return win->vscroll.pos;
  return win->hscroll.pos; // SB_HORZ or SB_BOTH → horizontal
}

// Explicitly enable or disable a built-in scrollbar's mouse interactivity.
// Disabled bars remain visible but ignore mouse clicks.
void enable_scroll_bar(window_t *win, int bar, bool enable) {
  if (!win) return;
  if (bar == SB_HORZ || bar == SB_BOTH) win->hscroll.enabled = enable;
  if (bar == SB_VERT || bar == SB_BOTH) win->vscroll.enabled = enable;
  invalidate_window(win);
}

// Show or hide a built-in scrollbar explicitly.
// Calling this locks the bar's visibility so that subsequent set_scroll_info()
// calls do not auto-show or auto-hide it.  To restore auto-visibility mode,
// call reset_scroll_bar_auto(win, bar).
void show_scroll_bar(window_t *win, int bar, bool show) {
  if (!win) return;
  if (bar == SB_HORZ || bar == SB_BOTH) {
    win->hscroll.visible = show;
    win->hscroll.visible_mode = show ? SB_VIS_SHOW : SB_VIS_HIDE;
  }
  if (bar == SB_VERT || bar == SB_BOTH) {
    win->vscroll.visible = show;
    win->vscroll.visible_mode = show ? SB_VIS_SHOW : SB_VIS_HIDE;
  }
  invalidate_window(win);
}

// Restore auto visibility mode for a built-in scrollbar.
// After this call, set_scroll_info() will again auto-show/hide the bar based
// on the content range vs page size, undoing any prior show_scroll_bar() call.
void reset_scroll_bar_auto(window_t *win, int bar) {
  if (!win) return;
  if (bar == SB_HORZ || bar == SB_BOTH) win->hscroll.visible_mode = SB_VIS_AUTO;
  if (bar == SB_VERT || bar == SB_BOTH) win->vscroll.visible_mode = SB_VIS_AUTO;
}
