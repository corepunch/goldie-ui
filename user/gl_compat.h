/* OpenGL compatibility header for cross-platform support */
#ifndef __GL_COMPAT_H__
#define __GL_COMPAT_H__

#ifdef __APPLE__
  #include <OpenGL/gl3.h>
#elif defined(_WIN32) || defined(_WIN64)
  /* Windows platform */
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <GL/gl.h>
  #include <GL/glext.h>
  
  /* Declare OpenGL 3.x+ function pointers for Windows */
  /* These functions are not available in opengl32.dll by default and must be loaded at runtime */
  extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
  extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
  extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
  extern PFNGLGENBUFFERSPROC glGenBuffers;
  extern PFNGLBINDBUFFERPROC glBindBuffer;
  extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;
  extern PFNGLBUFFERDATAPROC glBufferData;
  extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
  extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
  extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
  
  /* Function to load OpenGL extensions on Windows */
  void load_gl_extensions_win32(void);
#else
  /* Linux and other platforms */
  #include <GL/glcorearb.h>
  #include <GL/gl.h>
#endif

// Safe deletion macros for OpenGL resources and similar cleanup patterns
// NOTE: These macros are designed for scalar resource handles (GLuint, pointers, etc.)

// SAFE_DELETE: for single-parameter delete functions (e.g., glDeleteProgram, free)
// Usage: SAFE_DELETE(my_program, glDeleteProgram)
//        SAFE_DELETE(my_pointer, free)
#define SAFE_DELETE(resource, delete_func) \
  do { \
    if (resource) { \
      delete_func(resource); \
      resource = 0; \
    } \
  } while(0)

// SAFE_DELETE_N: for count+pointer delete functions (e.g., glDeleteTextures, glDeleteBuffers)
// Usage: SAFE_DELETE_N(my_texture, glDeleteTextures)
// NOTE: This macro takes the address of the resource (&resource), so it requires
//       scalar resource handles (GLuint), not pointer variables
#define SAFE_DELETE_N(resource, delete_func) \
  do { \
    if (resource) { \
      delete_func(1, &resource); \
      resource = 0; \
    } \
  } while(0)

#endif /* __GL_COMPAT_H__ */
