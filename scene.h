#ifndef SCENE_H
#define SCENE_H
#include "math.h"
#include "mesh.h"

typedef struct { char id[32]; vec3 color; float shininess; } Material;
typedef struct { vec3 pos,color; float intensity; int castsShadow; } Light;
typedef struct { Mesh mesh; vec3 color; float shininess; int castsShadow; } SceneObj;
typedef struct { vec3 *verts; int nverts,cverts; } ShadowVolume;

typedef struct {
    vec3 camPos, camLook; float camFov;
    vec3 ambient, bg;
    Light *lights; int nlights,clights;
    Material *mats; int nmats,cmats;
    SceneObj *objs; int nobjs,cobjs;
    ShadowVolume *svols;
} Scene;

int  load_scene(const char *path, Scene *s);
void scene_free(Scene *s);

/* scene_add_obj: transform mesh, auto-fix winding, compute face normals/edges */
void scene_add_obj(Scene *s, Mesh mesh, mat4 M, mat4 R, vec3 color, float shin, int castsShadow);

#endif
