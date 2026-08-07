/* ============================================================================
   simplegl renderer.c  --  a very small OpenGL(1.x/2.1 fixed-function) scene
   renderer with stencil-volume shadows, driven by an XML scene file.

   BUILD
     linux:  gcc -O2 renderer.c -o simplegl `pkg-config --cflags --libs sdl2` -lGL -lm
     (see Makefile)

   RUN
     ./simplegl scenes/sample_room.xml

   CONTROLS
     mouse       look around (mouse is captured; press ESC to release/quit)
     W A S D     move
     Q / E       down / up
     SHIFT       move faster

   DESIGN NOTES (read me before extending)
   -----------------------------------------------------------------------
   - Fixed-function GL (glBegin/glEnd, GL_LIGHTING) on purpose: it gives us
     GL_LIGHT/GL_MATERIAL and two-sided stencil ops for free, so the whole
     renderer + shadow algorithm fits in one small file with no shader code.
   - Every mesh is generated in local space, then baked into WORLD space once
     at scene-load time (position/rotation/scale applied to every vertex).
     The scene is treated as static: nothing moves after load. That means
     shadow volumes can also be precomputed once instead of every frame.
     (If you want moving lights/objects, move the shadow-volume build calls
     from load_scene() into the render loop -- the functions are already
     structured for that, just re-call build_shadow_volume() per frame.)
   - Shading normals (smooth for sphere/torus, flat/faceted for box/prism/
     cone/pyramid) are stored per-vertex in Vertex.nrm and used only for
     lighting. Shadow-silhouette detection uses a *separate* flat face
     normal computed from triangle positions (Mesh.triN), so shading style
     never affects shadow correctness.
   - Shadows: classic robust "z-fail" (Carmack's Reverse) stencil shadow
     volumes, single point light per pass, additive multi-pass lighting.
     One point light is the whole feature set on purpose -- it is the
     simplest version of the algorithm that is still correct when the
     camera is inside a shadow.
   - Booleans/CSG: true CSG (BSP clipping etc.) was deliberately skipped.
     Door/window openings are instead built directly as a handful of boxes
     (see build_wall_boxes()): the wall's length axis is cut into segments
     at each opening's edges, and each segment becomes a full-height box,
     or (inside an opening) a sill box + a lintel box. This is exactly the
     "just use boxes" approach requested -- it covers axis-aligned wall
     openings, which is all a room/furniture scene generator needs, without
     writing/porting a real solid-boolean library.
   ==========================================================================*/

#define GL_SILENCE_DEPRECATION
#define _POSIX_C_SOURCE 200809L
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------------------------------------------------------------- dynarr */
/* generic growable array: arr/count/cap must be plain locals of matching
   pointer/int/int type. Used everywhere instead of hand-rolled containers. */
#define DA_PUSH(arr,count,cap,item) do{ \
    if((count) >= (cap)){ (cap) = (cap) ? (cap)*2 : 8; \
        (arr) = realloc((arr), (size_t)(cap)*sizeof(*(arr))); } \
    (arr)[(count)++] = (item); }while(0)

/* ------------------------------------------------------------------ vec3 */
typedef struct { float x,y,z; } vec3;
static vec3 v3(float x,float y,float z){ vec3 v={x,y,z}; return v; }
static vec3 vadd(vec3 a,vec3 b){ return v3(a.x+b.x,a.y+b.y,a.z+b.z); }
static vec3 vsub(vec3 a,vec3 b){ return v3(a.x-b.x,a.y-b.y,a.z-b.z); }
static vec3 vscale(vec3 a,float s){ return v3(a.x*s,a.y*s,a.z*s); }
static vec3 vmul(vec3 a,vec3 b){ return v3(a.x*b.x,a.y*b.y,a.z*b.z); }
static vec3 vcross(vec3 a,vec3 b){
    return v3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x);
}
static float vdot(vec3 a,vec3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
static float vlen(vec3 a){ return sqrtf(vdot(a,a)); }
static vec3 vnorm(vec3 a){ float l=vlen(a); return l>1e-8f? vscale(a,1.0f/l): v3(0,0,1); }

/* ------------------------------------------------------------------ mat4 */
/* column-major, OpenGL layout: m[col*4+row]. */
typedef struct { float m[16]; } mat4;

static mat4 mat4_identity(void){
    mat4 r={{0}}; r.m[0]=r.m[5]=r.m[10]=r.m[15]=1.0f; return r;
}
static mat4 mat4_mul(mat4 a, mat4 b){ /* a*b */
    mat4 r={{0}};
    for(int c=0;c<4;c++) for(int row=0;row<4;row++){
        float s=0;
        for(int k=0;k<4;k++) s += a.m[k*4+row]*b.m[c*4+k];
        r.m[c*4+row]=s;
    }
    return r;
}
static mat4 mat4_translate(vec3 t){
    mat4 r=mat4_identity(); r.m[12]=t.x; r.m[13]=t.y; r.m[14]=t.z; return r;
}
static mat4 mat4_scale(vec3 s){
    mat4 r=mat4_identity(); r.m[0]=s.x; r.m[5]=s.y; r.m[10]=s.z; return r;
}
static mat4 mat4_rot_x(float deg){
    float a=deg*(float)M_PI/180.0f, c=cosf(a), s=sinf(a);
    mat4 r=mat4_identity(); r.m[5]=c; r.m[6]=s; r.m[9]=-s; r.m[10]=c; return r;
}
static mat4 mat4_rot_y(float deg){
    float a=deg*(float)M_PI/180.0f, c=cosf(a), s=sinf(a);
    mat4 r=mat4_identity(); r.m[0]=c; r.m[2]=-s; r.m[8]=s; r.m[10]=c; return r;
}
static mat4 mat4_rot_z(float deg){
    float a=deg*(float)M_PI/180.0f, c=cosf(a), s=sinf(a);
    mat4 r=mat4_identity(); r.m[0]=c; r.m[1]=s; r.m[4]=-s; r.m[5]=c; return r;
}
/* rotation order: Rz * Ry * Rx (euler degrees, applied X then Y then Z) */
static mat4 mat4_rot_xyz(vec3 rdeg){
    return mat4_mul(mat4_rot_z(rdeg.z), mat4_mul(mat4_rot_y(rdeg.y), mat4_rot_x(rdeg.x)));
}
static vec3 mat4_xform_point(mat4 m, vec3 p){
    return v3( m.m[0]*p.x+m.m[4]*p.y+m.m[8]*p.z+m.m[12],
               m.m[1]*p.x+m.m[5]*p.y+m.m[9]*p.z+m.m[13],
               m.m[2]*p.x+m.m[6]*p.y+m.m[10]*p.z+m.m[14]);
}
static vec3 mat4_xform_dir(mat4 m, vec3 p){ /* ignores translation */
    return v3( m.m[0]*p.x+m.m[4]*p.y+m.m[8]*p.z,
               m.m[1]*p.x+m.m[5]*p.y+m.m[9]*p.z,
               m.m[2]*p.x+m.m[6]*p.y+m.m[10]*p.z);
}
static mat4 mat4_perspective(float fovy_deg,float aspect,float zn,float zf){
    float f=1.0f/tanf(fovy_deg*(float)M_PI/360.0f);
    mat4 r={{0}};
    r.m[0]=f/aspect; r.m[5]=f;
    r.m[10]=(zf+zn)/(zn-zf); r.m[11]=-1.0f;
    r.m[14]=(2*zf*zn)/(zn-zf);
    return r;
}
static mat4 mat4_lookat(vec3 eye, vec3 center, vec3 up){
    vec3 f=vnorm(vsub(center,eye));
    vec3 s=vnorm(vcross(f,up));
    vec3 u=vcross(s,f);
    mat4 r=mat4_identity();
    r.m[0]=s.x; r.m[4]=s.y; r.m[8]=s.z;
    r.m[1]=u.x; r.m[5]=u.y; r.m[9]=u.z;
    r.m[2]=-f.x; r.m[6]=-f.y; r.m[10]=-f.z;
    r.m[12]=-vdot(s,eye); r.m[13]=-vdot(u,eye); r.m[14]=vdot(f,eye);
    return r;
}

/* ------------------------------------------------------------------ mesh */
typedef struct { vec3 pos, nrm; } Vertex;
typedef struct { int a,b,c; } Tri;
/* directed silhouette edge: p0->p1 as it appears in triangle t0's winding.
   t1 is the other triangle sharing this edge (-1 if boundary/open edge). */
typedef struct { vec3 p0,p1; int t0,t1; } Edge;

typedef struct {
    Vertex *verts; int nverts,cverts;
    Tri    *tris;  int ntris,ctris;
    Edge   *edges; int nedges,cedges;
    vec3   *triN;  /* flat face normal per triangle, world space, for shadows */
} Mesh;

static void mesh_free(Mesh *m) __attribute__((unused));
static void mesh_free(Mesh *m){
    free(m->verts); free(m->tris); free(m->edges); free(m->triN);
    memset(m,0,sizeof(*m));
}
static int mesh_add_vert(Mesh *m, vec3 p, vec3 n){
    Vertex v={p,n}; DA_PUSH(m->verts,m->nverts,m->cverts,v); return m->nverts-1;
}
static void mesh_add_tri(Mesh *m,int a,int b,int c){
    Tri t={a,b,c}; DA_PUSH(m->tris,m->ntris,m->ctris,t);
}

/* apply an affine transform (posM for positions, rotM for normals -- pass
   just the rotation so non-uniform scale doesn't warp shading normals) */
static void mesh_transform(Mesh *m, mat4 posM, mat4 rotM){
    for(int i=0;i<m->nverts;i++){
        m->verts[i].pos = mat4_xform_point(posM, m->verts[i].pos);
        m->verts[i].nrm = vnorm(mat4_xform_dir(rotM, m->verts[i].nrm));
    }
}

/* compute flat face normals from *world-space* vertex positions -- always
   correct regardless of how the mesh is shaded, used only for shadows */
static void mesh_compute_face_normals(Mesh *m){
    free(m->triN); m->triN = malloc(sizeof(vec3)*(size_t)m->ntris);
    for(int i=0;i<m->ntris;i++){
        Tri t=m->tris[i];
        vec3 a=m->verts[t.a].pos, b=m->verts[t.b].pos, c=m->verts[t.c].pos;
        m->triN[i] = vnorm(vcross(vsub(b,a),vsub(c,a)));
    }
}

/* weld-by-position (O(n^2), meshes here are a few hundred verts) then build
   the shared-edge adjacency needed for silhouette detection. */
static void mesh_build_edges(Mesh *m){
    int *weld = malloc(sizeof(int)*(size_t)m->nverts);
    for(int i=0;i<m->nverts;i++){
        weld[i]=i;
        for(int j=0;j<i;j++){
            if(vlen(vsub(m->verts[i].pos,m->verts[j].pos)) < 1e-4f){ weld[i]=weld[j]; break; }
        }
    }
    free(m->edges); m->edges=NULL; m->nedges=m->cedges=0;
    for(int i=0;i<m->ntris;i++){
        Tri t=m->tris[i];
        int pairs[3][2]={{t.a,t.b},{t.b,t.c},{t.c,t.a}};
        for(int e=0;e<3;e++){
            int v0=pairs[e][0], v1=pairs[e][1];
            int w0=weld[v0], w1=weld[v1];
            int found=-1;
            for(int k=0;k<m->nedges;k++){
                /* an existing edge matches if it is the SAME pair, reversed
                   (consistent winding on a closed manifold mesh) */
                Edge *ed=&m->edges[k];
                if(ed->t1<0){
                    vec3 ep0=ed->p0, ep1=ed->p1;
                    int ew0 = (vlen(vsub(ep0,m->verts[v1].pos))<1e-4f);
                    int ew1 = (vlen(vsub(ep1,m->verts[v0].pos))<1e-4f);
                    (void)w0; (void)w1;
                    if(ew0 && ew1){ found=k; break; }
                }
            }
            if(found>=0){ m->edges[found].t1=i; }
            else{
                Edge ne={ m->verts[v0].pos, m->verts[v1].pos, i, -1 };
                DA_PUSH(m->edges,m->nedges,m->cedges,ne);
            }
        }
    }
    free(weld);
}

/* signed volume via divergence theorem: positive = CCW outward. */
static float mesh_signed_volume(Mesh *m){
    float vol = 0.0f;
    for(int i=0;i<m->ntris;i++){
        Tri t=m->tris[i];
        vec3 a=m->verts[t.a].pos, b=m->verts[t.b].pos, c=m->verts[t.c].pos;
        vol += vdot(vcross(a,b),c);
    }
    return vol / 6.0f;
}
static void mesh_flip_winding(Mesh *m){
    for(int i=0;i<m->ntris;i++){ int t=m->tris[i].b; m->tris[i].b=m->tris[i].c; m->tris[i].c=t; }
    for(int i=0;i<m->nedges;i++){ vec3 p=m->edges[i].p0; m->edges[i].p0=m->edges[i].p1; m->edges[i].p1=p; }
}

/* ------------------------------------------------------- primitive gens */
/* All generators build LOCAL-space geometry. Flat-shaded faces duplicate
   vertices (one normal per face); smooth ones ("smoothSides") share a
   radial normal. Everything is centered on the local origin. */

static void add_quad(Mesh *m, vec3 a,vec3 b,vec3 c,vec3 d, vec3 n){
    int ia=mesh_add_vert(m,a,n), ib=mesh_add_vert(m,b,n),
        ic=mesh_add_vert(m,c,n), id=mesh_add_vert(m,d,n);
    mesh_add_tri(m,ia,ib,ic); mesh_add_tri(m,ia,ic,id);
}

static Mesh gen_box(float sx,float sy,float sz){
    Mesh m={0};
    float x=sx*0.5f,y=sy*0.5f,z=sz*0.5f;
    vec3 p[8]={ v3(-x,-y,-z),v3(x,-y,-z),v3(x,y,-z),v3(-x,y,-z),
                v3(-x,-y, z),v3(x,-y, z),v3(x,y, z),v3(-x,y, z) };
    add_quad(&m,p[0],p[1],p[2],p[3], v3(0,0,-1)); /* -Z */
    add_quad(&m,p[5],p[4],p[7],p[6], v3(0,0, 1)); /* +Z */
    add_quad(&m,p[4],p[0],p[3],p[7], v3(-1,0,0)); /* -X */
    add_quad(&m,p[1],p[5],p[6],p[2], v3( 1,0,0)); /* +X */
    add_quad(&m,p[4],p[5],p[1],p[0], v3(0,-1,0)); /* -Y */
    add_quad(&m,p[3],p[2],p[6],p[7], v3(0, 1,0)); /* +Y */
    return m;
}

/* generalized cylinder/cone/prism/pyramid: N sides, independent bottom/top
   radius (0 = a point, i.e. a cone/pyramid apex), capped ends. */
static Mesh gen_cylinder_like(int sides,float rBot,float rTop,float height,int smooth){
    Mesh m={0}; if(sides<3) sides=3;
    float hy=height*0.5f;
    for(int i=0;i<sides;i++){
        float a0=(float)i/sides*2.0f*(float)M_PI, a1=(float)(i+1)/sides*2.0f*(float)M_PI;
        vec3 b0=v3(cosf(a0)*rBot,-hy,sinf(a0)*rBot), b1=v3(cosf(a1)*rBot,-hy,sinf(a1)*rBot);
        vec3 t0=v3(cosf(a0)*rTop, hy,sinf(a0)*rTop), t1=v3(cosf(a1)*rTop, hy,sinf(a1)*rTop);
        vec3 flatN=vnorm(vcross(vsub(t0,b0),vsub(b1,b0)));
        if(smooth){
            vec3 n0=vnorm(v3(cosf(a0),0,sinf(a0))), n1=vnorm(v3(cosf(a1),0,sinf(a1)));
            int ib0=mesh_add_vert(&m,b0,n0), ib1=mesh_add_vert(&m,b1,n1);
            int it0=mesh_add_vert(&m,t0,n0), it1=mesh_add_vert(&m,t1,n1);
            if(rBot>1e-6f) mesh_add_tri(&m,ib0,ib1,it1);
            if(rTop>1e-6f || rBot>1e-6f) mesh_add_tri(&m,ib0,it1,it0);
        } else {
            add_quad(&m,b0,b1,t1,t0,flatN);
        }
    }
    if(rBot>1e-6f){ /* bottom cap fan */
        vec3 center=v3(0,-hy,0);
        for(int i=0;i<sides;i++){
            float a0=(float)i/sides*2.0f*(float)M_PI, a1=(float)(i+1)/sides*2.0f*(float)M_PI;
            vec3 b0=v3(cosf(a0)*rBot,-hy,sinf(a0)*rBot), b1=v3(cosf(a1)*rBot,-hy,sinf(a1)*rBot);
            vec3 n=v3(0,-1,0);
            int ic=mesh_add_vert(&m,center,n), i0=mesh_add_vert(&m,b1,n), i1=mesh_add_vert(&m,b0,n);
            mesh_add_tri(&m,ic,i0,i1);
        }
    }
    if(rTop>1e-6f){ /* top cap fan */
        vec3 center=v3(0,hy,0);
        for(int i=0;i<sides;i++){
            float a0=(float)i/sides*2.0f*(float)M_PI, a1=(float)(i+1)/sides*2.0f*(float)M_PI;
            vec3 t0=v3(cosf(a0)*rTop,hy,sinf(a0)*rTop), t1=v3(cosf(a1)*rTop,hy,sinf(a1)*rTop);
            vec3 n=v3(0,1,0);
            int ic=mesh_add_vert(&m,center,n), i0=mesh_add_vert(&m,t0,n), i1=mesh_add_vert(&m,t1,n);
            mesh_add_tri(&m,ic,i0,i1);
        }
    }
    return m;
}
static Mesh gen_cylinder(float r,float h,int sides){ return gen_cylinder_like(sides<=0?24:sides,r,r,h,1); }
static Mesh gen_prism(float r,float h,int sides){ return gen_cylinder_like(sides<3?6:sides,r,r,h,0); }
/* cone/pyramid: sides=4 with default square footprint gives a pyramid */
static Mesh gen_cone(float rBase,float rTop,float h,int sides){
    return gen_cylinder_like(sides<3?4:sides, rBase, rTop, h, sides>=16);
}

static Mesh gen_sphere(float r,int rings,int slices){
    Mesh m={0}; if(rings<3) rings=12; if(slices<3) slices=16;
    for(int i=0;i<=rings;i++){
        float v=(float)i/rings, phi=v*(float)M_PI; /* 0..pi */
        for(int j=0;j<=slices;j++){
            float u=(float)j/slices, th=u*2.0f*(float)M_PI;
            vec3 n=v3(sinf(phi)*cosf(th), cosf(phi), sinf(phi)*sinf(th));
            mesh_add_vert(&m, vscale(n,r), n);
        }
    }
    int stride=slices+1;
    for(int i=0;i<rings;i++) for(int j=0;j<slices;j++){
        int a=i*stride+j, b=a+1, c=(i+1)*stride+j, d=c+1;
        mesh_add_tri(&m,a,b,d); mesh_add_tri(&m,a,d,c);
    }
    return m;
}

static Mesh gen_torus(float R,float r,int majorSeg,int minorSeg){
    Mesh m={0}; if(majorSeg<3) majorSeg=24; if(minorSeg<3) minorSeg=12;
    for(int i=0;i<=majorSeg;i++){
        float u=(float)i/majorSeg*2.0f*(float)M_PI;
        for(int j=0;j<=minorSeg;j++){
            float v=(float)j/minorSeg*2.0f*(float)M_PI;
            vec3 n=v3(cosf(u)*cosf(v), sinf(v), sinf(u)*cosf(v));
            vec3 p=v3((R+r*cosf(v))*cosf(u), r*sinf(v), (R+r*cosf(v))*sinf(u));
            mesh_add_vert(&m,p,n);
        }
    }
    int stride=minorSeg+1;
    for(int i=0;i<majorSeg;i++) for(int j=0;j<minorSeg;j++){
        int a=i*stride+j, b=a+1, c=(i+1)*stride+j, d=c+1;
        mesh_add_tri(&m,a,d,b); mesh_add_tri(&m,a,c,d);
    }
    return m;
}

/* -------------------------------------------------------------- Tiny XML */
/* Just enough XML to read our scene schema: elements, quoted attributes,
   nesting, self-closing tags, comments. No entities/CDATA/namespaces. */
typedef struct XmlAttr { char *name, *value; } XmlAttr;
typedef struct XmlNode {
    char *tag;
    XmlAttr *attrs; int nattrs,cattrs;
    struct XmlNode **kids; int nkids,ckids;
} XmlNode;

static XmlNode* xml_new(const char*tag){
    XmlNode *n=calloc(1,sizeof(XmlNode)); n->tag=strdup(tag); return n;
}
static void xml_free(XmlNode*n){
    if(!n) return;
    for(int i=0;i<n->nattrs;i++){ free(n->attrs[i].name); free(n->attrs[i].value); }
    free(n->attrs);
    for(int i=0;i<n->nkids;i++) xml_free(n->kids[i]);
    free(n->kids); free(n->tag); free(n);
}
static const char* xml_attr(XmlNode*n,const char*name,const char*def){
    for(int i=0;i<n->nattrs;i++) if(!strcmp(n->attrs[i].name,name)) return n->attrs[i].value;
    return def;
}
static vec3 xml_attr_v3(XmlNode*n,const char*name,vec3 def){
    const char*s=xml_attr(n,name,NULL); if(!s) return def;
    vec3 v=def; sscanf(s,"%f %f %f",&v.x,&v.y,&v.z); return v;
}
static float xml_attr_f(XmlNode*n,const char*name,float def){
    const char*s=xml_attr(n,name,NULL); return s? (float)atof(s): def;
}
static int xml_attr_i(XmlNode*n,const char*name,int def){
    const char*s=xml_attr(n,name,NULL); return s? atoi(s): def;
}

static void xp_skip_ws(const char**p){ while(**p && isspace((unsigned char)**p)) (*p)++; }

static XmlNode* xml_parse_node(const char **p);

static void xml_parse_children(const char **p, XmlNode *parent){
    for(;;){
        xp_skip_ws(p);
        if(!**p) return;
        if(!strncmp(*p,"</",2)){ /* closing tag for parent, consumed by caller */ return; }
        if(!strncmp(*p,"<!--",4)){
            const char *end=strstr(*p,"-->");
            *p = end? end+3 : *p+strlen(*p);
            continue;
        }
        if(**p=='<'){
            XmlNode *child=xml_parse_node(p);
            if(!child) return;
            DA_PUSH(parent->kids,parent->nkids,parent->ckids,child);
            continue;
        }
        /* stray text content: skip to next '<' */
        while(**p && **p!='<') (*p)++;
    }
}

static XmlNode* xml_parse_node(const char **p){
    xp_skip_ws(p);
    if(**p!='<') return NULL;
    if(!strncmp(*p,"<?",2)){ const char*e=strstr(*p,"?>"); *p=e?e+2:*p+strlen(*p); return xml_parse_node(p); }
    if(!strncmp(*p,"<!--",4)){ const char*e=strstr(*p,"-->"); *p=e?e+3:*p+strlen(*p); return xml_parse_node(p); }
    (*p)++; /* '<' */
    char tag[64]; int ti=0;
    while(**p && !isspace((unsigned char)**p) && **p!='>' && **p!='/' && ti<63) tag[ti++]=*(*p)++;
    tag[ti]=0;
    XmlNode *n=xml_new(tag);
    for(;;){
        xp_skip_ws(p);
        if(!**p) return n;
        if(**p=='/' && (*p)[1]=='>'){ (*p)+=2; return n; }
        if(**p=='>'){ (*p)++; break; }
        char aname[64]; int ai=0;
        while(**p && **p!='=' && !isspace((unsigned char)**p) && **p!='>' && **p!='/' && ai<63) aname[ai++]=*(*p)++;
        aname[ai]=0;
        xp_skip_ws(p);
        char aval[256]={0};
        if(**p=='='){
            (*p)++; xp_skip_ws(p);
            if(**p=='"'||**p=='\''){
                char q=*(*p)++; int vi=0;
                while(**p && **p!=q && vi<255) aval[vi++]=*(*p)++;
                if(**p==q) (*p)++;
                aval[vi]=0;
            }
        }
        if(ai>0){
            XmlAttr a={ strdup(aname), strdup(aval) };
            DA_PUSH(n->attrs,n->nattrs,n->cattrs,a);
        }
    }
    /* children until </tag> */
    xml_parse_children(p, n);
    xp_skip_ws(p);
    if(!strncmp(*p,"</",2)){
        const char *end=strchr(*p,'>');
        *p = end? end+1 : *p+strlen(*p);
    }
    return n;
}
static XmlNode* xml_parse(const char *buf){
    const char *p=buf;
    for(;;){
        xp_skip_ws(&p);
        if(!*p) return NULL;
        if(!strncmp(p,"<?",2)){ const char*e=strstr(p,"?>"); p=e?e+2:p+strlen(p); continue; }
        if(!strncmp(p,"<!--",4)){ const char*e=strstr(p,"-->"); p=e?e+3:p+strlen(p); continue; }
        break;
    }
    return xml_parse_node(&p);
}

/* ------------------------------------------------------------- Scene ------ */
typedef struct { char id[32]; vec3 color; float shininess; } Material;
typedef struct { vec3 pos,color; float intensity; int castsShadow; } Light;
typedef struct { Mesh mesh; vec3 color; float shininess; int castsShadow; } SceneObj;
typedef struct { vec3 *verts; int nverts,cverts; } ShadowVolume; /* flat vec3 triples, 3 per triangle */

typedef struct {
    vec3 camPos, camLook; float camFov;
    vec3 ambient, bg;
    Light *lights; int nlights,clights;
    Material *mats; int nmats,cmats;
    SceneObj *objs; int nobjs,cobjs;
    ShadowVolume *svols; /* one per light, parallel to `lights` */
} Scene;

static Material* find_material(Scene*s,const char*id){
    if(!id) return NULL;
    for(int i=0;i<s->nmats;i++) if(!strcmp(s->mats[i].id,id)) return &s->mats[i];
    return NULL;
}

static void scene_add_obj(Scene *s, Mesh mesh, mat4 M, mat4 R, vec3 color,float shin,int castsShadow){
    mesh_transform(&mesh, M, R);
    if(castsShadow && mesh_signed_volume(&mesh) < 0.0f) mesh_flip_winding(&mesh);
    mesh_compute_face_normals(&mesh);
    if(castsShadow) mesh_build_edges(&mesh);
    SceneObj o={ mesh, color, shin, castsShadow };
    DA_PUSH(s->objs,s->nobjs,s->cobjs,o);
}

/* build the boxes that make up a wall with rectangular openings punched
   into it -- the "boolean via boxes" approach described at the top of the
   file. Local wall space: X along length (0..L, centered at wall pos),
   Y = height (0..H, floor at wall pos.y), Z = thickness (centered). */
typedef struct { float x,width,height,sill; } Opening;
static void build_wall_boxes(Scene *s, mat4 wallM, mat4 wallR, float L,float H,float T,
                              Opening *openings,int nopen, vec3 color,float shin){
    /* collect + sort X breakpoints */
    float *bp=NULL; int nbp=0,cbp=0;
    float b0=0,bL=L; DA_PUSH(bp,nbp,cbp,b0); DA_PUSH(bp,nbp,cbp,bL);
    for(int i=0;i<nopen;i++){
        float a=openings[i].x, b=openings[i].x+openings[i].width;
        DA_PUSH(bp,nbp,cbp,a); DA_PUSH(bp,nbp,cbp,b);
    }
    for(int i=0;i<nbp;i++) for(int j=i+1;j<nbp;j++) if(bp[j]<bp[i]){ float t=bp[i]; bp[i]=bp[j]; bp[j]=t; }

    for(int i=0;i+1<nbp;i++){
        float x0=bp[i], x1=bp[i+1];
        if(x1-x0 < 1e-4f) continue;
        float xm=(x0+x1)*0.5f;
        /* is [x0,x1] inside some opening? (midpoint test is enough since
           breakpoints already fall exactly on opening edges) */
        Opening *hit=NULL;
        for(int k=0;k<nopen;k++){
            if(xm>openings[k].x && xm<openings[k].x+openings[k].width){ hit=&openings[k]; break; }
        }
        float segs[2][2]; int nseg=0; /* [y0,y1] pairs */
        if(!hit){ segs[0][0]=0; segs[0][1]=H; nseg=1; }
        else {
            if(hit->sill>1e-4f){ segs[nseg][0]=0; segs[nseg][1]=hit->sill; nseg++; }
            float top=hit->sill+hit->height;
            if(top<H-1e-4f){ segs[nseg][0]=top; segs[nseg][1]=H; nseg++; }
        }
        for(int si=0;si<nseg;si++){
            float y0=segs[si][0], y1=segs[si][1];
            float w=x1-x0, h=y1-y0;
            if(w<1e-4f||h<1e-4f) continue;
            vec3 localCenter = v3(xm - L*0.5f, y0+h*0.5f, 0);
            Mesh box=gen_box(w,h,T);
            mat4 M = mat4_mul(wallM, mat4_translate(localCenter));
            scene_add_obj(s, box, M, wallR, color, shin, 1);
        }
    }
    free(bp);
}

/* recursively parse <box>/<sphere>/.../<group>/<wall> under `parentM`,`parentR` */
static void parse_nodes(Scene *s, XmlNode *parent, mat4 parentM, mat4 parentR){
    for(int i=0;i<parent->nkids;i++){
        XmlNode *n=parent->kids[i];
        vec3 pos=xml_attr_v3(n,"pos",v3(0,0,0));
        vec3 rot=xml_attr_v3(n,"rot",v3(0,0,0));
        vec3 scl=xml_attr_v3(n,"scale",v3(1,1,1));
        mat4 R = mat4_mul(parentR, mat4_rot_xyz(rot));
        mat4 M = mat4_mul(parentM, mat4_mul(mat4_translate(pos), mat4_mul(mat4_rot_xyz(rot), mat4_scale(scl))));
        Material *mat = find_material(s, xml_attr(n,"material",NULL));
        vec3 color = mat? mat->color : xml_attr_v3(n,"color",v3(0.8f,0.8f,0.8f));
        float shin = mat? mat->shininess : xml_attr_f(n,"shininess",8.0f);
        int castsShadow = xml_attr_i(n,"castShadow",1);

        if(!strcmp(n->tag,"box")){
            vec3 sz=xml_attr_v3(n,"size",v3(1,1,1));
            scene_add_obj(s, gen_box(sz.x,sz.y,sz.z), M,R, color,shin,castsShadow);
        } else if(!strcmp(n->tag,"sphere")){
            float r=xml_attr_f(n,"radius",0.5f);
            scene_add_obj(s, gen_sphere(r,xml_attr_i(n,"rings",16),xml_attr_i(n,"slices",24)), M,R, color,shin,castsShadow);
        } else if(!strcmp(n->tag,"cylinder")){
            float r=xml_attr_f(n,"radius",0.5f), h=xml_attr_f(n,"height",1.0f);
            scene_add_obj(s, gen_cylinder(r,h,xml_attr_i(n,"sides",24)), M,R, color,shin,castsShadow);
        } else if(!strcmp(n->tag,"prism")){
            float r=xml_attr_f(n,"radius",0.5f), h=xml_attr_f(n,"height",1.0f);
            scene_add_obj(s, gen_prism(r,h,xml_attr_i(n,"sides",6)), M,R, color,shin,castsShadow);
        } else if(!strcmp(n->tag,"cone")||!strcmp(n->tag,"pyramid")){
            float rb=xml_attr_f(n,"radius",0.5f), rt=xml_attr_f(n,"radiusTop",0.0f), h=xml_attr_f(n,"height",1.0f);
            int sides = xml_attr_i(n,"sides", !strcmp(n->tag,"pyramid")?4:24);
            scene_add_obj(s, gen_cone(rb,rt,h,sides), M,R, color,shin,castsShadow);
        } else if(!strcmp(n->tag,"torus")){
            float R_=xml_attr_f(n,"majorRadius",0.5f), r_=xml_attr_f(n,"minorRadius",0.15f);
            scene_add_obj(s, gen_torus(R_,r_,xml_attr_i(n,"majorSegments",24),xml_attr_i(n,"minorSegments",12)), M,R, color,shin,castsShadow);
        } else if(!strcmp(n->tag,"group")){
            parse_nodes(s, n, M, R);
        } else if(!strcmp(n->tag,"wall")){
            float L=xml_attr_f(n,"length",4.0f), H=xml_attr_f(n,"height",2.7f), T=xml_attr_f(n,"thickness",0.2f);
            Opening *op=NULL; int nop=0,cop=0;
            for(int k=0;k<n->nkids;k++){
                XmlNode *c=n->kids[k];
                if(strcmp(c->tag,"opening")) continue;
                Opening o;
                o.x = xml_attr_f(c,"x",0);
                o.width = xml_attr_f(c,"width",1.0f);
                int isDoor = !strcmp(xml_attr(c,"type","door"),"door");
                o.height = xml_attr_f(c,"height", isDoor?2.1f:1.2f);
                o.sill = isDoor? 0.0f : xml_attr_f(c,"sill",0.9f);
                DA_PUSH(op,nop,cop,o);
            }
            /* wall's own pos/rot were already folded into M/R above, but we
               need the *un-scaled* wall-local transform for box placement */
            mat4 wallM = mat4_mul(parentM, mat4_mul(mat4_translate(pos), mat4_rot_xyz(rot)));
            build_wall_boxes(s, wallM, R, L,H,T, op,nop, color, shin);
            free(op);
        }
    }
}

static char* read_file(const char*path){
    FILE *f=fopen(path,"rb"); if(!f){ fprintf(stderr,"cannot open %s\n",path); return NULL; }
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    char *buf=malloc((size_t)n+1);
    size_t rd=fread(buf,1,(size_t)n,f); buf[rd]=0; fclose(f);
    return buf;
}

static int load_scene(const char *path, Scene *s){
    memset(s,0,sizeof(*s));
    s->camPos=v3(0,1.6f,5); s->camLook=v3(0,1.2f,0); s->camFov=60;
    s->ambient=v3(0.12f,0.12f,0.14f); s->bg=v3(0.05f,0.06f,0.08f);

    char *buf=read_file(path); if(!buf) return 0;
    XmlNode *root=xml_parse(buf); free(buf);
    if(!root){ fprintf(stderr,"failed to parse %s\n",path); return 0; }

    for(int i=0;i<root->nkids;i++){
        XmlNode *n=root->kids[i];
        if(!strcmp(n->tag,"camera")){
            s->camPos = xml_attr_v3(n,"pos",s->camPos);
            s->camLook = xml_attr_v3(n,"look",s->camLook);
            s->camFov = xml_attr_f(n,"fov",s->camFov);
        } else if(!strcmp(n->tag,"ambient")){
            s->ambient = xml_attr_v3(n,"color",s->ambient);
        } else if(!strcmp(n->tag,"background")){
            s->bg = xml_attr_v3(n,"color",s->bg);
        } else if(!strcmp(n->tag,"material")){
            Material m={0}; strncpy(m.id, xml_attr(n,"id","mat"), 31);
            m.color = xml_attr_v3(n,"color",v3(0.8f,0.8f,0.8f));
            m.shininess = xml_attr_f(n,"shininess",8.0f);
            DA_PUSH(s->mats,s->nmats,s->cmats,m);
        } else if(!strcmp(n->tag,"light")){
            Light L={0};
            L.pos = xml_attr_v3(n,"pos",v3(0,3,0));
            L.color = xml_attr_v3(n,"color",v3(1,1,1));
            L.intensity = xml_attr_f(n,"intensity",1.0f);
            L.castsShadow = xml_attr_i(n,"castShadows",1);
            DA_PUSH(s->lights,s->nlights,s->clights,L);
        }
    }
    mat4 I=mat4_identity();
    parse_nodes(s, root, I, I);
    xml_free(root);

    /* precompute shadow volumes (scene is static -- see design note up top) */
    s->svols = calloc((size_t)s->nlights, sizeof(ShadowVolume));
    return 1;
}

/* ------------------------------------------------------ shadow volumes -- */
#define SHADOW_EXTRUDE 200.0f

static void build_shadow_volume(Mesh *m, vec3 lightPos, ShadowVolume *sv){
    memset(sv,0,sizeof(*sv));
    int nt=m->ntris;
    char *facing=malloc((size_t)nt);
    for(int i=0;i<nt;i++){
        vec3 p=m->verts[m->tris[i].a].pos;
        facing[i] = vdot(m->triN[i], vsub(lightPos,p)) > 0.0f;
    }
    #define PUSHV(vv) DA_PUSH(sv->verts,sv->nverts,sv->cverts,(vv))
    /* side quads from silhouette edges */
    for(int i=0;i<m->nedges;i++){
        Edge *e=&m->edges[i];
        int f0=facing[e->t0], f1=(e->t1>=0)?facing[e->t1]:0;
        int silhouette = (e->t1<0) ? f0 : (f0!=f1);
        if(!silhouette) continue;
        vec3 v0,v1;
        if(f0){ v0=e->p0; v1=e->p1; } else { v0=e->p1; v1=e->p0; }
        vec3 v0e = vadd(lightPos, vscale(vsub(v0,lightPos), SHADOW_EXTRUDE));
        vec3 v1e = vadd(lightPos, vscale(vsub(v1,lightPos), SHADOW_EXTRUDE));
        PUSHV(v0); PUSHV(v1); PUSHV(v1e);
        PUSHV(v0); PUSHV(v1e); PUSHV(v0e);
    }
    /* near cap (as-is) + far cap (extruded, reversed winding) */
    for(int i=0;i<nt;i++){
        if(!facing[i]) continue;
        Tri t=m->tris[i];
        vec3 a=m->verts[t.a].pos, b=m->verts[t.b].pos, c=m->verts[t.c].pos;
        PUSHV(a); PUSHV(b); PUSHV(c);
        vec3 ae=vadd(lightPos,vscale(vsub(a,lightPos),SHADOW_EXTRUDE));
        vec3 be=vadd(lightPos,vscale(vsub(b,lightPos),SHADOW_EXTRUDE));
        vec3 ce=vadd(lightPos,vscale(vsub(c,lightPos),SHADOW_EXTRUDE));
        PUSHV(ae); PUSHV(ce); PUSHV(be);
    }
    #undef PUSHV
    free(facing);
}

static void scene_build_all_shadow_volumes(Scene *s){
    for(int li=0; li<s->nlights; li++){
        ShadowVolume combined={0};
        if(s->lights[li].castsShadow){
            for(int oi=0; oi<s->nobjs; oi++){
                SceneObj *o=&s->objs[oi];
                if(!o->castsShadow) continue;
                ShadowVolume part;
                build_shadow_volume(&o->mesh, s->lights[li].pos, &part);
                for(int k=0;k<part.nverts;k++) DA_PUSH(combined.verts,combined.nverts,combined.cverts,part.verts[k]);
                free(part.verts);
            }
        }
        s->svols[li]=combined;
    }
}

/* -------------------------------------------------------------- drawing */
static void draw_mesh_flat(Mesh *m, vec3 color){
    glColor3f(color.x,color.y,color.z);
    glBegin(GL_TRIANGLES);
    for(int i=0;i<m->ntris;i++){
        Tri t=m->tris[i];
        glVertex3fv(&m->verts[t.a].pos.x);
        glVertex3fv(&m->verts[t.b].pos.x);
        glVertex3fv(&m->verts[t.c].pos.x);
    }
    glEnd();
}
static void draw_mesh_lit(Mesh *m, vec3 color, float shininess){
    glColor3f(color.x,color.y,color.z);
    GLfloat spec[4]={0.25f,0.25f,0.25f,1.0f};
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,spec);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,shininess);
    glBegin(GL_TRIANGLES);
    for(int i=0;i<m->ntris;i++){
        Tri t=m->tris[i];
        glNormal3fv(&m->verts[t.a].nrm.x); glVertex3fv(&m->verts[t.a].pos.x);
        glNormal3fv(&m->verts[t.b].nrm.x); glVertex3fv(&m->verts[t.b].pos.x);
        glNormal3fv(&m->verts[t.c].nrm.x); glVertex3fv(&m->verts[t.c].pos.x);
    }
    glEnd();
}
static void draw_shadow_volume(ShadowVolume *sv){
    glBegin(GL_TRIANGLES);
    for(int i=0;i<sv->nverts;i++) glVertex3fv(&sv->verts[i].x);
    glEnd();
}

static void render_frame(Scene *s, int w,int h, mat4 proj, mat4 view){
    glViewport(0,0,w,h);
    glClearColor(s->bg.x,s->bg.y,s->bg.z,1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION); glLoadMatrixf(proj.m);
    glMatrixMode(GL_MODELVIEW);  glLoadMatrixf(view.m);

    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK);

    /* ---- pass 1: ambient-only base (also fills the depth buffer) ---- */
    glDisable(GL_LIGHTING);
    for(int i=0;i<s->nobjs;i++)
        draw_mesh_flat(&s->objs[i].mesh, vmul(s->objs[i].color, s->ambient));

    /* ---- pass 2..N: one stencil-shadow-volume + additive lit pass per light --- */
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL); glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    GLfloat zero[4]={0,0,0,1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
    glLightfv(GL_LIGHT0, GL_AMBIENT, zero);

    for(int li=0; li<s->nlights; li++){
        Light *L=&s->lights[li];
        GLfloat lp[4]={L->pos.x,L->pos.y,L->pos.z,1.0f};
        GLfloat lc[4]={L->color.x*L->intensity, L->color.y*L->intensity, L->color.z*L->intensity, 1.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, lp);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lc);
        glLightfv(GL_LIGHT0, GL_SPECULAR, lc);

        if(L->castsShadow && s->svols[li].nverts>0){
            /* --- stencil pass: mark pixels in shadow, z-fail method --- */
            glClear(GL_STENCIL_BUFFER_BIT);
            glDisable(GL_LIGHTING);
            glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
            glDepthMask(GL_FALSE);
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS,0,0xFF);
            glDisable(GL_CULL_FACE);
            glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
            glStencilOpSeparate(GL_FRONT,GL_KEEP, GL_DECR_WRAP, GL_KEEP);
            draw_shadow_volume(&s->svols[li]);
            glEnable(GL_CULL_FACE);

            /* --- lit pass: additive, only where stencil == 0 --- */
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            glStencilFunc(GL_EQUAL,0,0xFF);
            glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE);
            glEnable(GL_LIGHTING);
            for(int i=0;i<s->nobjs;i++)
                draw_mesh_lit(&s->objs[i].mesh, s->objs[i].color, s->objs[i].shininess);
            glDisable(GL_BLEND);
            glDisable(GL_STENCIL_TEST);
            glDepthFunc(GL_LESS);
        } else {
            /* non-shadow-casting light: simple additive pass, no stencil */
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE);
            for(int i=0;i<s->nobjs;i++)
                draw_mesh_lit(&s->objs[i].mesh, s->objs[i].color, s->objs[i].shininess);
            glDisable(GL_BLEND);
            glDepthFunc(GL_LESS);
        }
    }
    glDepthMask(GL_TRUE);
}

/* ------------------------------------------------------------------ main */
int main(int argc,char**argv){
    const char *scenePath = argc>1? argv[1] : "scenes/sample_room.xml";
    Scene scene;
    if(!load_scene(scenePath,&scene)) return 1;
    scene_build_all_shadow_volumes(&scene);
    fprintf(stderr,"loaded %d objects, %d lights, %d materials\n",
            scene.nobjs, scene.nlights, scene.nmats);

    if(SDL_Init(SDL_INIT_VIDEO)!=0){ fprintf(stderr,"SDL_Init: %s\n",SDL_GetError()); return 1; }
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);

    int W=1024,H=768;
    SDL_Window *win=SDL_CreateWindow("simplegl",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        W,H, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!win){ fprintf(stderr,"CreateWindow: %s\n",SDL_GetError()); return 1; }
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    if(!ctx){ fprintf(stderr,"GL_CreateContext: %s\n",SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    /* camera: derive yaw/pitch from the scene's initial look-at */
    vec3 fwd=vnorm(vsub(scene.camLook,scene.camPos));
    float yaw = atan2f(fwd.x,-fwd.z) * 180.0f/(float)M_PI;
    float pitch = asinf(fwd.y) * 180.0f/(float)M_PI;
    vec3 pos=scene.camPos;

    int running=1;
    Uint32 lastTicks=SDL_GetTicks();
    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT) running=0;
            else if(ev.type==SDL_KEYDOWN && ev.key.keysym.sym==SDLK_ESCAPE) running=0;
            else if(ev.type==SDL_MOUSEMOTION){
                yaw   += ev.motion.xrel * 0.15f;
                pitch -= ev.motion.yrel * 0.15f;
                if(pitch>89) pitch=89;
                if(pitch<-89) pitch=-89;
            } else if(ev.type==SDL_WINDOWEVENT && ev.window.event==SDL_WINDOWEVENT_RESIZED){
                W=ev.window.data1; H=ev.window.data2;
            }
        }
        Uint32 now=SDL_GetTicks(); float dt=(now-lastTicks)/1000.0f; lastTicks=now;

        float yawR=yaw*(float)M_PI/180.0f, pitchR=pitch*(float)M_PI/180.0f;
        vec3 look = v3(sinf(yawR)*cosf(pitchR), sinf(pitchR), -cosf(yawR)*cosf(pitchR));
        vec3 right = vnorm(vcross(look, v3(0,1,0)));

        const Uint8 *ks=SDL_GetKeyboardState(NULL);
        float speed = (ks[SDL_SCANCODE_LSHIFT]||ks[SDL_SCANCODE_RSHIFT]) ? 6.0f : 2.5f;
        vec3 move={0,0,0};
        if(ks[SDL_SCANCODE_W]) move=vadd(move, look);
        if(ks[SDL_SCANCODE_S]) move=vsub(move, look);
        if(ks[SDL_SCANCODE_D]) move=vadd(move, right);
        if(ks[SDL_SCANCODE_A]) move=vsub(move, right);
        if(ks[SDL_SCANCODE_E]) move=vadd(move, v3(0,1,0));
        if(ks[SDL_SCANCODE_Q]) move=vsub(move, v3(0,1,0));
        if(vlen(move)>1e-6f) pos = vadd(pos, vscale(vnorm(move), speed*dt));

        mat4 proj = mat4_perspective(scene.camFov, (float)W/(float)H, 0.05f, 500.0f);
        mat4 view = mat4_lookat(pos, vadd(pos,look), v3(0,1,0));
        render_frame(&scene, W,H, proj, view);

        SDL_GL_SwapWindow(win);
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
