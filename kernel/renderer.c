#include "../ui.h"
#include "../user/gl_compat.h"

#include <cglm/cglm.h>
#include <cglm/struct.h>

#define OFFSET_OF(type, field) (void*)((size_t)&(((type *)0)->field))

// Vertex structure for our buffer (xyzuv)
typedef struct {
  int16_t x, y, z;    // Position
  int16_t u, v;       // Texture coordinates
  int8_t nx, ny, nz;  // Normal
  int32_t color;
} wall_vertex_t;

// Sprite vertices (quad)
wall_vertex_t sprite_verts[] = {
  {0, 0, 0, 0, 0, 0, 0, 0, -1}, // bottom left
  {0, 1, 0, 0, 1, 0, 0, 0, -1},  // top left
  {1, 1, 0, 1, 1, 0, 0, 0, -1}, // top right
  {1, 0, 0, 1, 0, 0, 0, 0, -1}, // bottom right
};

// Sprite system state
typedef struct {
  GLuint program;        // Shader program
  GLuint vao;            // Vertex array object
  GLuint vbo;            // Vertex buffer object
  mat4 projection;       // Orthographic projection matrix
} renderer_system_t;

renderer_system_t g_ref = {0};

// Sprite shader sources
const char* sprite_vs_src = "#version 150 core\n"
"in vec2 position;\n"
"in vec2 texcoord;\n"
"in vec4 color;\n"
"out vec2 tex;\n"
"out vec4 col;\n"
"uniform mat4 projection;\n"
"uniform vec2 offset;\n"
"uniform vec2 scale;\n"
"void main() {\n"
"  col = color;\n"
"  tex = texcoord;\n"
"  gl_Position = projection * vec4(position * scale + offset, 0.0, 1.0);\n"
"}";

const char* sprite_fs_src = "#version 150 core\n"
"in vec2 tex;\n"
"in vec4 col;\n"
"out vec4 outColor;\n"
"uniform sampler2D tex0;\n"
"uniform float alpha;\n"
"void main() {\n"
"  outColor = texture(tex0, tex) * col;\n"
"  outColor.a *= alpha;\n"
"  if(outColor.a < 0.1) discard;\n"
"}";

// Compile a shader
GLuint compile_shader(GLenum type, const char* src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, 0);
  glCompileShader(shader);
  
  // Check for errors
  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE) {
    GLint log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    char* log = malloc(log_length);
    glGetShaderInfoLog(shader, log_length, NULL, log);
    printf("Shader compilation error: %s\n", log);
    free(log);
  }
  
  return shader;
}

int get_sprite_prog(void) {
  return g_ref.program;
}

int get_sprite_vao(void) {
  return g_ref.vao;
}

// Initialize the sprite system
bool ui_init_prog(void) {
  // Create shader program
  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, sprite_vs_src);
  GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, sprite_fs_src);
  
  g_ref.program = glCreateProgram();
  glAttachShader(g_ref.program, vertex_shader);
  glAttachShader(g_ref.program, fragment_shader);
  glBindAttribLocation(g_ref.program, 0, "position");
  glBindAttribLocation(g_ref.program, 1, "texcoord");
  glBindAttribLocation(g_ref.program, 2, "color");
  glLinkProgram(g_ref.program);
  
  // Create VAO, VBO, EBO
  glGenVertexArrays(1, &g_ref.vao);
  glBindVertexArray(g_ref.vao);
  
  glGenBuffers(1, &g_ref.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_ref.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(sprite_verts), sprite_verts, GL_STATIC_DRAW);
  
  // Set up vertex attributes
  //  glEnableVertexAttribArray(0);
  //  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  //  glEnableVertexAttribArray(1);
  //  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
  
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), OFFSET_OF(wall_vertex_t, x)); // Position
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), OFFSET_OF(wall_vertex_t, u)); // UV
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(wall_vertex_t), OFFSET_OF(wall_vertex_t, color)); // Color
  
  // Create orthographic projection matrix for screen-space rendering
  int width, height;
  SDL_GetWindowSize(SDL_GL_GetCurrentWindow(), &width, &height);
  //  float scale = (float)height / DOOM_HEIGHT;
  //  float render_width = DOOM_WIDTH * scale;
  //  float offset_x = (width - render_width) / (2.0f * scale);
  //  black_bars = offset_x;
  //  glm_ortho(-offset_x, DOOM_WIDTH+offset_x, DOOM_HEIGHT, 0, -1, 1, g_ref.projection);
  screen_width = width/2;
  screen_height = height/2;
  glm_ortho(0, screen_width, screen_height, 0, -1, 1, g_ref.projection);
    
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  
  return true;
}

void ui_shutdown_prog(void) {
  // Delete shader program and buffers
  glDeleteProgram(g_ref.program);
  glDeleteVertexArrays(1, &g_ref.vao);
  glDeleteBuffers(1, &g_ref.vbo);
}

void push_sprite_args(int tex, int x, int y, int w, int h, float alpha) {
  // Bind sprite texture
  glUseProgram(g_ref.program);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(glGetUniformLocation(g_ref.program, "tex0"), 0);
  glUniform2f(glGetUniformLocation(g_ref.program, "offset"), x, y);
  glUniform2f(glGetUniformLocation(g_ref.program, "scale"), w, h);
  glUniform1f(glGetUniformLocation(g_ref.program, "alpha"), alpha);
}

void set_projection(int x, int y, int w, int h) {
  mat4 projection;
  glm_ortho(x, w, h, y, -1, 1, projection);
  glUseProgram(get_sprite_prog());
  glUniformMatrix4fv(glGetUniformLocation(g_ref.program, "projection"), 1, GL_FALSE, projection[0]);
}

float *get_sprite_matrix(void) {
  return (float*)&g_ref.projection;
}

// Draw a sprite at the specified screen position
void draw_rect_ex(int tex, int x, int y, int w, int h, int type, float alpha) {
  push_sprite_args(tex, x, y, w, h, alpha);
  
  // Bind VAO and draw
  glBindVertexArray(g_ref.vao);
  //  if (alpha == 1) {
  //    glDisable(GL_BLEND);
  //  } else {
  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //  }
  // Disable depth testing for UI elements
  glDisable(GL_DEPTH_TEST);
  
  glDrawArrays(type?GL_LINE_LOOP:GL_TRIANGLE_FAN, 0, 4);
  
  // Reset state
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
}

// Draw a sprite at the specified screen position
void draw_rect(int tex, int x, int y, int w, int h) {
  draw_rect_ex(tex, x, y, w, h, false, 1);
}

