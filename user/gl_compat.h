/* OpenGL compatibility header for cross-platform support */
#ifndef __GL_COMPAT_H__
#define __GL_COMPAT_H__

#ifdef __APPLE__
  #include <OpenGL/gl3.h>
#else
  /* Linux and other platforms */
  #include <GL/glcorearb.h>
  #include <GL/gl.h>
#endif

// Safe deletion macros for OpenGL resources
// SAFE_DELETE: for single-parameter delete functions (e.g., glDeleteProgram, free)
#define SAFE_DELETE(resource, delete_func) \
  do { \
    if (resource) { \
      delete_func(resource); \
      resource = 0; \
    } \
  } while(0)

// SAFE_DELETE_N: for count+pointer delete functions (e.g., glDeleteTextures, glDeleteBuffers)
#define SAFE_DELETE_N(resource, delete_func) \
  do { \
    if (resource) { \
      delete_func(1, &resource); \
      resource = 0; \
    } \
  } while(0)

#endif /* __GL_COMPAT_H__ */
