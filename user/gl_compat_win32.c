/* OpenGL extension loading for Windows */
#if defined(_WIN32) || defined(_WIN64)

#include "gl_compat.h"
#include <stdio.h>

/* Define the function pointers */
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = NULL;
PFNGLGENBUFFERSPROC glGenBuffers = NULL;
PFNGLBINDBUFFERPROC glBindBuffer = NULL;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = NULL;
PFNGLBUFFERDATAPROC glBufferData = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;

/* Load OpenGL extensions using wglGetProcAddress */
void load_gl_extensions_win32(void) {
  glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
  glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
  glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)wglGetProcAddress("glDeleteVertexArrays");
  glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
  glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
  glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)wglGetProcAddress("glDeleteBuffers");
  glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
  glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glEnableVertexAttribArray");
  glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glDisableVertexAttribArray");
  glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");
  
  /* Verify that all functions were loaded successfully */
  if (!glGenVertexArrays || !glBindVertexArray || !glDeleteVertexArrays ||
      !glGenBuffers || !glBindBuffer || !glDeleteBuffers || !glBufferData ||
      !glEnableVertexAttribArray || !glDisableVertexAttribArray || !glVertexAttribPointer) {
    fprintf(stderr, "Warning: Failed to load some OpenGL extensions on Windows\n");
  }
}

#endif /* _WIN32 || _WIN64 */
