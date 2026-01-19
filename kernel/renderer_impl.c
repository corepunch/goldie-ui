/* Renderer API Implementation - OpenGL abstraction layer */
#include "renderer.h"
#include "../user/gl_compat.h"
#include <string.h>

// Initialize a mesh with vertex attributes and drawing mode
void R_MeshInit(R_Mesh* mesh, const R_VertexAttrib* attribs, size_t attrib_count, 
                size_t vertex_size, GLenum draw_mode) {
  if (!mesh) return;
  
  memset(mesh, 0, sizeof(R_Mesh));
  mesh->vertex_size = vertex_size;
  mesh->draw_mode = draw_mode;
  
  // Create VAO and VBO
  glGenVertexArrays(1, &mesh->vao);
  glGenBuffers(1, &mesh->vbo);
  
  // Bind VAO to configure it
  glBindVertexArray(mesh->vao);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  
  // Set up vertex attributes
  if (attribs && attrib_count > 0) {
    R_SetVertexAttribs(attribs, attrib_count, vertex_size);
  }
  
  // Unbind to avoid accidental modifications
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Upload vertex data to mesh buffer
void R_MeshUpload(R_Mesh* mesh, const void* data, size_t vertex_count) {
  if (!mesh || !data || vertex_count == 0) return;
  
  mesh->vertex_count = vertex_count;
  
  // Bind and upload data
  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, vertex_count * mesh->vertex_size, data, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Draw the mesh using its current vertex data
void R_MeshDraw(R_Mesh* mesh) {
  if (!mesh || mesh->vertex_count == 0) return;
  
  glBindVertexArray(mesh->vao);
  glDrawArrays(mesh->draw_mode, 0, mesh->vertex_count);
  glBindVertexArray(0);
}

// Upload and draw in one call (efficient for dynamic geometry)
void R_MeshDrawDynamic(R_Mesh* mesh, const void* data, size_t vertex_count) {
  if (!mesh || !data || vertex_count == 0) return;
  
  mesh->vertex_count = vertex_count;
  
  // Bind VAO
  glBindVertexArray(mesh->vao);
  
  // Upload vertex data
  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, vertex_count * mesh->vertex_size, data, GL_DYNAMIC_DRAW);
  
  // Draw
  glDrawArrays(mesh->draw_mode, 0, vertex_count);
  
  // Unbind to avoid accidental modifications
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Destroy mesh and free GPU resources
void R_MeshDestroy(R_Mesh* mesh) {
  if (!mesh) return;
  
  SAFE_DELETE_N(mesh->vao, glDeleteVertexArrays);
  SAFE_DELETE_N(mesh->vbo, glDeleteBuffers);
  SAFE_DELETE_N(mesh->ibo, glDeleteBuffers);
  
  memset(mesh, 0, sizeof(R_Mesh));
}

// Bind texture to current texture unit
void R_TextureBind(R_Texture* texture) {
  if (!texture || texture->id == 0) return;
  glBindTexture(GL_TEXTURE_2D, texture->id);
}

// Unbind texture from current texture unit
void R_TextureUnbind(void) {
  glBindTexture(GL_TEXTURE_2D, 0);
}

// Enable and configure vertex attributes
void R_SetVertexAttribs(const R_VertexAttrib* attribs, size_t count, size_t vertex_size) {
  if (!attribs || count == 0) return;
  
  for (size_t i = 0; i < count; i++) {
    const R_VertexAttrib* attr = &attribs[i];
    glEnableVertexAttribArray(attr->index);
    glVertexAttribPointer(attr->index, attr->size, attr->type, attr->normalized,
                         vertex_size, (void*)attr->offset);
  }
}

// Disable vertex attributes
void R_ClearVertexAttribs(size_t count) {
  for (size_t i = 0; i < count; i++) {
    glDisableVertexAttribArray(i);
  }
}

// Allocate a font texture with given dimensions and format
GLuint R_AllocateFontTexture(R_Texture* tex, void *data) {
  glGenTextures(1, &tex->id);
  glBindTexture(GL_TEXTURE_2D, tex->id);
  
  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  
  // Use texture swizzling for proper rendering (R channel becomes alpha)
  GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
  
  // Upload texture data
  glTexImage2D(GL_TEXTURE_2D, 0, tex->format, tex->width, tex->height, 0, tex->format, GL_UNSIGNED_BYTE, data);

  return tex->id;
}
