// Project-level document management for Form Editor.
// Owns document lifecycle, activation, and window frame calculation.

#ifndef __FE_PROJECT_H__
#define __FE_PROJECT_H__

#include "fe_document.h"

// Document lifecycle
form_doc_t *create_form_doc(int w, int h);
void        close_form_doc(form_doc_t *doc);

// Document state management
void form_doc_update_title(form_doc_t *doc);
void form_doc_activate(form_doc_t *doc);
void form_doc_show_only(form_doc_t *doc);

// Document layout/sizing helpers
irect16_t form_doc_frame_for_size(int form_w, int form_h, uint32_t form_flags);
void      form_doc_auto_layout_reflow(form_doc_t *doc);

// Document window procedure
result_t doc_win_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

#endif // __FE_PROJECT_H__
