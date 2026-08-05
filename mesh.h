#ifndef MESH_H
#define MESH_H
#include <stdlib.h>
#include <string.h>
#include "math.h"

typedef struct { vec3 pos, nrm; } Vertex;
typedef struct { int a,b,c; } Tri;
typedef struct { vec3 p0,p1; int t0,t1; } Edge;

typedef struct {
    Vertex *verts; int nverts,cverts;
    Tri    *tris;  int ntris,ctris;
    Edge   *edges; int nedges,cedges;
    vec3   *triN;
} Mesh;

void mesh_free(Mesh *m);
int  mesh_add_vert(Mesh *m, vec3 p, vec3 n);
void mesh_add_tri(Mesh *m,int a,int b,int c);
void mesh_transform(Mesh *m, mat4 posM, mat4 rotM);
void mesh_compute_face_normals(Mesh *m);
void mesh_build_edges(Mesh *m);
float mesh_signed_volume(Mesh *m);
void mesh_flip_winding(Mesh *m);

/* primitive generators (local-space, centered at origin) */
Mesh gen_box(float sx,float sy,float sz);
Mesh gen_cylinder_like(int sides,float rBot,float rTop,float height,int smooth);
Mesh gen_cylinder(float r,float h,int sides);
Mesh gen_prism(float r,float h,int sides);
Mesh gen_cone(float rBase,float rTop,float h,int sides);
Mesh gen_sphere(float r,int rings,int slices);
Mesh gen_torus(float R,float r,int majorSeg,int minorSeg);

#endif
