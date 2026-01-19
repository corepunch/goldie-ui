/* Renderer API - High-level abstraction for OpenGL rendering
 * Provides clean API for mesh and texture management to reduce boilerplate
 */
#ifndef __UI_RENDERER_H__
#define __UI_RENDERER_H__

#include <stddef.h>
#include <stdbool.h>
#include "../user/gl_compat.h"

// Vertex attribute description
typedef struct {
  GLuint index;           // Attribute index (location in shader)
  GLint size;             // Number of components (1-4)
  GLenum type;            // Data type (GL_FLOAT, GL_SHORT, GL_UNSIGNED_BYTE, etc.)
  GLboolean normalized;   // Whether to normalize fixed-point data
  size_t offset;          // Offset in vertex structure
} R_VertexAttrib;

// Mesh/drawable object - encapsulates VAO, VBO, and vertex format
typedef struct {
  GLuint vao;             // Vertex array object
  GLuint vbo;             // Vertex buffer object
  GLuint ibo;             // Index buffer object (0 if unused)
  size_t vertex_size;     // Size of a single vertex in bytes
  size_t vertex_count;    // Number of vertices currently in buffer
  GLenum draw_mode;       // Drawing mode (GL_TRIANGLES, GL_LINES, etc.)
} R_Mesh;

// Texture object - encapsulates texture state
typedef struct {
  GLuint id;              // OpenGL texture ID
  int width;              // Texture width in pixels
  int height;             // Texture height in pixels
  GLenum format;          // Texture format (GL_RGBA, GL_RED, etc.)
} R_Texture;

// Mesh management functions
// Initialize a mesh with vertex attributes and drawing mode
void R_MeshInit(R_Mesh* mesh, const R_VertexAttrib* attribs, size_t attrib_count, 
                size_t vertex_size, GLenum draw_mode);

// Upload vertex data to mesh buffer (for static or dynamic geometry)
void R_MeshUpload(R_Mesh* mesh, const void* data, size_t vertex_count);

// Draw the mesh using its current vertex data
void R_MeshDraw(R_Mesh* mesh);

// Upload and draw in one call (efficient for dynamic geometry that changes every frame)
void R_MeshDrawDynamic(R_Mesh* mesh, const void* data, size_t vertex_count);

// Destroy mesh and free GPU resources
void R_MeshDestroy(R_Mesh* mesh);

// Texture management functions
// Bind texture to current texture unit
void R_TextureBind(R_Texture* texture);

// Unbind texture from current texture unit
void R_TextureUnbind(void);

// Low-level vertex attribute helpers
// Enable and configure vertex attributes
void R_SetVertexAttribs(const R_VertexAttrib* attribs, size_t count, size_t vertex_size);

// Disable vertex attributes
void R_ClearVertexAttribs(size_t count);

// Allocate a font texture with given dimensions and format
GLuint R_AllocateFontTexture(R_Texture* texture, void *data);

#endif /* __UI_RENDERER_H__ */
