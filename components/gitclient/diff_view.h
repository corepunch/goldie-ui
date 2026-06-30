#ifndef GC_DIFF_VIEW_H
#define GC_DIFF_VIEW_H

#include "../../user/vga_text.h"
#include "../../ui.h"

#define GC_DIFF_VIEW_CLASS_NAME "DiffView"

#define GC_DIFF_BUF_SIZE (256 * 1024)

typedef struct {
  char  **lines;
  int     line_count;
  int     scroll_y;
  vga_text_grid_t grid;
  char    diff_buf[GC_DIFF_BUF_SIZE];
} gc_diff_state_t;

result_t gc_diff_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

#endif
