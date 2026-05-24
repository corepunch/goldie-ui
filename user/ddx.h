#ifndef __UI_DDX_H__
#define __UI_DDX_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "messages.h"

typedef struct window_s window_t;

// Dialog Data Exchange (DDX)
//
// Analogous to MFC DDX / WinAPI dialog-data routines. Describe each
// control-to-field mapping in a static ctrl_binding_t array, then call
// dialog_push() on create and dialog_pull() on accept.

// Returns the number of elements in a statically-sized array.
#ifndef ARRAY_LEN
#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#endif

// Returns sizeof(((type *)0)->field): the byte size of a struct field.
#ifndef sizeof_field
#define sizeof_field(type, field) ((size_t)(sizeof(((type *)0)->field)))
#endif

typedef struct ctrl_binding_s ctrl_binding_t;
typedef void (*ddx_bind_push_fn)(window_t *dlg, const ctrl_binding_t *binding,
                                 const void *state);
typedef void (*ddx_bind_pull_fn)(window_t *dlg, const ctrl_binding_t *binding,
                                 void *state);

typedef struct ctrl_binding_s {
  uint32_t    ctrl_id; // numeric child control ID
  uint16_t    command; // evCommand notification to listen for (HIWORD(wparam)); 0 = any
  uint32_t    getter;  // control getter message for message-based bindings (edGetText, cbGetCurrentSelection, etc.)
  size_t      offset;  // offsetof(state_t, field)
  size_t      wparam;  // getter message wparam (edGetText: buffer size; cbGetCurrentSelection: default index when selection < 0)
  ddx_bind_push_fn push; // optional push callback (state -> control)
  ddx_bind_pull_fn pull; // optional pull callback (control -> state)
} ctrl_binding_t;

// Built-in DDX callbacks for common scalar/text fields.
void ddx_push_int(window_t *dlg, const ctrl_binding_t *b, const void *state);
void ddx_pull_int(window_t *dlg, const ctrl_binding_t *b, void *state);
void ddx_push_float(window_t *dlg, const ctrl_binding_t *b, const void *state);
void ddx_pull_float(window_t *dlg, const ctrl_binding_t *b, void *state);
void ddx_push_u8(window_t *dlg, const ctrl_binding_t *b, const void *state);
void ddx_pull_u8(window_t *dlg, const ctrl_binding_t *b, void *state);
void ddx_push_text(window_t *dlg, const ctrl_binding_t *b, const void *state);
void ddx_pull_text(window_t *dlg, const ctrl_binding_t *b, void *state);
void ddx_push_combo(window_t *dlg, const ctrl_binding_t *b, const void *state);
void ddx_pull_combo(window_t *dlg, const ctrl_binding_t *b, void *state);
void ddx_push_check(window_t *dlg, const ctrl_binding_t *b, const void *state);
void ddx_pull_check(window_t *dlg, const ctrl_binding_t *b, void *state);

// DDX_TEXT binds a textedit control; _Generic dispatches push/pull by field type.
//   int field           -> ddx_push_int   / ddx_pull_int
//   float field         -> ddx_push_float / ddx_pull_float
//   unsigned char field -> ddx_push_u8    / ddx_pull_u8
//   char[] / other      -> ddx_push_text  / ddx_pull_text
#define DDX_TEXT(id_, state_type, field) \
  (ctrl_binding_t){ \
    .ctrl_id = (id_), \
    .command = edUpdate, \
    .getter  = 0, \
    .offset  = offsetof(state_type, field), \
    .wparam  = sizeof_field(state_type, field), \
    .push = _Generic((((state_type *)0)->field), \
      int: ddx_push_int, \
      float: ddx_push_float, \
      unsigned char: ddx_push_u8, \
      default: ddx_push_text), \
    .pull = _Generic((((state_type *)0)->field), \
      int: ddx_pull_int, \
      float: ddx_pull_float, \
      unsigned char: ddx_pull_u8, \
      default: ddx_pull_text), \
  }

// DDX_COMBO binds a combobox control; field must be int (compile error otherwise).
// default_idx is used when combobox has no valid current selection.
#define DDX_COMBO(id_, state_type, field, default_idx) \
  (ctrl_binding_t){ \
    .ctrl_id = (id_), \
    .command = cbSelectionChange, \
    .getter  = 0, \
    .offset  = offsetof(state_type, field), \
    .wparam  = (default_idx), \
    .push = _Generic((((state_type *)0)->field), int: ddx_push_combo), \
    .pull = _Generic((((state_type *)0)->field), int: ddx_pull_combo), \
  }

// DDX_CHECK binds a checkbox control; field must be bool or int (compile error otherwise).
#define DDX_CHECK(id_, state_type, field) \
  (ctrl_binding_t){ \
    .ctrl_id = (id_), \
    .command = btnClicked, \
    .getter  = 0, \
    .offset  = offsetof(state_type, field), \
    .wparam  = 0, \
    .push = _Generic((((state_type *)0)->field), bool: ddx_push_check, int: ddx_push_check), \
    .pull = _Generic((((state_type *)0)->field), bool: ddx_pull_check, int: ddx_pull_check), \
  }

#endif // __UI_DDX_H__
