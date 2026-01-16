#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "columnview.h"
#include "../user/user.h"
#include "../user/messages.h"
#include "../user/draw.h"

#define MAX_COLUMNVIEW_ITEMS 256
#define ENTRY_HEIGHT 13
#define DEFAULT_COLUMN_WIDTH 160
#define ICON_OFFSET 12
#define ICON_DODGE 1
#define WIN_PADDING 4

// ColumnView data structure
typedef struct {
  columnview_item_t items[MAX_COLUMNVIEW_ITEMS];
  int count;
  int selected;
  int column_width;
  uint32_t last_click_time;
  int last_click_index;
} columnview_data_t;

// Calculate number of columns that fit in window
static inline int get_column_count(int window_width, int column_width) {
  if (window_width <= 0 || column_width <= 0) {
    return 1;
  }
  int ncol = window_width / column_width;
  return (ncol > 0) ? ncol : 1;
}

// ColumnView control window procedure
result_t win_columnview(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  columnview_data_t *data = (columnview_data_t *)win->userdata;
  
  switch (msg) {
    case WM_CREATE: {
      data = malloc(sizeof(columnview_data_t));
      if (!data) return false;
      win->userdata = data;
      data->count = 0;
      data->selected = -1;
      data->column_width = DEFAULT_COLUMN_WIDTH;
      data->last_click_time = 0;
      data->last_click_index = -1;
      return true;
    }
    
    case WM_PAINT: {
      const int ncol = get_column_count(win->frame.w, data->column_width);
      
      for (int i = 0; i < data->count; i++) {
        int col = i % ncol;
        int x = col * data->column_width + WIN_PADDING;
        int y = (i / ncol) * ENTRY_HEIGHT + WIN_PADDING;
        
        set_clip_rect(win, &(rect_t){x - 2, y - 2, data->column_width - 6, ENTRY_HEIGHT - 2});
        
        if (i == data->selected) {
          fill_rect(COLOR_TEXT_NORMAL, x - 2, y - 2, data->column_width - 6, ENTRY_HEIGHT - 2);
          draw_icon8(data->items[i].icon, x, y - ICON_DODGE, COLOR_PANEL_BG);
          draw_text_small(data->items[i].text, x + ICON_OFFSET, y, COLOR_PANEL_BG);
        } else {
          draw_icon8(data->items[i].icon, x, y - ICON_DODGE, data->items[i].color);
          draw_text_small(data->items[i].text, x + ICON_OFFSET, y, data->items[i].color);
        }
      }
      
      return false;
    }
    
    case WM_LBUTTONDOWN: {
      int mx = LOWORD(wparam);
      int my = HIWORD(wparam);
      const int ncol = get_column_count(win->frame.w, data->column_width);
      int col = mx / data->column_width;
      int row = (my - WIN_PADDING) / ENTRY_HEIGHT;
      int index = row * ncol + col;
      
      if (index >= 0 && index < data->count) {
        uint32_t now = SDL_GetTicks();
        
        // Check for double-click
        if (data->last_click_index == index && (now - data->last_click_time) < 500) {
          // Send double-click notification
          send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, CVN_DBLCLK), (void*)(intptr_t)index);
          data->last_click_time = 0;
          data->last_click_index = -1;
        } else {
          // Single click - update selection
          int old_selection = data->selected;
          data->selected = index;
          data->last_click_time = now;
          data->last_click_index = index;
          
          // Send selection change notification if changed
          if (old_selection != data->selected) {
            send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, CVN_SELCHANGE), (void*)(intptr_t)index);
          }
          
          invalidate_window(win);
        }
      }
      return true;
    }
    
    case CVM_ADDITEM: {
      columnview_item_t *item = (columnview_item_t *)lparam;
      if (data->count < MAX_COLUMNVIEW_ITEMS && item) {
        strncpy(data->items[data->count].text, item->text, sizeof(data->items[data->count].text) - 1);
        data->items[data->count].text[sizeof(data->items[data->count].text) - 1] = '\0';
        data->items[data->count].icon = item->icon;
        data->items[data->count].color = item->color;
        data->items[data->count].userdata = item->userdata;
        data->count++;
        invalidate_window(win);
        return data->count - 1; // Return index of added item
      }
      return -1;
    }
    
    case CVM_DELETEITEM: {
      int index = (int)wparam;
      if (index >= 0 && index < data->count) {
        // Shift items down
        for (int i = index; i < data->count - 1; i++) {
          data->items[i] = data->items[i + 1];
        }
        data->count--;
        
        // Adjust selection
        if (data->selected == index) {
          data->selected = -1;
        } else if (data->selected > index) {
          data->selected--;
        }
        
        invalidate_window(win);
        return true;
      }
      return false;
    }
    
    case CVM_GETITEMCOUNT:
      return data->count;
    
    case CVM_GETSELECTION:
      return data->selected;
    
    case CVM_SETSELECTION: {
      int index = (int)wparam;
      if (index >= -1 && index < data->count) {
        data->selected = index;
        invalidate_window(win);
        return true;
      }
      return false;
    }
    
    case CVM_CLEAR:
      data->count = 0;
      data->selected = -1;
      data->last_click_time = 0;
      data->last_click_index = -1;
      invalidate_window(win);
      return true;
    
    case CVM_SETCOLUMNWIDTH: {
      int width = (int)wparam;
      if (width > 0) {
        data->column_width = width;
        invalidate_window(win);
        return true;
      }
      return false;
    }
    
    case CVM_GETCOLUMNWIDTH:
      return data->column_width;
    
    case CVM_GETITEMDATA: {
      int index = (int)wparam;
      if (index >= 0 && index < data->count) {
        columnview_item_t *dest = (columnview_item_t *)lparam;
        if (dest) {
          *dest = data->items[index];
          return true;
        }
      }
      return false;
    }
    
    case CVM_SETITEMDATA: {
      int index = (int)wparam;
      columnview_item_t *item = (columnview_item_t *)lparam;
      if (index >= 0 && index < data->count && item) {
        strncpy(data->items[index].text, item->text, sizeof(data->items[index].text) - 1);
        data->items[index].text[sizeof(data->items[index].text) - 1] = '\0';
        data->items[index].icon = item->icon;
        data->items[index].color = item->color;
        data->items[index].userdata = item->userdata;
        invalidate_window(win);
        return true;
      }
      return false;
    }
    
    case WM_DESTROY:
      if (data) {
        free(data);
      }
      return true;
    
    default:
      return false;
  }
}
