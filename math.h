#ifndef MATH_H
#define MATH_H
#include <math.h>
#include <stdlib.h>

#define M_PIf 3.14159265358979323846f

/* generic growable array: arr/count/cap must be plain locals of matching
   pointer/int/int type. */
#define DA_PUSH(arr,count,cap,item) do{ \
    if((count) >= (cap)){ (cap) = (cap) ? (cap)*2 : 8; \
        (arr) = realloc((arr), (size_t)(cap)*sizeof(*(arr))); } \
    (arr)[(count)++] = (item); }while(0)

typedef struct { float x,y,z; } vec3;
vec3 v3(float x,float y,float z);
vec3 vadd(vec3 a,vec3 b);
vec3 vsub(vec3 a,vec3 b);
vec3 vscale(vec3 a,float s);
vec3 vmul(vec3 a,vec3 b);
vec3 vcross(vec3 a,vec3 b);
float vdot(vec3 a,vec3 b);
float vlen(vec3 a);
vec3 vnorm(vec3 a);

typedef struct { float m[16]; } mat4;
mat4 mat4_identity(void);
mat4 mat4_mul(mat4 a, mat4 b);
mat4 mat4_translate(vec3 t);
mat4 mat4_scale(vec3 s);
mat4 mat4_rot_x(float deg);
mat4 mat4_rot_y(float deg);
mat4 mat4_rot_z(float deg);
mat4 mat4_rot_xyz(vec3 rdeg);
vec3 mat4_xform_point(mat4 m, vec3 p);
vec3 mat4_xform_dir(mat4 m, vec3 p);
mat4 mat4_perspective(float fovy_deg,float aspect,float znear,float zfar);
mat4 mat4_lookat(vec3 eye, vec3 center, vec3 up);

#endif
