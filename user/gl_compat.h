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

#endif /* __GL_COMPAT_H__ */
