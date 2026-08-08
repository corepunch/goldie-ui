#ifndef SHADER_H
#define SHADER_H

#include "simplegl.h"

void shader_init(void);
void shader_deinit(void);
void shader_bind(void);
void shader_unbind(void);
void shader_set_viewproj(mat4 m);
void shader_set_camera_pos(vec3 pos);
void shader_set_light(Light *L);
void shader_set_material(vec3 color, float shininess);
void shader_set_texture(unsigned int tex);
void shader_draw_mesh(Mesh *m);

#endif
