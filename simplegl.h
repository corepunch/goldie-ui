#ifndef SIMPLEGL_H
#define SIMPLEGL_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define M_PIf 3.14159265358979323846f

#define DA_PUSH(arr,count,cap,item) do{ \
	if((count) >= (cap)){ (cap) = (cap) ? (cap)*2 : 8; \
		(arr) = realloc((arr), (size_t)(cap)*sizeof(*(arr))); } \
	(arr)[(count)++] = (item); }while(0)

typedef struct { float x,y,z; } vec3;
typedef struct { float m[16]; } mat4;

vec3 v3(float x,float y,float z);
vec3 vadd(vec3 a,vec3 b);
vec3 vsub(vec3 a,vec3 b);
vec3 vscale(vec3 a,float s);
vec3 vmul(vec3 a,vec3 b);
vec3 vcross(vec3 a,vec3 b);
float vdot(vec3 a,vec3 b);
float vlen(vec3 a);
vec3 vnorm(vec3 a);

mat4 mat4_identity(void);
mat4 mat4_mul(mat4 a,mat4 b);
mat4 mat4_translate(vec3 t);
mat4 mat4_scale(vec3 s);
mat4 mat4_rot_x(float deg);
mat4 mat4_rot_y(float deg);
mat4 mat4_rot_z(float deg);
mat4 mat4_rot_xyz(vec3 rdeg);
vec3 mat4_xform_point(mat4 m,vec3 p);
vec3 mat4_xform_dir(mat4 m,vec3 p);
mat4 mat4_perspective(float fovy_deg,float aspect,float znear,float zfar);
mat4 mat4_lookat(vec3 eye,vec3 center,vec3 up);

typedef struct { vec3 pos,nrm; } Vertex;
typedef struct { int a,b,c; } Tri;
typedef struct { vec3 p0,p1; int t0,t1; } Edge;

typedef struct {
	Vertex *verts; int nverts,cverts;
	Tri *tris; int ntris,ctris;
	Edge *edges; int nedges,cedges;
	vec3 *triN;
} Mesh;

void mesh_free(Mesh *m);
int mesh_add_vert(Mesh *m,vec3 p,vec3 n);
void mesh_add_tri(Mesh *m,int a,int b,int c);
void mesh_transform(Mesh *m,mat4 posM,mat4 rotM);
void mesh_compute_face_normals(Mesh *m);
void mesh_build_edges(Mesh *m);
float mesh_signed_volume(Mesh *m);
void mesh_flip_winding(Mesh *m);
void mesh_apply_taper(Mesh *m,float amount,float curvature,char axis);
void mesh_apply_twist(Mesh *m,float angle_deg,char axis);
void mesh_apply_bend(Mesh *m,float angle_deg,char axis);
void mesh_apply_stretch(Mesh *m,float amount,float amplify,char axis);
void mesh_apply_skew(Mesh *m,float amount,char axis);
void mesh_apply_array(Mesh *m,int count,vec3 off,vec3 rot);
Mesh gen_box(float sx,float sy,float sz);
Mesh gen_box_inset(float sx,float sy,float sz,float insetX,float insetY);
Mesh gen_cylinder_like(int sides,float rBot,float rTop,float height,int smooth);
Mesh gen_cylinder(float r,float h,int sides);
Mesh gen_cylinder_tube(float r,float h,float wall,int sides);
Mesh gen_prism(float r,float h,int sides);
Mesh gen_cone(float rBase,float rTop,float h,int sides);
Mesh gen_sphere(float r,int rings,int slices);
Mesh gen_torus(float R,float r,int majorSeg,int minorSeg);
Mesh gen_arch(float width,float height,float depth,float wall,int segments,float inset);

typedef struct { char id[32]; vec3 color; float shininess; } Material;
typedef struct { char name[32]; char comment[64]; vec3 pos,look; float fov; } Camera;
typedef struct { vec3 pos,color,dir; float intensity,radius; int castsShadow,isDirectional; } Light;
typedef struct { Mesh mesh; vec3 color; float shininess; int castsShadow,renderable,unlit,sanityIgnore,sanityFloor,sanityCheck; } SceneObj;
typedef struct { float x,y,z,w; } ShadowVertex;
typedef struct { ShadowVertex *verts; int nverts,cverts; } ShadowVolume;
typedef struct { char name[32]; vec3 pos; } AttachPoint;
typedef struct { char ref[32]; void *root; AttachPoint *attaches; int nattaches, cattaches; } PrefabDef;
typedef struct { char name[32]; char ref[32]; mat4 transform, rotMatrix; } InstanceDef;
typedef struct { mat4 transform; vec3 size; } NegativeBox;
typedef struct { mat4 transform; float width,height,depth; } NegativeArch;
typedef struct { vec3 start, end, color; int category; } OverlayLine;
typedef struct { char name[32]; float height, radius; float top, neck, pelvis, feet; } CharDef;

typedef struct {
	vec3 camPos,camLook; float camFov;
	Camera *cameras; int ncameras,ccameras;
	vec3 ambient,bg;
	Light *lights; int nlights,clights;
	Material *mats; int nmats,cmats;
	SceneObj *objs; int nobjs,cobjs;
	ShadowVolume *svols;
	PrefabDef *prefabs; int nprefabs,cprefabs;
	InstanceDef *instances; int ninstances,cinstances;
	NegativeBox *negativeBoxes; int nnegativeBoxes,cnegativeBoxes;
	NegativeArch *negativeArches; int nnegativeArches,cnegativeArches;
	vec3 prefabTint; int prefabTintActive;
	int sanityIgnoreActive, sanityFloorActive, sanityCheckActive;
	OverlayLine *overlayLines; int noverlayLines, coverlayLines;
	CharDef *charDefs; int ncharDefs, ccharDefs;
} Scene;

int load_scene(const char *path,Scene *s);
void scene_free(Scene *s);
void scene_select_camera(Scene *s,const char *name);
void scene_add_obj(Scene *s,Mesh mesh,mat4 M,mat4 R,vec3 color,float shin,int castsShadow,int renderable,int unlit);
int scene_sanity_check(Scene *s);
vec3 light_to_source(Light *light,vec3 point);
void scene_rebuild_camera_gizmos(Scene *s,float aspect);

void build_shadow_volume(Mesh *m,vec3 lightPos,vec3 lightDir,int isDir,ShadowVolume *sv);
void scene_build_all_shadow_volumes(Scene *s);

#define DBG_NONE            0
#define DBG_NO_SHADOWS      (1 << 0)
#define DBG_WIRE_SHADOWVOL  (1 << 1)
#define DBG_SHOW_STENCIL    (1 << 2)
#define DBG_HIDE_LIGHTS     (1 << 3)
#define DBG_HIDE_CHARS      (1 << 4)

void render_frame(Scene *s,int w,int h,mat4 proj,mat4 view,vec3 camPos,int debugFlags);

#endif
