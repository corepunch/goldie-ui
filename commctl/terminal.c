#include <SDL2/SDL.h>
#include "../user/gl_compat.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "commctl.h"
#include "../user/text.h"
#include "../user/user.h"
#include "../user/messages.h"

/* Lua headers - different paths on Windows vs Unix */
#if defined(_WIN32) || defined(_WIN64)
  #include <lua.h>
  #include <lauxlib.h>
  #include <lualib.h>
#else
  #include <lua5.4/lua.h>
  #include <lua5.4/lauxlib.h>
  #include <lua5.4/lualib.h>
#endif

#define DEFAULT_TEXT_BUFFER_SIZE 4096
#define TEXTBUF(L) ((text_buffer_t**)lua_getextraspace(L))

#define ICON_CURSOR 8

typedef struct text_buffer_s {
  size_t size;
  size_t capacity;
  char data[];
} text_buffer_t;

// Forward declarations
typedef struct terminal_state_s terminal_state_t;
typedef void (*terminal_cmd_func_t)(terminal_state_t *);

// Terminal command structure
typedef struct {
  const char *name;
  const char *help;
  terminal_cmd_func_t callback;
} terminal_cmd_t;

typedef struct terminal_state_s {
  lua_State *L;          // Main Lua state (NULL if in command mode)
  lua_State *co;         // Coroutine for script execution (NULL if in command mode)
  text_buffer_t *textbuf;
  char input_buffer[256];
  bool waiting_for_input;
  bool process_finished;
  bool command_mode;     // True if running in command mode (no Lua script)
} terminal_state_t;

extern void draw_icon8(int icon, int x, int y, uint32_t col);

static void init_text_buffer(text_buffer_t **buf) {
  *buf = malloc(sizeof(text_buffer_t) + DEFAULT_TEXT_BUFFER_SIZE);
  if (!*buf) {
    return;  // Allocation failed
  }
  (*buf)->size = 0;
  (*buf)->capacity = DEFAULT_TEXT_BUFFER_SIZE;
  (*buf)->data[0] = '\0';
}

static void free_text_buffer(text_buffer_t **buf) {
  free(*buf);
  *buf = NULL;
}

static void f_strcat(text_buffer_t **b, const char *s) {
  if (!*b || !s) return;  // Safety check
  
  size_t l = strlen(s);
  if ((*b)->size + l + 1 > (*b)->capacity) {
    size_t c = (*b)->capacity;
    while (c < (*b)->size + l + 1) c <<= 1;
    text_buffer_t *new_buf = realloc(*b, sizeof(text_buffer_t) + c);
    if (!new_buf) {
      return;  // Realloc failed, keep old buffer
    }
    *b = new_buf;
    (*b)->capacity = c;
  }
  strcpy((*b)->data + (*b)->size, s);
  (*b)->size += l;
}

static int f_print(lua_State *L) {
  const char *msg = lua_tostring(L, -1);
  if (msg) {
    f_strcat(TEXTBUF(L), msg);
    f_strcat(TEXTBUF(L), "\n");
  }
  return 0;
}

static int f_io_read(lua_State *L) {
  // This function yields the coroutine, giving control back to the event loop
  // The user can then type input, which will be provided when the coroutine resumes
  return lua_yield(L, 0);
}

static lua_State *create_lua_state(text_buffer_t **textbuf) {
  lua_State *L = luaL_newstate();
  if (!L) {
    return NULL;  // Lua state creation failed
  }
  luaL_openlibs(L);
  
  init_text_buffer(textbuf);
  if (!*textbuf) {
    lua_close(L);
    return NULL;  // Text buffer allocation failed
  }
  
  // Store text buffer pointer in extra space
  *(text_buffer_t **)lua_getextraspace(L) = *textbuf;
  
  // Override print function
  lua_pushcfunction(L, f_print);
  lua_setglobal(L, "print");
  
  // Override io.read function
  lua_getglobal(L, "io");
  lua_pushcfunction(L, f_io_read);
  lua_setfield(L, -2, "read");
  lua_pop(L, 1); // pop io table
  
  return L;
}

static void continue_coroutine(terminal_state_t *state, int nargs) {
  int nres, status = lua_resume(state->co, NULL, nargs, &nres);
  
  if (status == LUA_OK) {
    f_strcat(&state->textbuf, "\nProcess finished\n");
    state->waiting_for_input = false;
    state->process_finished = true;  // Mark process as finished
  } else if (status == LUA_YIELD) {
    state->waiting_for_input = true;
    f_strcat(&state->textbuf, "\n> ");
  } else {
    f_strcat(&state->textbuf, "Error: ");
    f_strcat(&state->textbuf, lua_tostring(state->co, -1));
    f_strcat(&state->textbuf, "\n");
    state->waiting_for_input = false;
    state->process_finished = true;  // Mark process as finished on error
  }
}

// Command mode functions
static void cmd_exit(terminal_state_t *state) {
  f_strcat(&state->textbuf, "Exiting terminal...\n");
  state->process_finished = true;
  state->waiting_for_input = false;
}

// Forward declaration
static const terminal_cmd_t terminal_commands[];

static void cmd_help(terminal_state_t *state) {
  f_strcat(&state->textbuf, "Available commands:\n");
  for (int i = 0; terminal_commands[i].name != NULL; i++) {
    f_strcat(&state->textbuf, "  ");
    f_strcat(&state->textbuf, terminal_commands[i].name);
    f_strcat(&state->textbuf, " - ");
    f_strcat(&state->textbuf, terminal_commands[i].help);
    f_strcat(&state->textbuf, "\n");
  }
}

static void cmd_clear(terminal_state_t *state) {
  // Clear the text buffer
  if (state->textbuf) {
    state->textbuf->size = 0;
    state->textbuf->data[0] = '\0';
  }
  f_strcat(&state->textbuf, "Terminal> ");
}

// Static array of available commands
static const terminal_cmd_t terminal_commands[] = {
  {"exit", "Closes current terminal instance", cmd_exit},
  {"help", "Lists available commands", cmd_help},
  {"clear", "Clears the terminal screen", cmd_clear},
  {NULL, NULL, NULL}  // Sentinel
};

static void process_command(terminal_state_t *state, const char *cmd) {
  if (!cmd || !state) return;  // Safety check
  
  // Trim leading/trailing whitespace
  while (*cmd == ' ' || *cmd == '\t') cmd++;
  
  if (strlen(cmd) == 0) {
    f_strcat(&state->textbuf, "Terminal> ");
    return;
  }
  
  // Find and execute command
  bool found = false;
  for (int i = 0; terminal_commands[i].name != NULL; i++) {
    if (strcmp(cmd, terminal_commands[i].name) == 0) {
      terminal_commands[i].callback(state);
      found = true;
      break;
    }
  }
  
  if (!found) {
    f_strcat(&state->textbuf, "Unknown command: ");
    f_strcat(&state->textbuf, cmd);
    f_strcat(&state->textbuf, "\nType 'help' for a list of commands.\n");
  }
  
  // Show prompt again if not finished
  if (!state->process_finished) {
    f_strcat(&state->textbuf, "Terminal> ");
  }
}

// Public API: Get terminal buffer content
// This function allows external code (including tests) to retrieve the current
// terminal output buffer. It safely handles null pointers and invalid window types.
const char* terminal_get_buffer(window_t *win) {
  if (!win || !win->userdata) {
    return "";
  }
  
  // Verify this is a terminal window by checking the window procedure
  if (win->proc != win_terminal) {
    return "";
  }
  
  terminal_state_t *state = (terminal_state_t *)win->userdata;
  if (!state || !state->textbuf) {
    return "";
  }
  
  return state->textbuf->data;
}

result_t win_terminal(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  terminal_state_t *state = (terminal_state_t *)win->userdata;
  
  switch (msg) {
    case kWindowMessageCreate: {
      state = allocate_window_data(win, sizeof(terminal_state_t));
      if (!state) {
        return false;
      }
      
      // Check if lparam is NULL - if so, enter command mode
      if (lparam == NULL) {
        // Command mode - no Lua script
        state->L = NULL;
        state->co = NULL;
        state->command_mode = true;
        state->waiting_for_input = true;
        state->process_finished = false;
        state->input_buffer[0] = '\0';
        
        // Initialize text buffer manually for command mode
        init_text_buffer(&state->textbuf);
        if (!state->textbuf) {
          return false;
        }
        
        // Display welcome message
        f_strcat(&state->textbuf, "Terminal - Command Mode\n");
        f_strcat(&state->textbuf, "Type 'help' for available commands\n");
        f_strcat(&state->textbuf, "Terminal> ");
      } else {
        // Lua script mode
        state->command_mode = false;
        state->L = create_lua_state(&state->textbuf);
        if (!state->L) {
          // Lua state creation failed
          return false;
        }
        state->co = lua_newthread(state->L); // Create coroutine for script execution
        state->waiting_for_input = false;
        state->process_finished = false;
        state->input_buffer[0] = '\0';
        
        // Load the file into the coroutine
        if (luaL_loadfile(state->co, lparam) != LUA_OK) {
          f_strcat(&state->textbuf, "Error loading file: ");
          f_strcat(&state->textbuf, lua_tostring(state->co, -1));
          f_strcat(&state->textbuf, "\n");
          state->process_finished = true;
          return true;
        }
        
        // Start executing the coroutine
        continue_coroutine(state, 0);
      }
      
      return true;
    }
    case kWindowMessageKeyDown:
      // Don't accept input if process is finished
      if (state->process_finished || !state->waiting_for_input) {
        return false;
      } else if (wparam == SDL_SCANCODE_RETURN) {
        // User pressed Enter
        f_strcat(&state->textbuf, state->input_buffer);
        f_strcat(&state->textbuf, "\n");
        
        if (state->command_mode) {
          // Command mode - process command
          process_command(state, state->input_buffer);
        } else if (state->co) {
          // Lua mode - push the input as return value for io.read
          lua_pushstring(state->co, state->input_buffer);
          // Resume the coroutine with 1 argument (the input string)
          continue_coroutine(state, 1);
        }
        
        // Clear input buffer
        state->input_buffer[0] = '\0';
        invalidate_window(win);
        return true;
      } else if (wparam == SDL_SCANCODE_BACKSPACE) {
        // Handle backspace
        size_t len = strlen(state->input_buffer);
        if (len > 0) {
          state->input_buffer[len - 1] = '\0';
          invalidate_window(win);
        }
        return true;
      } else {
        return false;
      }
    case kWindowMessageTextInput:
      // Don't accept input if process is finished
      if (state->process_finished || !state->waiting_for_input) {
        return false;
      } else {
        // Validate that lparam contains a valid printable character
        char c = *(char*)lparam;
        if (c < 32 || c > 126) {
          // Not a printable ASCII character, ignore
          return false;
        }
        size_t len = strlen(state->input_buffer);
        if (len < sizeof(state->input_buffer) - 1) {
          state->input_buffer[len] = c;
          state->input_buffer[len + 1] = '\0';
        }
        invalidate_window(win);
        return true;
      }
    
    case kWindowMessageDestroy:
      if (state) {
        free_text_buffer(&state->textbuf);
        if (state->L) {
          lua_close(state->L);
        }
        free(state);
        win->userdata = NULL;  // Prevent use-after-free
      }
      return true;
      
    case kWindowMessagePaint: {
      if (!state) return false;
      
      // Draw terminal contents
      draw_text_small(state->textbuf->data, WINDOW_PADDING, WINDOW_PADDING, COLOR_TEXT_NORMAL);
      
      // Only show input prompt if waiting for input AND process not finished
      if (state->waiting_for_input && !state->process_finished) {
        // Draw input buffer at the bottom of the window
        int y = win->frame.h - WINDOW_PADDING - CHAR_HEIGHT; // Position near bottom
        draw_text_small(state->input_buffer, WINDOW_PADDING, y, COLOR_TEXT_NORMAL);
        draw_icon8(ICON_CURSOR, WINDOW_PADDING + strwidth(state->input_buffer), y, COLOR_TEXT_NORMAL);
      }
      
      return true;
    }
    
    default:
      return false;
  }
}
