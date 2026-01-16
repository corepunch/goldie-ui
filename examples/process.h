#ifndef __LUA_TERMINAL_H__
#define __LUA_TERMINAL_H__

#include "../ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>

#define DEFAULT_TEXT_BUFFER_SIZE 4096
#define TEXTBUF(L) ((text_buffer_t**)lua_getextraspace(L))

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
  }
  return 0;
}

int f_io_read(lua_State *L) {
  // This function yields the coroutine, giving control back to the event loop
  // The user can then type input, which will be provided when the coroutine resumes
  return lua_yield(L, 0);
}

result_t win_terminal(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  terminal_state_t *state = (terminal_state_t *)win->userdata;
  
  switch (msg) {
    case WM_CREATE: {
      state = malloc(sizeof(terminal_state_t));
      win->userdata = state;
      
      // Create main Lua state
      state->L = luaL_newstate();
      init_text_buffer(&state->textbuf);
      *TEXTBUF(state->L) = state->textbuf;
      
      f_strcat(&state->textbuf, "Terminal initialized\n");
      luaL_openlibs(state->L);
      
      // Set up custom print function
      lua_pushlightuserdata(state->L, win);
      lua_pushcclosure(state->L, f_print, 1);
      lua_setglobal(state->L, "print");
      
      // Override io.read with our coroutine-yielding version
      lua_getglobal(state->L, "io");
      lua_pushlightuserdata(state->L, win);
      lua_pushcclosure(state->L, f_io_read, 1);
      lua_setfield(state->L, -2, "read");
      lua_pop(state->L, 1);
      
      // Create coroutine for script execution
      state->co = lua_newthread(state->L);
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
      int nres;
      int status = lua_resume(state->co, NULL, 0, &nres);
      
      if (status == LUA_OK) {
        f_strcat(&state->textbuf, "\nProcess finished\n");
      } else if (status == LUA_YIELD) {
        state->waiting_for_input = true;
        f_strcat(&state->textbuf, "\n> ");
      } else {
        f_strcat(&state->textbuf, "Error: ");
        f_strcat(&state->textbuf, lua_tostring(state->co, -1));
        f_strcat(&state->textbuf, "\n");
      }
      
      return true;
    }
    
    case WM_KEYDOWN: {
      if (!state->waiting_for_input) return false;
      
      char c = (char)wparam;
      if (c == '\r' || c == '\n') {
        // User pressed Enter - resume coroutine with input
        f_strcat(&state->textbuf, state->input_buffer);
        f_strcat(&state->textbuf, "\n");
        
        // Push the input as return value for io.read
        lua_pushstring(state->co, state->input_buffer);
        
        // Resume the coroutine
        int nres;
        int status = lua_resume(state->co, NULL, 1, &nres);
        
        if (status == LUA_OK) {
          f_strcat(&state->textbuf, "\nProcess finished\n");
          state->waiting_for_input = false;
        } else if (status == LUA_YIELD) {
          // Still waiting for more input
          f_strcat(&state->textbuf, "> ");
        } else {
          f_strcat(&state->textbuf, "Error: ");
          f_strcat(&state->textbuf, lua_tostring(state->co, -1));
          f_strcat(&state->textbuf, "\n");
          state->waiting_for_input = false;
        }
        
        // Clear input buffer
        state->input_buffer[0] = '\0';
        return true;
      } else if (c == '\b' || c == 127) {
        // Backspace
        size_t len = strlen(state->input_buffer);
        if (len > 0) {
          state->input_buffer[len - 1] = '\0';
        }
        return true;
      } else if (c >= 32 && c < 127) {
        // Printable character
        size_t len = strlen(state->input_buffer);
        if (len < sizeof(state->input_buffer) - 1) {
          state->input_buffer[len] = c;
          state->input_buffer[len + 1] = '\0';
        }
        return true;
      }
      return false;
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
      char display[8192];
      snprintf(display, sizeof(display), "%s%s", 
        state->textbuf->data,
        state->waiting_for_input ? state->input_buffer : "");
      
      draw_text_small(display, WINDOW_PADDING, WINDOW_PADDING, COLOR_TEXT_NORMAL);
      return true;
    }
    
    default:
      return false;
  }
}

#endif // __LUA_TERMINAL_H__
