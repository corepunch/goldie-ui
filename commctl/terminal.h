#ifndef __UI_TERMINAL_H__
#define __UI_TERMINAL_H__

#include "../user/user.h"

// Terminal window procedure
// Creates an interactive terminal that runs Lua scripts
// When a script finishes, the terminal becomes read-only (like Windows CMD)
// 
// lparam in WM_CREATE should be a const char* path to the Lua script file
result_t win_terminal(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

#endif // __UI_TERMINAL_H__
