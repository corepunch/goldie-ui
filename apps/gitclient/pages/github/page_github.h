#ifndef __PAGE_GITHUB_H__
#define __PAGE_GITHUB_H__

#include "../../gitclient.h"

result_t  page_github_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
bool      page_github_handle(window_t *main_win, uint32_t msg, uint32_t wparam, void *lparam);
void      page_github_refresh(void);

#endif
