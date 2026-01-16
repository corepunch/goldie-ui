#ifndef __UI_COLUMNVIEW_H__
#define __UI_COLUMNVIEW_H__

#include <stdint.h>
#include "../user/user.h"

// ColumnView messages
enum {
  CVM_ADDITEM = WM_USER + 100,
  CVM_DELETEITEM,
  CVM_GETITEMCOUNT,
  CVM_GETSELECTION,
  CVM_SETSELECTION,
  CVM_CLEAR,
  CVM_SETCOLUMNWIDTH,
  CVM_GETCOLUMNWIDTH,
  CVM_GETITEMDATA,
  CVM_SETITEMDATA,
};

// ColumnView notification messages
enum {
  CVN_SELCHANGE = 200,
  CVN_DBLCLK,
};

// ColumnView item structure
typedef struct {
  char text[256];
  int icon;
  uint32_t color;
  void *userdata;
} columnview_item_t;

#endif // __UI_COLUMNVIEW_H__
