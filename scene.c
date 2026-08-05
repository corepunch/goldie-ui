#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "scene.h"
#include "mesh.h"
#include "math.h"

/* -------------------------------------------------------------- Tiny XML */
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
        if(!strncmp(*p,"</",2)){ return; }
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
        while(**p && **p!='<') (*p)++;
    }
}
static XmlNode* xml_parse_node(const char **p){
    xp_skip_ws(p);
    if(**p!='<') return NULL;
    if(!strncmp(*p,"<?",2)){ const char*e=strstr(*p,"?>"); *p=e?e+2:*p+strlen(*p); return xml_parse_node(p); }
    if(!strncmp(*p,"<!--",4)){ const char*e=strstr(*p,"-->"); *p=e?e+3:*p+strlen(*p); return xml_parse_node(p); }
    (*p)++;
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

void scene_free(Scene *s){
    for(int i=0;i<s->nobjs;i++) mesh_free(&s->objs[i].mesh);
    for(int i=0;i<s->nlights;i++) free(s->svols[i].verts);
    free(s->lights); free(s->mats); free(s->objs); free(s->svols);
    memset(s,0,sizeof(*s));
}

static Material* find_material(Scene*s,const char*id){
    if(!id) return NULL;
    for(int i=0;i<s->nmats;i++) if(!strcmp(s->mats[i].id,id)) return &s->mats[i];
    return NULL;
}

void scene_add_obj(Scene *s, Mesh mesh, mat4 M, mat4 R, vec3 color, float shin, int castsShadow){
    mesh_transform(&mesh, M, R);
    if(castsShadow && mesh_signed_volume(&mesh) < 0.0f) mesh_flip_winding(&mesh);
    mesh_compute_face_normals(&mesh);
    if(castsShadow) mesh_build_edges(&mesh);
    SceneObj o={ mesh, color, shin, castsShadow };
    DA_PUSH(s->objs,s->nobjs,s->cobjs,o);
}

/* build the boxes that make up a wall with rectangular openings */
typedef struct { float x,width,height,sill; } Opening;
static void build_wall_boxes(Scene *s, mat4 wallM, mat4 wallR, float L,float H,float T,
                              Opening *openings,int nopen, vec3 color,float shin){
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
        Opening *hit=NULL;
        for(int k=0;k<nopen;k++){
            if(xm>openings[k].x && xm<openings[k].x+openings[k].width){ hit=&openings[k]; break; }
        }
        float segs[2][2]; int nseg=0;
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

int load_scene(const char *path, Scene *s){
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

    s->svols = calloc((size_t)s->nlights, sizeof(ShadowVolume));
    return 1;
}
