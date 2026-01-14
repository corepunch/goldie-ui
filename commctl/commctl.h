#ifndef __UI_COMMCTL_H__
#define __UI_COMMCTL_H__

#include "../user/user.h"
#include "console.h"

// Common control window procedures
result_t win_button(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_checkbox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_combobox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_textedit(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_label(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_list(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_console(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_space(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

#endif
