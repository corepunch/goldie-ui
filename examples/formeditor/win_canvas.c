// Form canvas window — renders the design surface, handles element
// selection/move/resize with drag handles, rubber-band placement of new
// controls, and built-in scrollbars when the form is larger than the viewport.

#include "formeditor.h"
#include "../../commctl/commctl.h"

// ============================================================
// Constants
// ============================================================

// Size of each resize-handle square drawn around the selection.
#define HANDLE_SIZE  5
#define HANDLE_HALF  (HANDLE_SIZE / 2)
#define HANDLE_HIT_OUTSET 3

// Minimum element dimensions after a resize drag.
#define MIN_ELEM_W  10
#define MIN_ELEM_H   8

// Colour of the design-time dot grid.
#define GRID_DOT_COLOR  0xFFA0A0A0
#define LAYOUT_HOVER_FILL   0x663A7DFF
#define LAYOUT_HOVER_BORDER 0xCC3A7DFF

// ============================================================
// Handle indices
//   0=TL  1=TC  2=TR
//   3=ML        4=MR
//   5=BL  6=BC  7=BR
// ============================================================
#define HANDLE_TL    0
#define HANDLE_TC    1
#define HANDLE_TR    2
#define HANDLE_ML    3
#define HANDLE_MR    4
#define HANDLE_BL    5
#define HANDLE_BC    6
#define HANDLE_BR    7
#define HANDLE_COUNT 8

// ============================================================
// Coordinate helpers
// ============================================================

static inline canvas_pt_t form_to_canvas_pt(canvas_state_t *s, form_pt_t p) {
  return (canvas_pt_t){p.x - s->pan.x, p.y - s->pan.y};
}

static inline form_pt_t canvas_to_form_pt(canvas_state_t *s, canvas_pt_t p) {
  return (form_pt_t){p.x + s->pan.x, p.y + s->pan.y};
}

static inline irect16_t form_to_canvas_rect(canvas_state_t *s, irect16_t r) {
  canvas_pt_t p = form_to_canvas_pt(s, (form_pt_t){r.x, r.y});
  return (irect16_t){p.x, p.y, r.w, r.h};
}

static void canvas_set_draw_space(window_t *win) {
  window_t *root = get_root_window(win);
  int t = titlebar_height(root);
  int cx = win->parent ? win->frame.x : 0;
  int cy = win->parent ? win->frame.y : 0;

  set_viewport(root->frame);
  set_projection(root->hscroll.pos - cx,
                 -t - cy + root->vscroll.pos,
                 root->frame.w + root->hscroll.pos - cx,
                 root->frame.h - t - cy + root->vscroll.pos);
}

// ============================================================
// Scrollbar helpers  (hscroll on doc_win, vscroll on canvas_win)
// ============================================================
static void canvas_sync_scrollbars(window_t *win, canvas_state_t *s) {
  form_doc_t *doc = s->doc;
  int content_w = doc->form_size.w;
  int content_h = doc->form_size.h;
  // vscroll strip occupies the rightmost SCROLLBAR_WIDTH pixels only when the
  // form is taller than the canvas viewport.
  bool has_vscroll = content_h > win->frame.h;
  int view_w = win->frame.w - (has_vscroll ? SCROLLBAR_WIDTH : 0);
  int view_h = win->frame.h;
  if (view_w < 0) view_w = 0;

  scroll_info_t si;
  si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  si.nMin  = 0;

  // Horizontal: owned by doc_win (merged with status bar).
  si.nMax  = content_w;
  si.nPage = view_w;
  si.nPos  = s->pan.x;
  set_scroll_info(doc->doc_win, SB_HORZ, &si, false);

  // Vertical: owned by this canvas window.
  si.nMax  = content_h;
  si.nPage = view_h;
  si.nPos  = s->pan.y;
  set_scroll_info(win, SB_VERT, &si, false);
}

static void canvas_clamp_pan(canvas_state_t *s, int win_w, int win_h) {
  form_doc_t *doc = s->doc;
  bool has_vscroll = doc->form_size.h > win_h;
  int view_w = win_w - (has_vscroll ? SCROLLBAR_WIDTH : 0);
  int max_x = MAX(0, doc->form_size.w - view_w);
  int max_y = MAX(0, doc->form_size.h - win_h);
  if (s->pan.x < 0) s->pan.x = 0;
  if (s->pan.y < 0) s->pan.y = 0;
  if (s->pan.x > max_x) s->pan.x = max_x;
  if (s->pan.y > max_y) s->pan.y = max_y;
}

static bool canvas_child_window_alive(window_t *root, window_t *target);
static form_element_t *canvas_find_element_by_id(form_doc_t *doc, uint32_t id);
static window_t *canvas_live_window_for_element(form_doc_t *doc, const form_element_t *el);
static bool canvas_doc_has_children(form_doc_t *doc, uint32_t parent_id);
static bool canvas_parent_is_layout_managed(form_doc_t *doc, uint32_t parent_id);
static bool canvas_element_parent_is_layout_managed(form_doc_t *doc,
                                                    const form_element_t *el);
static irect16_t canvas_element_canvas_rect(form_doc_t *doc, canvas_state_t *s,
                                            const form_element_t *el);
window_t *canvas_find_component_drop_target(form_doc_t *doc, int type,
                                            int canvas_x, int canvas_y);

// ============================================================
// Hit testing
// ============================================================

// Return index into doc->elements hit by (lx, ly) in window-local coords,
// or -1 if nothing was hit.  Tests in reverse paint order so topmost wins.
static int hit_test_elements(canvas_state_t *s, int lx, int ly) {
  form_doc_t *doc = s->doc;
  for (int i = doc->element_count - 1; i >= 0; i--) {
    form_element_t *el = &doc->elements[i];
    irect16_t r = canvas_element_canvas_rect(doc, s, el);
    if (lx >= r.x && lx < r.x + r.w && ly >= r.y && ly < r.y + r.h)
      return i;
  }
  return -1;
}

// Like hit_test_elements(), but clicking the current selection cycles to the
// next element underneath it at the same point.  If there is nothing lower,
// wrap back to the topmost hit so repeated clicks keep cycling.
static int hit_test_elements_cycle(canvas_state_t *s, int lx, int ly) {
  if (!s || !s->doc)
    return -1;

  int topmost = hit_test_elements(s, lx, ly);
  if (topmost < 0)
    return -1;

  if (s->selected_idx >= 0 && s->selected_idx < s->doc->element_count) {
    form_element_t *selected = &s->doc->elements[s->selected_idx];
    irect16_t selected_rc = canvas_element_canvas_rect(s->doc, s, selected);
    if (lx >= selected_rc.x && lx < selected_rc.x + selected_rc.w &&
        ly >= selected_rc.y && ly < selected_rc.y + selected_rc.h) {
      for (int i = s->selected_idx - 1; i >= 0; i--) {
        form_element_t *el = &s->doc->elements[i];
        irect16_t r = canvas_element_canvas_rect(s->doc, s, el);
        if (lx >= r.x && lx < r.x + r.w && ly >= r.y && ly < r.y + r.h)
          return i;
      }
    }
  }

  return topmost;
}

static irect16_t canvas_element_canvas_rect(form_doc_t *doc, canvas_state_t *s,
                                            const form_element_t *el) {
  if (!doc || !s || !el)
    return (irect16_t){0, 0, 0, 0};
  window_t *live = canvas_live_window_for_element(doc, el);
  if (el->parent == 0) {
    irect16_t r = form_to_canvas_rect(s, el->frame);
    if (canvas_child_window_alive(doc->canvas_win, live)) {
      r.w = live->frame.w;
      r.h = live->frame.h;
    }
    return r;
  }
  form_element_t *parent = canvas_find_element_by_id(doc, el->parent);
  if (!parent)
    return form_to_canvas_rect(s, el->frame);
  irect16_t pr = canvas_element_canvas_rect(doc, s, parent);
  if (canvas_child_window_alive(doc->canvas_win, live)) {
    return R(pr.x + live->frame.x,
             pr.y + live->frame.y,
             live->frame.w,
             live->frame.h);
  }
  return R(pr.x + el->frame.x, pr.y + el->frame.y, el->frame.w, el->frame.h);
}

// Compute the 8 handle positions (top-left corners) in window-local coords
// for the selected element, stored into out[HANDLE_COUNT].
static void get_handle_rects(canvas_state_t *s, form_element_t *el,
                              int out_x[HANDLE_COUNT], int out_y[HANDLE_COUNT]) {
  irect16_t r = canvas_element_canvas_rect(s->doc, s, el);
  int left = r.x - 1;
  int top = r.y - 1;
  int right = r.x + r.w;
  int bottom = r.y + r.h;
  int cx = r.x + r.w / 2 - HANDLE_HALF;
  int cy = r.y + r.h / 2 - HANDLE_HALF;

  out_x[HANDLE_TL] = left - HANDLE_HALF;   out_y[HANDLE_TL] = top - HANDLE_HALF;
  out_x[HANDLE_TC] = cx;                   out_y[HANDLE_TC] = top - HANDLE_HALF;
  out_x[HANDLE_TR] = right - HANDLE_HALF;  out_y[HANDLE_TR] = top - HANDLE_HALF;
  out_x[HANDLE_ML] = left - HANDLE_HALF;   out_y[HANDLE_ML] = cy;
  out_x[HANDLE_MR] = right - HANDLE_HALF;  out_y[HANDLE_MR] = cy;
  out_x[HANDLE_BL] = left - HANDLE_HALF;   out_y[HANDLE_BL] = bottom - HANDLE_HALF;
  out_x[HANDLE_BC] = cx;                   out_y[HANDLE_BC] = bottom - HANDLE_HALF;
  out_x[HANDLE_BR] = right - HANDLE_HALF;  out_y[HANDLE_BR] = bottom - HANDLE_HALF;
}

// Return which resize handle (0-7) is under (lx, ly), or -1 if none.
static int hit_test_handles(canvas_state_t *s, int lx, int ly) {
  if (s->selected_idx < 0) return -1;
  form_element_t *el = &s->doc->elements[s->selected_idx];
  int hx[HANDLE_COUNT], hy[HANDLE_COUNT];
  get_handle_rects(s, el, hx, hy);
  for (int i = 0; i < HANDLE_COUNT; i++) {
    int hit_x = hx[i] - HANDLE_HIT_OUTSET;
    int hit_y = hy[i] - HANDLE_HIT_OUTSET;
    int hit_size = HANDLE_SIZE + HANDLE_HIT_OUTSET * 2;
    if (lx >= hit_x && lx < hit_x + hit_size &&
        ly >= hit_y && ly < hit_y + hit_size)
      return i;
  }
  return -1;
}

// ============================================================
// Snap helpers
// ============================================================

// Round v to the nearest multiple of grid.
static inline int snap_val(int v, int grid) {
  if (grid <= 1) return v;
  // Correct round-to-nearest for both positive and negative v.
  int half = grid / 2;
  return ((v >= 0) ? (v + half) : (v - half)) / grid * grid;
}

// Snap a form-space coordinate to the document grid (if enabled).
static inline int snap(form_doc_t *doc, int v) {
  return (doc->snap_to_grid && doc->grid_size > 1)
             ? snap_val(v, doc->grid_size) : v;
}

// ============================================================
// Drawing helpers
// ============================================================

// Clamp a rectangle to stay within the form surface bounds.
static irect16_t clamp_to_form(form_doc_t *doc, irect16_t r) {
  if (r.x < 0) { r.w += r.x; r.x = 0; }
  if (r.y < 0) { r.h += r.y; r.y = 0; }
  if (r.x + r.w > doc->form_size.w) r.w = doc->form_size.w - r.x;
  if (r.y + r.h > doc->form_size.h) r.h = doc->form_size.h - r.y;
  if (r.w < 1) r.w = 1;
  if (r.h < 1) r.h = 1;
  return r;
}

static void draw_handles(window_t *win, canvas_state_t *s);
static void draw_element_outlines(window_t *win, canvas_state_t *s);
static void draw_rubber_band(window_t *win, canvas_state_t *s);
static void draw_layout_hover(canvas_state_t *s);
static void canvas_update_layout_hover(canvas_state_t *s, canvas_pt_t pos);
static int canvas_component_id_for_token(const char *token);

static winproc_t ctrl_type_to_proc(int type) {
  const fe_component_desc_t *c = fe_component_by_id(type);
  return c ? c->proc : NULL;
}

static bool canvas_type_is_grid(int type) {
  int g = canvas_component_id_for_token("grid");
  int gv = canvas_component_id_for_token("gridview");
  return (g >= 0 && type == g) || (gv >= 0 && type == gv);
}

static int canvas_add_element(form_doc_t *doc, int type, irect16_t frame,
                              int insert_index, uint32_t parent_id);
static void canvas_sync_live_parent_layout(form_doc_t *doc, uint32_t parent_id);

static window_t *canvas_find_window_by_id(window_t *root, uint32_t id) {
  if (!root)
    return NULL;
  if ((uint32_t)root->id == id)
    return root;
  toolbar_state_t *tb = window_toolbar_state(root);
  for (window_t *child = root->children; child; child = child->next) {
    window_t *hit = canvas_find_window_by_id(child, id);
    if (hit)
      return hit;
  }
  for (window_t *child = tb ? tb->children : NULL; child; child = child->next) {
    window_t *hit = canvas_find_window_by_id(child, id);
    if (hit)
      return hit;
  }
  return NULL;
}

static window_t *canvas_live_window_for_element(form_doc_t *doc, const form_element_t *el) {
  if (!doc || !doc->canvas_win || !el)
    return NULL;
  return canvas_find_window_by_id(doc->canvas_win, (uint32_t)el->id);
}

static form_element_t *canvas_find_element_for_live_window(window_t *win) {
  form_doc_t *doc;
  if (!win) return NULL;

  /*
   * Live controls can be nested under layout containers (for example a grid
   * column), so the canvas state is not always on the immediate parent. Walk
   * the ancestor chain until we reach the canvas window that owns the
   * document.
   */
  for (window_t *ancestor = win->parent; ancestor; ancestor = ancestor->parent) {
    canvas_state_t *s = (canvas_state_t *)ancestor->userdata;
    if (!s || !s->doc)
      continue;
    doc = s->doc;
    return canvas_find_element_by_id(doc, (uint32_t)win->id);
  }
  return NULL;
}

static bool canvas_window_is_descendant(window_t *win, window_t *ancestor) {
  for (window_t *it = win; it; it = it->parent) {
    if (it == ancestor)
      return true;
  }
  return false;
}

static bool canvas_child_window_alive(window_t *root, window_t *target) {
  if (!root || !target) return false;
  if (root == target) return true;
  toolbar_state_t *tb = window_toolbar_state(root);
  for (window_t *child = root->children; child; child = child->next) {
    if (canvas_child_window_alive(child, target))
      return true;
  }
  for (window_t *child = tb ? tb->children : NULL; child; child = child->next) {
    if (canvas_child_window_alive(child, target))
      return true;
  }
  return false;
}

static result_t design_live_ctrl_proc(window_t *win, uint32_t msg,
                                      uint32_t wparam, void *lparam) {
  form_element_t *el = canvas_find_element_for_live_window(win);
  winproc_t real_proc = el ? ctrl_type_to_proc(el->type) : NULL;

  switch (msg) {
    case evCreate: {
      form_element_t *creating_el = (form_element_t *)lparam;
      if (!real_proc && creating_el)
        real_proc = ctrl_type_to_proc(creating_el->type);
      win->flags |= WINDOW_NOTABSTOP;
      if (real_proc == win_label && creating_el) {
        label_create_params_t params = {
          .color_index = creating_el->color,
          .font = creating_el->font_set ? (ui_font_t)creating_el->font : FONT_SMALL,
          .color_set = creating_el->color_set,
        };
        return real_proc(win, msg, wparam, &params);
      }
      return real_proc ? real_proc(win, msg, wparam, NULL) : true;
    }
    case evDestroy:
    case evPaint:
    case evMeasure:
    case evArrange:
    case evResize:
    case evCanParent:
      return real_proc ? real_proc(win, msg, wparam, lparam) : false;
    case evInitChildren:
      if (el && canvas_type_is_grid(el->type) && el->parent == 0)
        return true;
      return real_proc ? real_proc(win, msg, wparam, lparam) : true;
    case evLeftButtonDown:
    case evLeftButtonDoubleClick:
    case evLeftButtonUp:
    case evRightButtonDown:
    case evRightButtonUp:
    case evMouseMove:
      return true;
    case evKeyDown:
    case evKeyUp:
    case evCommand:
    case evSetFocus:
      return true;
    case evParentNotify:
      // Layout containers are often nested under the canvas, so forward child
      // notifications up the chain until the canvas can decide selection and
      // drag handling.
      return win->parent ? send_message(win->parent, msg, wparam, lparam) : false;
    default:
      return false;
  }
}

static result_t preview_ctrl_proc(window_t *win, uint32_t msg,
                                  uint32_t wparam, void *lparam) {
  canvas_state_t *s = (win && win->parent) ? (canvas_state_t *)win->parent->userdata : NULL;
  int type = s ? s->preview_type : -1;
  winproc_t real_proc = ctrl_type_to_proc(type);

  switch (msg) {
    case evCreate:
      win->flags |= WINDOW_NOTABSTOP;
      return real_proc ? real_proc(win, msg, wparam, NULL) : true;
    case evDestroy:
    case evPaint:
    case evResize:
      return real_proc ? real_proc(win, msg, wparam, lparam) : false;
    case evLeftButtonDown:
    case evLeftButtonDoubleClick:
    case evLeftButtonUp:
    case evRightButtonDown:
    case evRightButtonUp:
    case evMouseMove:
      return true;
    case evKeyDown:
    case evKeyUp:
    case evCommand:
    case evSetFocus:
      return true;
    default:
      return false;
  }
}

static void canvas_destroy_preview(canvas_state_t *s) {
  if (!s) return;
  if (s->doc && s->doc->canvas_win &&
      canvas_child_window_alive(s->doc->canvas_win, s->preview_win))
    destroy_window(s->preview_win);
  s->preview_win = NULL;
  s->preview_type = -1;
}

static void canvas_reset_drag(canvas_state_t *s) {
  if (!s) return;
  canvas_destroy_preview(s);
  s->hover_layout_idx = -1;
  s->hover_layout_rc = (irect16_t){0, 0, 0, 0};
  s->external_component_drag = false;
  s->drag = (drag_state_t){.mode = DRAG_NONE};
  set_capture(NULL);
}

static void canvas_set_select_tool(void) {
  if (!g_app) return;
  g_app->current_tool = ID_TOOL_SELECT;
  if (app_get_window(FE_WIN_TOOLBOX))
    send_message(app_get_window(FE_WIN_TOOLBOX), bxSetActiveItem,
                 (uint32_t)ID_TOOL_SELECT, NULL);
}

static void canvas_cancel_drag(canvas_state_t *s) {
  bool was_placing = s && s->drag.mode == DRAG_RUBBERBND;
  canvas_reset_drag(s);
  if (was_placing)
    canvas_set_select_tool();
}

static void canvas_update_preview(canvas_state_t *s, int type, irect16_t form_rc,
                                  const char *text, uint32_t flags) {
  form_doc_t *doc;
  if (!s || !s->doc || !s->doc->canvas_win) return;
  doc = s->doc;
  if (type < 0 || !ctrl_type_to_proc(type)) return;
  irect16_t canvas_rc = form_to_canvas_rect(s, form_rc);
  int draw_w = MAX(form_rc.w, 1);
  int draw_h = MAX(form_rc.h, 1);

  if (!canvas_child_window_alive(doc->canvas_win, s->preview_win) ||
      s->preview_type != type) {
    canvas_destroy_preview(s);
    s->preview_type = type;
    s->preview_win = create_window(text ? text : "", flags,
                                   MAKERECT(0, 0, draw_w, draw_h),
                                   doc->canvas_win, preview_ctrl_proc, 0, NULL);
    if (!s->preview_win) return;
    s->preview_win->flags |= WINDOW_NOTABSTOP;
  }

  draw_w = MAX(draw_w, s->preview_win->frame.w);
  draw_h = MAX(draw_h, s->preview_win->frame.h);
  move_window(s->preview_win, canvas_rc.x, canvas_rc.y);
  resize_window(s->preview_win, draw_w, draw_h);
  if (text && strcmp(s->preview_win->title, text) != 0) {
    snprintf(s->preview_win->title, sizeof(s->preview_win->title), "%s", text);
    invalidate_window(s->preview_win);
  }
}

static void canvas_sync_live_element_window(form_doc_t *doc, form_element_t *el) {
  canvas_state_t *s;
  window_t *live;
  if (!doc || !doc->canvas_win || !el ||
      !canvas_child_window_alive(doc->canvas_win, (live = canvas_live_window_for_element(doc, el))))
    return;
  s = (canvas_state_t *)doc->canvas_win->userdata;
  if (!s) return;
  if (el->parent == 0) {
    irect16_t r = form_to_canvas_rect(s, el->frame);
    move_window(live, r.x, r.y);
    resize_window(live, el->frame.w, el->frame.h);
  } else if (!canvas_element_parent_is_layout_managed(doc, el)) {
    move_window(live, el->frame.x, el->frame.y);
    resize_window(live, el->frame.w, el->frame.h);
  }
  if (live->proc == win_label) {
    uint32_t packed = label_pack_userdata(el->color,
                                          el->font_set ? (ui_font_t)el->font : FONT_SMALL,
                                          el->color_set);
    if ((uint32_t)(uintptr_t)live->userdata != packed)
      live->userdata = (void *)(uintptr_t)packed;
  }
  if (strcmp(live->title, el->text) != 0) {
    snprintf(live->title, sizeof(live->title), "%s", el->text);
    invalidate_window(live);
  }
}

static void canvas_create_live_element_window(form_doc_t *doc, form_element_t *el) {
  canvas_state_t *s;
  window_t *live;
  if (!doc || !doc->canvas_win || !el) return;
  s = (canvas_state_t *)doc->canvas_win->userdata;
  if (!s) return;
  if (!ctrl_type_to_proc(el->type)) return;
  window_t *parent = doc->canvas_win;
  if (el->parent != 0) {
    form_element_t *pel = canvas_find_element_by_id(doc, el->parent);
    window_t *plive = canvas_live_window_for_element(doc, pel);
    if (pel && canvas_child_window_alive(doc->canvas_win, plive))
      parent = plive;
  }
  irect16_t r = (el->parent == 0)
      ? form_to_canvas_rect(s, el->frame)
      : R(el->frame.x, el->frame.y, el->frame.w, el->frame.h);
  live = create_window(el->text, el->flags,
                       MAKERECT(r.x, r.y, r.w, r.h),
                       parent, design_live_ctrl_proc, 0, el);
  if (!live) return;
  live->id = el->id;
  live->flags |= WINDOW_NOTABSTOP;
    if (canvas_type_is_grid(el->type) &&
      !canvas_doc_has_children(doc, el->id))
    send_message(live, evInitChildren, 0, NULL);
  if (live->frame.w > el->frame.w ||
      live->frame.h > el->frame.h) {
    el->frame.w = MAX(el->frame.w, live->frame.w);
    el->frame.h = MAX(el->frame.h, live->frame.h);
    canvas_sync_live_element_window(doc, el);
  }
}

void canvas_sync_live_controls(form_doc_t *doc) {
  if (!doc || !doc->canvas_win) return;
  for (int i = 0; i < doc->element_count; i++)
    canvas_sync_live_element_window(doc, &doc->elements[i]);
  invalidate_window(doc->canvas_win);
  property_browser_refresh(doc);
}

void canvas_rebuild_live_controls(form_doc_t *doc) {
  canvas_state_t *s;
  if (!doc || !doc->canvas_win) return;
  s = (canvas_state_t *)doc->canvas_win->userdata;
  if (!s) return;

  while (doc->canvas_win->children)
    destroy_window(doc->canvas_win->children);

  for (int i = 0; i < doc->element_count; i++) {
    canvas_create_live_element_window(doc, &doc->elements[i]);
  }

  canvas_sync_live_controls(doc);
}

// Draw the 8 resize handles around the selected element.
static void draw_handles(window_t *win, canvas_state_t *s) {
  (void)win;
  if (s->selected_idx < 0) return;
  form_element_t *el = &s->doc->elements[s->selected_idx];
  int hx[HANDLE_COUNT], hy[HANDLE_COUNT];
  get_handle_rects(s, el, hx, hy);

  // Dotted selection border (4-pixel segments, screen coords)
  irect16_t r = canvas_element_canvas_rect(s->doc, s, el);
  draw_sel_rect(R(r.x - 1, r.y - 1, r.w + 2, r.h + 2));

  // Solid handle squares
  uint32_t hcol = 0xFFFFFFFF;
  for (int i = 0; i < HANDLE_COUNT; i++)
    fill_rect(hcol, R(hx[i], hy[i], HANDLE_SIZE, HANDLE_SIZE));
}

// Soft outline for every element on the canvas so the design-time bounds are
// visible even when the live control itself has a flat interior.
static void draw_element_outlines(window_t *win, canvas_state_t *s) {
  (void)win;
  if (!s || !s->doc)
    return;

  const uint32_t outline = 0x40808080u;  // semi-transparent grey
  for (int i = 0; i < s->doc->element_count; i++) {
    form_element_t *el = &s->doc->elements[i];
    irect16_t r = canvas_element_canvas_rect(s->doc, s, el);
    if (r.w <= 0 || r.h <= 0)
      continue;
    fill_rect(outline, R(r.x - 1, r.y - 1, r.w + 2, 1));
    fill_rect(outline, R(r.x - 1, r.y - 1, 1, r.h + 2));
    fill_rect(outline, R(r.x + r.w, r.y - 1, 1, r.h + 2));
    fill_rect(outline, R(r.x - 1, r.y + r.h, r.w + 2, 1));
  }
}

// Draw a rubber-band rectangle (for placement drag) in form coords.
static void draw_rubber_band(window_t *win, canvas_state_t *s) {
  (void)win;
  if (s->drag.mode != DRAG_RUBBERBND) return;
  irect16_t rb = s->drag.place.band;
  int x0 = rb.x < 0 ? 0 : rb.x;
  int y0 = rb.y < 0 ? 0 : rb.y;
  int x1 = x0 + rb.w;
  int y1 = y0 + rb.h;
  if (x1 > s->doc->form_size.w) x1 = s->doc->form_size.w;
  if (y1 > s->doc->form_size.h) y1 = s->doc->form_size.h;
  if (x1 <= x0 || y1 <= y0) return;
  draw_sel_rect(form_to_canvas_rect(s, R(x0, y0, x1 - x0, y1 - y0)));
}

static void draw_layout_hover(canvas_state_t *s) {
  if (!s || !s->doc || !(s->doc->flags & WINDOW_AUTO_LAYOUT))
    return;
  if (s->drag.mode != DRAG_RUBBERBND && !s->external_component_drag)
    return;
  irect16_t hover_rc = s->hover_layout_rc;
  if (!s->external_component_drag && s->doc &&
      s->hover_layout_idx >= 0 && s->hover_layout_idx < s->doc->element_count)
    hover_rc = s->doc->elements[s->hover_layout_idx].frame;
  if (hover_rc.w <= 0 || hover_rc.h <= 0)
    hover_rc = R(0, 0, s->doc->form_size.w, s->doc->form_size.h);
  if (hover_rc.w <= 0 || hover_rc.h <= 0)
    return;
  irect16_t r = form_to_canvas_rect(s, hover_rc);
  fill_rect(LAYOUT_HOVER_FILL, r);
  fill_rect(LAYOUT_HOVER_BORDER, R(r.x, r.y, r.w, 1));
  fill_rect(LAYOUT_HOVER_BORDER, R(r.x, r.y + r.h - 1, r.w, 1));
  fill_rect(LAYOUT_HOVER_BORDER, R(r.x, r.y, 1, r.h));
  fill_rect(LAYOUT_HOVER_BORDER, R(r.x + r.w - 1, r.y, 1, r.h));
}

static void canvas_update_layout_hover(canvas_state_t *s, canvas_pt_t pos) {
  if (!s || !s->doc || !(s->doc->flags & WINDOW_AUTO_LAYOUT)) return;
  s->hover_layout_idx = -1;
  s->hover_layout_rc = R(0, 0, s->doc->form_size.w, s->doc->form_size.h);
  if (s->doc->element_count <= 0)
    return;
  int hit = hit_test_elements(s, pos.x, pos.y);
  if (hit >= 0 && hit < s->doc->element_count) {
    s->hover_layout_idx = hit;
    s->hover_layout_rc = s->doc->elements[hit].frame;
  }
}

static uint32_t g_grid_dot_tex = 0;
static int      g_grid_dot_tex_size = 0;

static uint32_t ensure_grid_dot_texture(int grid) {
  if (g_grid_dot_tex != 0 && g_grid_dot_tex_size == grid)
    return g_grid_dot_tex;

  if (g_grid_dot_tex != 0) {
    R_DeleteTexture(g_grid_dot_tex);
    g_grid_dot_tex = 0;
    g_grid_dot_tex_size = 0;
  }

  size_t pixel_count = (size_t)grid * (size_t)grid;
  uint8_t *pixels = (uint8_t *)calloc(pixel_count, 4);
  if (!pixels)
    return 0;

  pixels[0] = 255;
  pixels[1] = 255;
  pixels[2] = 255;
  pixels[3] = 255;
  g_grid_dot_tex = R_CreateTextureRGBA(grid, grid, pixels,
                                       R_FILTER_NEAREST, R_WRAP_REPEAT);
  free(pixels);
  if (g_grid_dot_tex != 0)
    g_grid_dot_tex_size = grid;
  return g_grid_dot_tex;
}

static void free_grid_dot_texture(void) {
  if (g_grid_dot_tex == 0)
    return;
  R_DeleteTexture(g_grid_dot_tex);
  g_grid_dot_tex = 0;
  g_grid_dot_tex_size = 0;
}

// Draw the design-time dot grid with one repeat-wrapped texture draw.
// The texture is grid x grid pixels with a single opaque dot at (0,0).
static void draw_grid(canvas_state_t *s, irect16_t canvas_rc) {
  form_doc_t *doc = s->doc;
  if (!doc->show_grid) return;
  int grid = doc->grid_size;
  if (grid <= 1) return;  // grid=1 would paint every pixel; skip for performance
  uint32_t tex = ensure_grid_dot_texture(grid);
  if (tex == 0) return;
  draw_sprite_region((int)tex, canvas_rc,
                     UV_RECT(0.0f, 0.0f,
                             (float)canvas_rc.w / (float)grid,
                             (float)canvas_rc.h / (float)grid),
                     GRID_DOT_COLOR, 0);
}

// ============================================================
// Tool -> control type mapping
// ============================================================
static int tool_to_ctrl_type(int tool) {
  for (int i = 0; i < fe_component_count(); i++) {
    const fe_component_desc_t *c = fe_component_at(i);
    if (!c) continue;
    if (c->toolbox_ident == tool && (c->capabilities & FE_COMPONENT_PLACEABLE))
      return i;
  }
  return -1;
}

static form_element_t *canvas_find_element_by_id(form_doc_t *doc, uint32_t id) {
  if (!doc || id == 0) return NULL;
  for (int i = 0; i < doc->element_count; i++) {
    if ((uint32_t)doc->elements[i].id == id)
      return &doc->elements[i];
  }
  return NULL;
}

static bool canvas_element_parent_is_layout_managed(form_doc_t *doc,
                                                    const form_element_t *el) {
  if (!doc || !el || el->parent == 0)
    return false;
  form_element_t *parent = canvas_find_element_by_id(doc, el->parent);
  window_t *plive = canvas_live_window_for_element(doc, parent);
  return parent && plive && (plive->flags & WINDOW_AUTO_LAYOUT);
}

static bool canvas_parent_is_layout_managed(form_doc_t *doc, uint32_t parent_id) {
  if (!doc || parent_id == 0)
    return false;
  form_element_t *parent = canvas_find_element_by_id(doc, parent_id);
  window_t *plive = canvas_live_window_for_element(doc, parent);
  return parent && plive && (plive->flags & WINDOW_AUTO_LAYOUT);
}

static canvas_pt_t canvas_screen_to_canvas_pt(form_doc_t *doc, int screen_x, int screen_y) {
  if (!doc || !doc->canvas_win)
    return (canvas_pt_t){screen_x, screen_y};
  return (canvas_pt_t){
    (int16_t)(screen_x - window_screen_x(doc->canvas_win) + doc->canvas_win->hscroll.pos),
    (int16_t)(screen_y - window_screen_y(doc->canvas_win) + doc->canvas_win->vscroll.pos),
  };
}

// Default dimensions for newly placed controls.
static isize16_t default_ctrl_size(int type) {
  const fe_component_desc_t *c = fe_component_by_id(type);
  return c ? c->default_size : (isize16_t){80, 20};
}

// Control type display names for use in caption and name generation.
static const char *ctrl_type_name(int type) {
  const fe_component_desc_t *c = fe_component_by_id(type);
  return c ? c->display_name : "Control";
}

static void ctrl_make_caption(int type, int index, char *text, size_t text_sz) {
  if (!text || text_sz == 0) return;
  snprintf(text, text_sz, "%s%d", ctrl_type_name(type), index);
}

static int canvas_component_id_for_token(const char *token) {
  const fe_component_desc_t *desc = fe_component_by_token(token);
  if (!desc) return -1;
  for (int i = 0; i < fe_component_count(); i++) {
    if (fe_component_at(i) == desc)
      return i;
  }
  return -1;
}

static bool canvas_doc_has_children(form_doc_t *doc, uint32_t parent_id) {
  if (!doc || parent_id == 0)
    return false;
  for (int i = 0; i < doc->element_count; i++) {
    if (doc->elements[i].parent == parent_id)
      return true;
  }
  return false;
}

window_t *canvas_find_component_drop_target(form_doc_t *doc, int type,
                                            int canvas_x, int canvas_y) {
  canvas_state_t *s;
  const fe_component_desc_t *c;
  int hit;

  if (!doc || !doc->canvas_win)
    return NULL;
  c = fe_component_by_id(type);
  if (!c)
    c = fe_component_by_tool_ident(type);
  if (!c)
    return NULL;

  s = (canvas_state_t *)doc->canvas_win->userdata;
  if (!s)
    return NULL;

  canvas_pt_t canvas_pt = canvas_screen_to_canvas_pt(doc, canvas_x, canvas_y);
  canvas_x = canvas_pt.x;
  canvas_y = canvas_pt.y;
  hit = hit_test_elements(s, canvas_x, canvas_y);
  if (hit < 0) {
    if (canvas_x >= 0 && canvas_y >= 0 &&
        canvas_x < doc->form_size.w && canvas_y < doc->form_size.h)
      return doc->canvas_win;
    return NULL;
  }

  for (form_element_t *el = &doc->elements[hit]; el; el = canvas_find_element_by_id(doc, el->parent)) {
    window_t *target = canvas_live_window_for_element(doc, el);
    if (!target)
      continue;
    if (!fe_component_rejects_parent(c, target))
      return target;
  }

  if (canvas_x >= 0 && canvas_y >= 0 &&
      canvas_x < doc->form_size.w && canvas_y < doc->form_size.h &&
      !fe_component_rejects_parent(c, doc->canvas_win))
    return doc->canvas_win;

  return NULL;
}

static bool canvas_seed_grid_children(form_doc_t *doc, int grid_index) {
  if (!doc || grid_index < 0 || grid_index >= doc->element_count)
    return false;
  form_element_t *grid = &doc->elements[grid_index];
  if (canvas_doc_has_children(doc, grid->id))
    return true;
  int column_type = canvas_component_id_for_token("column");
  if (column_type < 0)
    return false;
  irect16_t child_frame = {0, 0, 0, 0};
  if (canvas_add_element(doc, column_type, child_frame, grid_index + 1, grid->id) < 0)
    return false;
  if (canvas_add_element(doc, column_type, child_frame, grid_index + 2, grid->id) < 0)
    return false;
  if (grid_index >= 0 && grid_index < doc->element_count) {
    canvas_state_t *s = doc->canvas_win ? (canvas_state_t *)doc->canvas_win->userdata : NULL;
    if (s)
      s->selected_idx = grid_index;
  }
  return true;
}

static void canvas_sync_live_parent_layout(form_doc_t *doc, uint32_t parent_id) {
  if (!doc || parent_id == 0)
    return;
  form_element_t *parent = canvas_find_element_by_id(doc, parent_id);
  window_t *plive = canvas_live_window_for_element(doc, parent);
  if (!parent || !plive || !(plive->flags & WINDOW_AUTO_LAYOUT))
    return;
  window_layout_sync(plive);
}

// ============================================================
// Add a new element to the document
// ============================================================
static int canvas_add_element(form_doc_t *doc, int type, irect16_t frame,
                              int insert_index, uint32_t parent_id) {
  const fe_component_desc_t *c = fe_component_by_id(type);
  if (doc->element_count >= MAX_ELEMENTS) return -1;
  if (!c || type < 0 || type >= FE_MAX_COMPONENTS) return -1;
  if (!canvas_parent_is_layout_managed(doc, parent_id)) {
    if (frame.w < MIN_ELEM_W) frame.w = MIN_ELEM_W;
    if (frame.h < MIN_ELEM_H) frame.h = MIN_ELEM_H;
  }

  int index = doc->element_count;
  if (insert_index >= 0 && insert_index < doc->element_count)
    index = insert_index;
  if (index < doc->element_count) {
    memmove(&doc->elements[index + 1],
            &doc->elements[index],
            (size_t)(doc->element_count - index) * sizeof(doc->elements[0]));
  }
  form_element_t *el = &doc->elements[index];
  *el = (form_element_t){0};
  el->type  = type;
  el->id    = doc->next_id++;
  el->parent = parent_id;
  el->frame = frame;
  el->flags = 0;
  el->h_align = LAYOUT_ALIGN_STRETCH;
  el->v_align = LAYOUT_ALIGN_STRETCH;
  el->font = FONT_SMALL;
  el->font_set = false;
  el->color = brTextNormal;
  el->color_set = false;

  int n = ++doc->type_counters[type];
  // Caption (text shown inside the control)
  ctrl_make_caption(type, n, el->text, sizeof(el->text));
  // Identifier name (used by the generated form resource).
  char pfx[8];
  strncpy(pfx, c->name_prefix, sizeof(pfx) - 1);
  pfx[sizeof(pfx)-1] = '\0';
  snprintf(el->name, sizeof(el->name), "%s%d", pfx, n);

  doc->element_count++;
  canvas_state_t *s = doc->canvas_win ? (canvas_state_t *)doc->canvas_win->userdata : NULL;
  if (s)
    s->selected_idx = index;
  canvas_create_live_element_window(doc, el);
  if (canvas_type_is_grid(type))
    canvas_seed_grid_children(doc, index);
  canvas_sync_live_parent_layout(doc, parent_id);
  if ((doc->flags & WINDOW_AUTO_LAYOUT))
    form_doc_auto_layout_reflow(doc);
  else
    canvas_sync_live_controls(doc);
  canvas_sync_live_parent_layout(doc, parent_id);
  doc->modified = true;
  form_doc_update_title(doc);
  return index;
}

bool canvas_drop_component(form_doc_t *doc, int type, int canvas_x, int canvas_y) {
  return canvas_drop_component_to_target(doc, type, NULL, canvas_x, canvas_y);
}

static uint32_t canvas_target_parent_id(form_doc_t *doc, window_t *target) {
  if (!doc || !target || target == doc->canvas_win)
    return 0;

  for (window_t *it = target; it; it = it->parent) {
    for (int i = 0; i < doc->element_count; i++) {
      if ((uint32_t)doc->elements[i].id == (uint32_t)it->id)
        return (uint32_t)doc->elements[i].id;
    }
    if (it == doc->canvas_win)
      break;
  }

  return 0;
}

bool canvas_drop_component_to_target(form_doc_t *doc, int type, window_t *target,
                                     int screen_x, int screen_y) {
  canvas_state_t *s;
  const fe_component_desc_t *c;
  isize16_t size;
  irect16_t frame;
  int insert_index = -1;
  int ctrl_type = type;
  uint32_t parent_id = 0;
  int canvas_x = screen_x;
  int canvas_y = screen_y;

  if (!doc || !(doc->flags & WINDOW_AUTO_LAYOUT) || !doc->canvas_win)
    return false;
  s = (canvas_state_t *)doc->canvas_win->userdata;
  if (!s)
    return false;
  c = (ctrl_type >= 0 && ctrl_type < FE_MAX_COMPONENTS)
    ? fe_component_by_id(ctrl_type)
    : NULL;
  if (!c) {
    ctrl_type = tool_to_ctrl_type(type);
    if (ctrl_type < 0 || ctrl_type >= FE_MAX_COMPONENTS)
      return false;
    c = fe_component_by_id(ctrl_type);
  }
  if (!c)
    return false;

  if (target && target != doc->canvas_win) {
    ipoint16_t origin = {0, 0};
    for (window_t *it = target; it; it = it->parent) {
      origin.x += it->frame.x;
      origin.y += it->frame.y;
      if (!it->parent) {
        origin.y += titlebar_height(it);
        break;
      }
    }
    canvas_x = screen_x - origin.x + target->hscroll.pos;
    canvas_y = screen_y - origin.y + target->vscroll.pos;
  } else if (target == doc->canvas_win) {
    canvas_pt_t pt = canvas_screen_to_canvas_pt(doc, screen_x, screen_y);
    canvas_x = pt.x;
    canvas_y = pt.y;
  }

  parent_id = canvas_target_parent_id(doc, target ? target : doc->canvas_win);

  size = c->default_size;
  if (size.w < MIN_ELEM_W) size.w = MIN_ELEM_W;
  if (size.h < MIN_ELEM_H) size.h = MIN_ELEM_H;

  form_pt_t form_pt = canvas_to_form_pt(s, (canvas_pt_t){canvas_x, canvas_y});
  frame = (irect16_t){
    form_pt.x - size.w / 2,
    form_pt.y - size.h / 2,
    size.w,
    size.h,
  };
  frame = clamp_to_form(doc, frame);

  if ((doc->flags & WINDOW_AUTO_LAYOUT) && doc->element_count > 0) {
    if (s->external_component_drag && s->hover_layout_idx >= 0) {
      // Palette drags already tracked the hovered layout target during motion.
      // Prefer that target directly so mouse-up cannot re-hit a different
      // element because of target-local coordinate conversion.
      insert_index = s->hover_layout_idx;
    } else {
      int hit = hit_test_elements(s, canvas_x, canvas_y);
      if (hit >= 0)
        insert_index = hit;
      else if (s->hover_layout_idx >= 0)
        insert_index = s->hover_layout_idx;
    }
  }

  return canvas_add_element(doc, ctrl_type, frame, insert_index, parent_id) >= 0;
}

void canvas_set_component_drag_hover(form_doc_t *doc, bool active, window_t *target) {
  canvas_state_t *s;
  if (!doc || !doc->canvas_win) return;
  s = (canvas_state_t *)doc->canvas_win->userdata;
  if (!s) return;
  s->external_component_drag = active && (doc->flags & WINDOW_AUTO_LAYOUT);
  if (!s->external_component_drag) {
    s->hover_layout_idx = -1;
    s->hover_layout_rc = (irect16_t){0, 0, 0, 0};
    invalidate_window(doc->canvas_win);
    return;
  }
  if (!target || !canvas_child_window_alive(doc->canvas_win, target)) {
    s->hover_layout_idx = -1;
    s->hover_layout_rc = R(0, 0, doc->form_size.w, doc->form_size.h);
  } else if (target == doc->canvas_win) {
    s->hover_layout_idx = -1;
    s->hover_layout_rc = R(0, 0, doc->form_size.w, doc->form_size.h);
  } else {
    s->hover_layout_idx = -1;
    for (window_t *it = target; it; it = it->parent) {
      form_element_t *el = NULL;
      for (int i = 0; i < doc->element_count; i++) {
        if ((uint32_t)doc->elements[i].id == (uint32_t)it->id) {
          el = &doc->elements[i];
          break;
        }
      }
      if (el) {
        s->hover_layout_idx = (int)(el - doc->elements);
        break;
      }
      if (it == doc->canvas_win)
        break;
    }
    s->hover_layout_rc = R(window_screen_x(target) - window_screen_x(doc->canvas_win) + s->pan.x,
                           window_screen_y(target) - window_screen_y(doc->canvas_win) + s->pan.y,
                           target->frame.w,
                           target->frame.h);
  }
  invalidate_window(doc->canvas_win);
}

static irect16_t canvas_rubber_band_rect(canvas_state_t *s, canvas_pt_t pos) {
  form_doc_t *doc = s->doc;
  form_pt_t p = canvas_to_form_pt(s, pos);
  form_pt_t o = canvas_to_form_pt(s, s->drag.place.start);
  int x0 = snap(doc, o.x < p.x ? o.x : p.x);
  int y0 = snap(doc, o.y < p.y ? o.y : p.y);
  int x1 = snap(doc, o.x < p.x ? p.x : o.x);
  int y1 = snap(doc, o.y < p.y ? p.y : o.y);
  return (irect16_t){x0, y0, x1 - x0, y1 - y0};
}

static void canvas_update_rubber_band(canvas_state_t *s, canvas_pt_t pos) {
  s->drag.place.band = canvas_rubber_band_rect(s, pos);
  canvas_update_layout_hover(s, pos);
}

static void canvas_update_placement_preview(canvas_state_t *s) {
  form_doc_t *doc = s->doc;
  int ctrl_type = s->drag.place.ctrl_type;
  char preview_text[64];
  if (ctrl_type < 0) return;
  irect16_t preview = s->drag.place.band;
  if (preview.w < MIN_ELEM_W || preview.h < MIN_ELEM_H) {
    isize16_t size = default_ctrl_size(ctrl_type);
    preview.w = size.w;
    preview.h = size.h;
  }
  ctrl_make_caption(ctrl_type, doc->type_counters[ctrl_type] + 1,
                    preview_text, sizeof(preview_text));
  canvas_update_preview(s, ctrl_type, preview, preview_text, 0);
}

// ============================================================
// Apply resize delta to the selected element
// ============================================================
typedef struct {
  int left, top, right, bottom;
} handle_edges_t;

static const handle_edges_t k_handle_edges[HANDLE_COUNT] = {
  [HANDLE_TL] = {1, 1, 0, 0},
  [HANDLE_TC] = {0, 1, 0, 0},
  [HANDLE_TR] = {0, 1, 1, 0},
  [HANDLE_ML] = {1, 0, 0, 0},
  [HANDLE_MR] = {0, 0, 1, 0},
  [HANDLE_BL] = {1, 0, 0, 1},
  [HANDLE_BC] = {0, 0, 0, 1},
  [HANDLE_BR] = {0, 0, 1, 1},
};

static void canvas_apply_resize(canvas_state_t *s, int dx, int dy) {
  form_doc_t     *doc = s->doc;
  form_element_t *el  = &doc->elements[s->selected_idx];
  irect16_t start = s->drag.resize.frame;
  int handle = s->drag.resize.handle;
  if (handle < 0 || handle >= HANDLE_COUNT) return;
  handle_edges_t edges = k_handle_edges[handle];
  int left = start.x;
  int top = start.y;
  int right = start.x + start.w;
  int bottom = start.y + start.h;

  if (edges.left) left += dx;
  if (edges.top) top += dy;
  if (edges.right) right += dx;
  if (edges.bottom) bottom += dy;

  if (doc->snap_to_grid && doc->grid_size > 1) {
    int g = doc->grid_size;
    if (edges.left) left = snap_val(left, g);
    if (edges.top) top = snap_val(top, g);
    if (edges.right) right = snap_val(right, g);
    if (edges.bottom) bottom = snap_val(bottom, g);
  }

  int x = left;
  int y = top;
  int w = right - left;
  int h = bottom - top;

  if (w < MIN_ELEM_W) {
    if (edges.left) x = start.x + start.w - MIN_ELEM_W;
    w = MIN_ELEM_W;
  }
  if (h < MIN_ELEM_H) {
    if (edges.top) y = start.y + start.h - MIN_ELEM_H;
    h = MIN_ELEM_H;
  }
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  el->frame.x = x; el->frame.y = y; el->frame.w = w; el->frame.h = h;
}

// ============================================================
// Window procedure
// ============================================================
result_t win_canvas_proc(window_t *win, uint32_t msg,
                          uint32_t wparam, void *lparam) {
  canvas_state_t *s = (canvas_state_t *)win->userdata;
  form_doc_t *doc = s ? s->doc : NULL;

  switch (msg) {
    case evCreate: {
      canvas_state_t *st = allocate_window_data(win, sizeof(canvas_state_t));
      st->doc          = (form_doc_t *)lparam;
      st->preview_type = -1;
      st->selected_idx = -1;
      st->hover_layout_idx = -1;
      st->hover_layout_rc = (irect16_t){0, 0, 0, 0};
      st->external_component_drag = false;
      st->pan          = (ipoint16_t){0, 0};
      st->drag         = (drag_state_t){.mode = DRAG_NONE};
      canvas_sync_scrollbars(win, st);
      canvas_rebuild_live_controls(st->doc);
      return true;
    }

    case evDestroy:
      canvas_destroy_preview(s);
      free_grid_dot_texture();
      // win->userdata freed by the framework via allocate_window_data.
      return false;

    case evSetFocus:
      return false;

    case evResize: {
      if (!s) return false;
      canvas_clamp_pan(s, win->frame.w, win->frame.h);
      canvas_sync_scrollbars(win, s);
      canvas_sync_live_controls(doc);
      return false;
    }

    case evHScroll: {
      if (!s) return false;
      s->pan.x = (int)wparam;
      canvas_clamp_pan(s, win->frame.w, win->frame.h);
      canvas_sync_scrollbars(win, s);
      canvas_sync_live_controls(doc);
      return true;
    }

    case evVScroll: {
      if (!s) return false;
      s->pan.y = (int)wparam;
      canvas_clamp_pan(s, win->frame.w, win->frame.h);
      canvas_sync_scrollbars(win, s);
      canvas_sync_live_controls(doc);
      return true;
    }

    case evPaint: {
      if (!s || !doc) return true;

      // Dark workspace background
      fill_rect(get_sys_color(brWorkspaceBg),
                R(0, 0, win->frame.w, win->frame.h));

      // Form surface (window-colored rectangle with a 1px dark border)
      irect16_t form_rc = form_to_canvas_rect(s, R(0, 0, doc->form_size.w, doc->form_size.h));
      fill_rect(get_sys_color(brWindowBg), form_rc);
      fill_rect(get_sys_color(brDarkEdge), R(form_rc.x - 1, form_rc.y - 1, form_rc.w + 2, 1));
      fill_rect(get_sys_color(brDarkEdge), R(form_rc.x - 1, form_rc.y - 1, 1, form_rc.h + 2));
      fill_rect(get_sys_color(brDarkEdge), R(form_rc.x - 1, form_rc.y + form_rc.h, form_rc.w + 2, 1));
      fill_rect(get_sys_color(brDarkEdge), R(form_rc.x + form_rc.w, form_rc.y - 1, 1, form_rc.h + 2));

      // Dot grid on the form surface
      draw_grid(s, form_rc);

      for (window_t *child = win->children; child; child = child->next)
        send_message(child, evPaint, 0, NULL);
      canvas_set_draw_space(win);
      draw_element_outlines(win, s);
      draw_layout_hover(s);
      draw_handles(win, s);
      draw_rubber_band(win, s);

      return true;
    }

    case evParentNotify: {
      if (!s || !doc || !lparam) return false;
      parent_notify_t *pn = (parent_notify_t *)lparam;
      if (!pn->child || !canvas_window_is_descendant(pn->child, win))
        return false;

      uint32_t child_msg = pn->child_msg;
      uint32_t child_wp = pn->child_wparam;
      switch (child_msg) {
        case evLeftButtonDown:
        case evLeftButtonDoubleClick:
        case evLeftButtonUp:
        case evRightButtonDown:
        case evRightButtonUp:
        case evMouseMove: {
          int lx = (int16_t)LOWORD(child_wp);
          int ly = (int16_t)HIWORD(child_wp);
          int cx = window_screen_x(pn->child) - window_screen_x(doc->canvas_win) + lx;
          int cy = window_screen_y(pn->child) - window_screen_y(doc->canvas_win) + ly;
          uint32_t parent_wp = MAKEDWORD((uint16_t)cx, (uint16_t)cy);
          return win_canvas_proc(win, child_msg, parent_wp, pn->child_lparam);
        }
        case evWheel:
        case evKeyDown:
        case evKeyUp:
        case evTextInput:
          return win_canvas_proc(win, child_msg, child_wp, pn->child_lparam);
        default:
          return false;
      }
    }

    case evLeftButtonDown: {
      if (!s || !doc) return false;
      form_doc_activate(doc);
      int lx = (int16_t)LOWORD(wparam);
      int ly = (int16_t)HIWORD(wparam);
      int tool = g_app ? g_app->current_tool : ID_TOOL_SELECT;

      if (s->drag.mode != DRAG_NONE) {
        canvas_cancel_drag(s);
        canvas_sync_live_controls(doc);
        return true;
      }

      if (tool == ID_TOOL_SELECT) {
        // Check resize handles first
        int handle = hit_test_handles(s, lx, ly);
        if (handle >= 0 && s->selected_idx >= 0) {
          form_element_t *el = &doc->elements[s->selected_idx];
          s->drag = (drag_state_t){
            .mode = DRAG_RESIZE,
            .resize = {
              .start = {lx, ly},
              .frame = el->frame,
              .handle = handle,
            },
          };
          set_capture(win);
          return true;
        }
        // Hit test elements; clicking the current selection cycles to the next
        // lower overlapping element at the same point.
        int hit = hit_test_elements_cycle(s, lx, ly);
        s->selected_idx = hit;
        property_browser_refresh(doc);
        if (hit >= 0) {
          form_element_t *el = &doc->elements[hit];
          s->drag = (drag_state_t){
            .mode = DRAG_MOVE,
            .move = {
              .start = {lx, ly},
              .frame = el->frame,
            },
          };
          set_capture(win);
        } else {
          s->drag = (drag_state_t){.mode = DRAG_NONE};
        }
        invalidate_window(win);
        return true;
      }

      if ((doc->flags & WINDOW_AUTO_LAYOUT))
        return false;

      // Placement tools: start rubber-band drag
      int ctrl_type = tool_to_ctrl_type(tool);
      if (ctrl_type >= 0) {
        form_pt_t fp = canvas_to_form_pt(s, (canvas_pt_t){lx, ly});
        char preview_text[64];
        // Clamp to form surface
        if (fp.x < 0) fp.x = 0;
        if (fp.y < 0) fp.y = 0;
        if (fp.x > doc->form_size.w) fp.x = doc->form_size.w;
        if (fp.y > doc->form_size.h) fp.y = doc->form_size.h;
        s->drag = (drag_state_t){
          .mode = DRAG_RUBBERBND,
          .place = {
            .start = {lx, ly},
            .band = {fp.x, fp.y, 0, 0},
            .ctrl_type = ctrl_type,
          },
        };
        s->selected_idx = -1;
        property_browser_refresh(doc);
        ctrl_make_caption(ctrl_type, doc->type_counters[ctrl_type] + 1,
                          preview_text, sizeof(preview_text));
        canvas_update_preview(s, ctrl_type, R(fp.x, fp.y, 1, 1), preview_text, 0);
        set_capture(win);
        return true;
      }
      return false;
    }

    case evMouseMove: {
      if (!s) return false;
      int lx = (int16_t)LOWORD(wparam);
      int ly = (int16_t)HIWORD(wparam);

      if (s->drag.mode == DRAG_MOVE && s->selected_idx >= 0) {
        form_element_t *el = &doc->elements[s->selected_idx];
        int nx = snap(doc, s->drag.move.frame.x + (lx - s->drag.move.start.x));
        int ny = snap(doc, s->drag.move.frame.y + (ly - s->drag.move.start.y));
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx + el->frame.w > doc->form_size.w) nx = doc->form_size.w - el->frame.w;
        if (ny + el->frame.h > doc->form_size.h) ny = doc->form_size.h - el->frame.h;
        el->frame.x = nx;
        el->frame.y = ny;
        canvas_sync_live_controls(doc);
        invalidate_window(win);
        return true;
      }

      if (s->drag.mode == DRAG_RESIZE && s->selected_idx >= 0) {
        int dx = lx - s->drag.resize.start.x;
        int dy = ly - s->drag.resize.start.y;
        canvas_apply_resize(s, dx, dy);
        doc->modified = true;
        canvas_sync_live_controls(doc);
        invalidate_window(win);
        return true;
      }

      if (s->drag.mode == DRAG_RUBBERBND) {
        canvas_update_rubber_band(s, (canvas_pt_t){lx, ly});
        canvas_update_placement_preview(s);
        canvas_sync_live_controls(doc);
        invalidate_window(win);
        return true;
      }
      invalidate_window(win);
      return false;
    }

    case evLeftButtonUp: {
      if (!s) return false;
      int lx = (int16_t)LOWORD(wparam);
      int ly = (int16_t)HIWORD(wparam);

      if (s->drag.mode == DRAG_MOVE && s->selected_idx >= 0) {
        doc->modified = true;
        form_doc_update_title(doc);
      }

      if (s->drag.mode == DRAG_RESIZE) {
        doc->modified = true;
        form_doc_update_title(doc);
      }

      if (s->drag.mode == DRAG_RUBBERBND) {
        int ctrl_type = s->drag.place.ctrl_type;
        if (ctrl_type >= 0) {
          irect16_t frame = canvas_rubber_band_rect(s, (canvas_pt_t){lx, ly});
          // If no drag (click only), use the default size for the control.
          if (frame.w < MIN_ELEM_W || frame.h < MIN_ELEM_H) {
            isize16_t size = default_ctrl_size(ctrl_type);
            frame.w = size.w;
            frame.h = size.h;
          }
          frame = clamp_to_form(doc, frame);
          int insert_index = -1;
          if ((doc->flags & WINDOW_AUTO_LAYOUT) && doc->element_count > 0) {
            int drop_hit = hit_test_elements(s, lx, ly);
            if (drop_hit >= 0)
              insert_index = drop_hit;
            else if (s->hover_layout_idx >= 0)
              insert_index = s->hover_layout_idx;
          }
          canvas_add_element(doc, ctrl_type, frame, insert_index, 0);
        }
        // Revert to Select tool after placing
        canvas_set_select_tool();
      }

      canvas_reset_drag(s);
      canvas_sync_live_controls(doc);
      return true;
    }

    case evKeyDown: {
      if (!s) return false;
      // Del key deletes the selected element (redundant with accelerator,
      // but handles canvas-focused case when menubar proc isn't active).
      uint32_t key = wparam;
      if ((key == AX_KEY_DEL || key == AX_KEY_BACKSPACE) && s->selected_idx >= 0) {
        handle_menu_command(ID_EDIT_DELETE);
        return true;
      }
      return false;
    }

    case evWheel: {
      if (!s) return false;
      // lparam = scroll deltas MAKEDWORD(dx, dy); values already multiplied by SCROLL_SENSITIVITY
      int delta = (int16_t)HIWORD((uintptr_t)lparam);
      s->pan.y -= delta;
      canvas_clamp_pan(s, win->frame.w, win->frame.h);
      canvas_sync_scrollbars(win, s);
      canvas_sync_live_controls(doc);
      return true;
    }

    default:
      return false;
  }
}
