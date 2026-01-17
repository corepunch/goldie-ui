#ifndef __LUA_TERMINAL_H__
#define __LUA_TERMINAL_H__

#include "../ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua5.4/lua.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>

#define DEFAULT_TEXT_BUFFER_SIZE 4096
#define TEXTBUF(L) ((text_buffer_t**)lua_getextraspace(L))

#define ICON_CURSOR 8

typedef struct text_buffer_s {
  size_t size;
  size_t capacity;
  char data[];
} text_buffer_t;

typedef struct {
  lua_State *L;          // Main Lua state
  lua_State *co;         // Coroutine for script execution
  text_buffer_t *textbuf;
  char input_buffer[256];
  bool waiting_for_input;
} terminal_state_t;

void init_text_buffer(text_buffer_t **buf) {
  *buf = malloc(sizeof(text_buffer_t) + DEFAULT_TEXT_BUFFER_SIZE);
  (*buf)->size = 0;
  (*buf)->capacity = DEFAULT_TEXT_BUFFER_SIZE;
  (*buf)->data[0] = '\0';
}

void free_text_buffer(text_buffer_t **buf) {
  free(*buf);
  *buf = NULL;
}

void f_strcat(text_buffer_t **b, const char *s) {
  size_t l = strlen(s);
  if ((*b)->size + l + 1 > (*b)->capacity) {
    size_t c = (*b)->capacity;
    while (c < (*b)->size + l + 1) c <<= 1;
    *b = realloc(*b, sizeof(text_buffer_t) + c);
    (*b)->capacity = c;
  }
  strcpy((*b)->data + (*b)->size, s);
  (*b)->size += l;
}

int f_print(lua_State *L) {
  const char *msg = lua_tostring(L, -1);
  if (msg) {
    f_strcat(TEXTBUF(L), msg);
    f_strcat(TEXTBUF(L), "\n");
  }
  return 0;
}

int f_io_read(lua_State *L) {
  // This function yields the coroutine, giving control back to the event loop
  // The user can then type input, which will be provided when the coroutine resumes
  return lua_yield(L, 0);
}

lua_State *create_lua_state(text_buffer_t **textbuf) {
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  
  init_text_buffer(textbuf);
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

void continue_coroutine(terminal_state_t *state, int nargs) {
  int nres, status = lua_resume(state->co, NULL, nargs, &nres);
  
  if (status == LUA_OK) {
    f_strcat(&state->textbuf, "\nProcess finished\n");
    state->waiting_for_input = false;
  } else if (status == LUA_YIELD) {
    state->waiting_for_input = true;
    f_strcat(&state->textbuf, "\n> ");
  } else {
    f_strcat(&state->textbuf, "Error: ");
    f_strcat(&state->textbuf, lua_tostring(state->co, -1));
    f_strcat(&state->textbuf, "\n");
    state->waiting_for_input = false;
  }
}

result_t win_terminal(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  terminal_state_t *state = (terminal_state_t *)win->userdata;
  
  switch (msg) {
    case WM_CREATE: {
      state = allocate_window_data(win, sizeof(terminal_state_t));
      state->L = create_lua_state(&state->textbuf);
      state->co = lua_newthread(state->L); // Create coroutine for script execution
      state->waiting_for_input = false;
      state->input_buffer[0] = '\0';
      
      // Load the file into the coroutine
      if (luaL_loadfile(state->co, lparam) != LUA_OK) {
        f_strcat(&state->textbuf, "Error loading file: ");
        f_strcat(&state->textbuf, lua_tostring(state->co, -1));
        f_strcat(&state->textbuf, "\n");
        return true;
      }
      
      // Start executing the coroutine
      continue_coroutine(state, 0);
      
      return true;
    }
    case WM_KEYDOWN:
      if (!state->waiting_for_input) {
        return false;
      } else if (wparam == SDL_SCANCODE_RETURN) {
        // User pressed Enter - resume coroutine with input
        f_strcat(&state->textbuf, state->input_buffer);
        f_strcat(&state->textbuf, "\n");
        
        // Push the input as return value for io.read
        lua_pushstring(state->co, state->input_buffer);
        
        // Resume the coroutine with 1 argument (the input string)
        continue_coroutine(state, 1);
        
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
    case WM_TEXTINPUT:
      if (!state->waiting_for_input) {
        return false;
      } else {
        size_t len = strlen(state->input_buffer);
        if (len < sizeof(state->input_buffer) - 1) {
          state->input_buffer[len] = *(char*)lparam;
          state->input_buffer[len + 1] = '\0';
        }
        invalidate_window(win);
        return true;
      }
    
    case WM_DESTROY:
      if (state) {
        free_text_buffer(&state->textbuf);
        lua_close(state->L);
        free(state);
      }
      return true;
      
    case WM_PAINT: {
      if (!state) return false;
      
      // Draw terminal contents
      draw_text_small(state->textbuf->data, WINDOW_PADDING, WINDOW_PADDING, COLOR_TEXT_NORMAL);
      
      if (state->waiting_for_input) {
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

#endif // __LUA_TERMINAL_H__
