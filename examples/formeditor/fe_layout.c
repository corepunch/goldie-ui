#include "formeditor.h"
#include "fe_layout.h"
#include "fe_document.h"

// Layout computation - arranges elements according to layout mode
void fe_layout_reflow(form_doc_t *doc) {
  if (!doc || !(doc->flags & WINDOW_AUTO_LAYOUT)) return;
  
  // Apply component default sizes for auto-layout forms
  for (int i = 0; i < doc->element_count; i++) {
    form_element_t *el = &doc->elements[i];
    const fe_component_desc_t *desc = fe_component_by_id(el->type);
    if (desc) {
      el->frame.w = MAX(1, desc->default_size.w);
      el->frame.h = MAX(1, desc->default_size.h);
    }
  }
  
  // Collect root-level elements (parent == 0)
  form_element_t *roots[MAX_ELEMENTS];
  int root_count = 0;
  for (int i = 0; i < doc->element_count; i++) {
    if (doc->elements[i].parent == 0)
      roots[root_count++] = &doc->elements[i];
  }
  
  const int gap = doc->layout_spacing > 0 ? doc->layout_spacing : 4;
  int count = root_count;
  int pad_l = doc->padding.x;
  int pad_t = doc->padding.y;
  int pad_r = doc->padding.w;
  int pad_b = doc->padding.h;
  int max_w = MAX(1, doc->form_size.w - pad_l - pad_r);
  int max_h = MAX(1, doc->form_size.h - pad_t - pad_b);
  int content_x = pad_l;
  int content_y = pad_t;

  // Grid layout mode
  if (doc->layout_mode == 2) {
    int cols = doc->layout_columns > 0 ? doc->layout_columns : 2;
    if (cols < 1) cols = 1;
    int rows = (count + cols - 1) / cols;
    if (rows < 1) rows = 1;
    int base_w = max_w / cols;
    int rem_w = max_w % cols;
    int base_h = max_h / rows;
    int rem_h = max_h % rows;
    for (int i = 0; i < count; i++) {
      form_element_t *el = roots[i];
      int row = i / cols;
      int col = i % cols;
      irect16_t margin = el->margin;
      int cell_w = base_w + (col < rem_w ? 1 : 0);
      int cell_h = base_h + (row < rem_h ? 1 : 0);
      int ow = el->frame.w > 0 ? el->frame.w + margin.x + margin.w : margin.x + margin.w + 1;
      int oh = el->frame.h > 0 ? el->frame.h + margin.y + margin.h : margin.y + margin.h + 1;
      int x = content_x + col * (base_w + gap) + (col < rem_w ? col : rem_w);
      int y = content_y + row * (base_h + gap) + (row < rem_h ? row : rem_h);
      int outer_x = x;
      int outer_y = y;
      int outer_w = (el->h_align == LAYOUT_ALIGN_STRETCH) ? cell_w : MIN(ow, cell_w);
      int outer_h = (el->v_align == LAYOUT_ALIGN_STRETCH) ? cell_h : MIN(oh, cell_h);
      if (el->h_align == LAYOUT_ALIGN_CENTER)
        outer_x += (cell_w - outer_w) / 2;
      else if (el->h_align == LAYOUT_ALIGN_END)
        outer_x += cell_w - outer_w;
      if (el->v_align == LAYOUT_ALIGN_CENTER)
        outer_y += (cell_h - outer_h) / 2;
      else if (el->v_align == LAYOUT_ALIGN_END)
        outer_y += cell_h - outer_h;
      el->frame = (irect16_t){
        outer_x + margin.x,
        outer_y + margin.y,
        MAX(1, outer_w - margin.x - margin.w),
        MAX(1, outer_h - margin.y - margin.h)
      };
    }
    return;
  }

  // Stack layout - horizontal
  if (doc->flags & WINDOW_STACK_HORIZONTAL) {
    int x = content_x;
    for (int i = 0; i < count; i++) {
      form_element_t *el = roots[i];
      irect16_t margin = el->margin;
      if (i > 0) x += gap;
      int inner_w = el->frame.w > 0 ? el->frame.w : 1;
      int inner_h = el->frame.h > 0 ? el->frame.h : 1;
      int ow = inner_w + margin.x + margin.w;
      int oh = inner_h + margin.y + margin.h;
      int outer_y = content_y;
      if (el->v_align == LAYOUT_ALIGN_STRETCH) {
        oh = max_h;
      } else {
        if (oh > max_h) oh = max_h;
        if (el->v_align == LAYOUT_ALIGN_CENTER) outer_y = content_y + (max_h - oh) / 2;
        else if (el->v_align == LAYOUT_ALIGN_END) outer_y = content_y + max_h - oh;
      }
      el->frame = (irect16_t){
        x + margin.x,
        outer_y + margin.y,
        MAX(1, ow - margin.x - margin.w),
        MAX(1, oh - margin.y - margin.h)
      };
      x += ow;
    }
  } else {
    // Stack layout - vertical
    int y = content_y;
    for (int i = 0; i < count; i++) {
      form_element_t *el = roots[i];
      irect16_t margin = el->margin;
      if (i > 0) y += gap;
      int inner_w = el->frame.w > 0 ? el->frame.w : 1;
      int inner_h = el->frame.h > 0 ? el->frame.h : 1;
      int ow = inner_w + margin.x + margin.w;
      int oh = inner_h + margin.y + margin.h;
      int outer_x = content_x;
      if (el->h_align == LAYOUT_ALIGN_STRETCH) {
        ow = max_w;
      } else {
        if (ow > max_w) ow = max_w;
        if (el->h_align == LAYOUT_ALIGN_CENTER) outer_x = content_x + (max_w - ow) / 2;
        else if (el->h_align == LAYOUT_ALIGN_END) outer_x = content_x + max_w - ow;
      }
      el->frame = (irect16_t){
        outer_x + margin.x,
        y + margin.y,
        MAX(1, ow - margin.x - margin.w),
        MAX(1, oh - margin.y - margin.h)
      };
      y += oh;
    }
  }
}
