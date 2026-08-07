#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "simplegl.h"

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
static int xml_attr_2f(XmlNode*n,const char*name,float defX,float defY,float *outX,float *outY){
	const char*s=xml_attr(n,name,NULL);
	if(!s){ *outX=defX; *outY=defY; return 0; }
	*outX=defX; *outY=defY;
	int count=sscanf(s,"%f %f",outX,outY);
	if(count==1) *outY=*outX;
	return count>0;
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
	for(int i=0;i<s->nprefabs;i++) xml_free((XmlNode*)s->prefabs[i].root);
	for(int i=0;i<s->nprefabs;i++) free(s->prefabs[i].attaches);
	for(int i=0;i<s->nobjs;i++) mesh_free(&s->objs[i].mesh);
	for(int i=0;i<s->nlights;i++) free(s->svols[i].verts);
	free(s->lights); free(s->mats); free(s->objs); free(s->svols); free(s->cameras); free(s->prefabs); free(s->instances); free(s->negativeBoxes); free(s->overlayLines); free(s->charDefs);
	memset(s,0,sizeof(*s));
}

static Material preset_materials[] = {
	{ "wall",     {0.80f,0.78f,0.72f}, 6.0f },
	{ "floor",    {0.35f,0.28f,0.22f}, 12.0f },
	{ "wood",     {0.50f,0.32f,0.18f}, 20.0f },
	{ "metal",    {0.70f,0.70f,0.75f}, 60.0f },
	{ "glass",    {0.65f,0.80f,0.85f}, 90.0f },
	{ "stone",    {0.38f,0.36f,0.33f}, 8.0f },
	{ "concrete", {0.52f,0.50f,0.46f}, 4.0f },
	{ "plaster",  {0.90f,0.88f,0.80f}, 3.0f },
	{ "bronze",   {0.48f,0.30f,0.14f}, 40.0f },
	{ "iron",     {0.28f,0.28f,0.30f}, 55.0f },
};
static const int npreset_mats = (int)(sizeof(preset_materials)/sizeof(preset_materials[0]));

static Material* find_material(Scene*s, const char*id){
	if(!id) return NULL;
	for(int i=0;i<s->nmats;i++) if(!strcmp(s->mats[i].id,id)) return &s->mats[i];
	for(int i=0;i<npreset_mats;i++) if(!strcmp(preset_materials[i].id,id)) return &preset_materials[i];
	return NULL;
}

void scene_add_obj(Scene *s, Mesh mesh, mat4 M, mat4 R, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	mesh_transform(&mesh, M, R);
	if(castsShadow && mesh_signed_volume(&mesh) < 0.0f) mesh_flip_winding(&mesh);
	mesh_compute_face_normals(&mesh);
	if(castsShadow) mesh_build_edges(&mesh);
	SceneObj o={ mesh, color, shin, castsShadow, renderable, unlit,
		s->sanityIgnoreActive, s->sanityFloorActive, s->sanityCheckActive };
	DA_PUSH(s->objs,s->nobjs,s->cobjs,o);
}

/* ---------------------------------------------------- Modifiers ---------- */

static char* read_file(const char*path);
static void warn_unknown_elements(XmlNode *root, const char *path, int prefab);

typedef void (*modifier_parser_fn)(Mesh *m, XmlNode *n);

static char mod_axis(XmlNode *n){ const char *a=xml_attr(n,"axis","y"); return a[0]? a[0] : 'y'; }

static void parse_mod_taper(Mesh *m, XmlNode *n){
	mesh_apply_taper(m, xml_attr_f(n,"amount",0.0f), xml_attr_f(n,"curvature",1.0f), mod_axis(n));
}
static void parse_mod_twist(Mesh *m, XmlNode *n){
	mesh_apply_twist(m, xml_attr_f(n,"angle",0.0f), mod_axis(n));
}
static void parse_mod_bend(Mesh *m, XmlNode *n){
	mesh_apply_bend(m, xml_attr_f(n,"angle",0.0f), mod_axis(n));
}
static void parse_mod_stretch(Mesh *m, XmlNode *n){
	mesh_apply_stretch(m, xml_attr_f(n,"amount",0.0f), xml_attr_f(n,"amplify",1.0f), mod_axis(n));
}
static void parse_mod_skew(Mesh *m, XmlNode *n){
	mesh_apply_skew(m, xml_attr_f(n,"amount",0.0f), mod_axis(n));
}
static void parse_mod_array(Mesh *m, XmlNode *n){
	mesh_apply_array(m, xml_attr_i(n,"count",1),
		xml_attr_v3(n,"translation",v3(0,0,0)),
		xml_attr_v3(n,"rotation",v3(0,0,0)));
}

static const struct {
	const char *tag;
	modifier_parser_fn parse;
} modifier_parsers[] = {
	{ "taper",   parse_mod_taper },
	{ "twist",   parse_mod_twist },
	{ "bend",    parse_mod_bend },
	{ "stretch", parse_mod_stretch },
	{ "skew",    parse_mod_skew },
	{ "array",   parse_mod_array },
};

static void apply_modifiers(Mesh *m, XmlNode *n){
	for(int i=0;i<n->nkids;i++){
		XmlNode *c=n->kids[i];
		for(int j=0;j<(int)(sizeof(modifier_parsers)/sizeof(modifier_parsers[0]));j++){
			if(!strcmp(c->tag, modifier_parsers[j].tag)){
				modifier_parsers[j].parse(m, c);
				break;
			}
		}
	}
}

/* ---------------------------------------------------- Overlay lines ------- */

static void scene_add_overlay_line(Scene *s, vec3 start, vec3 end, vec3 color, int category){
	OverlayLine ol={start,end,color,category};
	DA_PUSH(s->overlayLines,s->noverlayLines,s->coverlayLines,ol);
}

static void add_circle_lines(Scene *s, vec3 center, float radius, vec3 normal, vec3 color, int n, int category){
	vec3 u,v;
	if(fabsf(normal.x)>0.001f||fabsf(normal.z)>0.001f)
		u=vnorm(v3(-normal.z,0,normal.x));
	else
		u=vnorm(v3(1,0,0));
	v=vnorm(vcross(normal,u));
	vec3 prev=vadd(center,vscale(u,radius));
	for(int i=1;i<=n;i++){
		float angle=2.0f*M_PIf*(float)i/(float)n;
		vec3 next=vadd(center,vadd(vscale(u,radius*cosf(angle)),vscale(v,radius*sinf(angle))));
		scene_add_overlay_line(s,prev,next,color,category);
		prev=next;
	}
}

static void add_cross_lines(Scene *s, vec3 center, float radius, vec3 normal, vec3 color, int category){
	vec3 u,v;
	if(fabsf(normal.x)>0.001f||fabsf(normal.z)>0.001f)
		u=vnorm(v3(-normal.z,0,normal.x));
	else
		u=vnorm(v3(1,0,0));
	v=vnorm(vcross(normal,u));
	scene_add_overlay_line(s,vadd(center,vscale(u,radius)),vadd(center,vscale(u,-radius)),color,category);
	scene_add_overlay_line(s,vadd(center,vscale(v,radius)),vadd(center,vscale(v,-radius)),color,category);
}

static void add_character_dummy(Scene *s, vec3 pos, CharDef *cd, vec3 color){
	float h=cd->height, r=cd->radius;
	float yHead=pos.y+cd->top*h;
	float yNeck=pos.y+cd->neck*h;
	float yPelvis=pos.y+cd->pelvis*h;
	float yFeet=pos.y+cd->feet*h;
	float shoulderW=r*1.8f;
	float hipW=r*1.3f;
	vec3 up=v3(0,1,0);
	vec3 head=v3(pos.x,yHead,pos.z);
	vec3 neck=v3(pos.x,yNeck,pos.z);
	vec3 pelvis=v3(pos.x,yPelvis,pos.z);
	vec3 feet=v3(pos.x,yFeet,pos.z);
	vec3 shoulderL=v3(pos.x-shoulderW,yNeck,pos.z);
	vec3 shoulderR=v3(pos.x+shoulderW,yNeck,pos.z);
	vec3 hipL=v3(pos.x-hipW,yPelvis,pos.z);
	vec3 hipR=v3(pos.x+hipW,yPelvis,pos.z);
	vec3 legSplit=v3(pos.x,yPelvis-r*0.2f,pos.z);
	scene_add_overlay_line(s,feet,head,color,0);
	scene_add_overlay_line(s,shoulderL,shoulderR,color,0);
	scene_add_overlay_line(s,hipL,hipR,color,0);
	scene_add_overlay_line(s,shoulderL,hipL,color,0);
	scene_add_overlay_line(s,shoulderR,hipR,color,0);
	scene_add_overlay_line(s,neck,pelvis,color,0);
	scene_add_overlay_line(s,legSplit,hipL,color,0);
	scene_add_overlay_line(s,legSplit,hipR,color,0);
	add_circle_lines(s,head,r,up,color,16,0);
	add_cross_lines(s,head,r,up,color,0);
	add_circle_lines(s,neck,r*0.55f,up,color,16,0);
	add_circle_lines(s,pelvis,r*0.85f,up,color,16,0);
	add_cross_lines(s,pelvis,r*0.85f,up,color,0);
	add_circle_lines(s,feet,r*0.65f,up,color,16,0);
}

static void add_lamp_dummy(Scene *s, vec3 pos, float radius, vec3 color, int category){
	add_circle_lines(s,pos,radius,v3(1,0,0),color,16,category);
	add_circle_lines(s,pos,radius,v3(0,1,0),color,16,category);
	add_circle_lines(s,pos,radius,v3(0,0,1),color,16,category);
}

static void add_camera_dummy(Scene *s, vec3 pos, vec3 look, float fov, float aspect, vec3 color, int category){
	vec3 forward=vnorm(vsub(look,pos));
	vec3 worldUp=v3(0,1,0);
	vec3 right=vnorm(vcross(forward,worldUp));
	vec3 up=vnorm(vcross(right,forward));
	float dist=0.3f;
	float hh=dist*tanf(fov*M_PIf/360.0f);
	float hw=hh*aspect;
	vec3 center=vadd(pos,vscale(forward,dist));
	vec3 tl=vadd(center,vadd(vscale(up,hh),vscale(right,-hw)));
	vec3 tr=vadd(center,vadd(vscale(up,hh),vscale(right,hw)));
	vec3 bl=vadd(center,vadd(vscale(up,-hh),vscale(right,-hw)));
	vec3 br=vadd(center,vadd(vscale(up,-hh),vscale(right,hw)));
	scene_add_overlay_line(s,pos,tl,color,category); scene_add_overlay_line(s,pos,tr,color,category);
	scene_add_overlay_line(s,pos,bl,color,category); scene_add_overlay_line(s,pos,br,color,category);
	scene_add_overlay_line(s,tl,tr,color,category); scene_add_overlay_line(s,tr,br,color,category);
	scene_add_overlay_line(s,br,bl,color,category); scene_add_overlay_line(s,bl,tl,color,category);
}

void scene_rebuild_camera_gizmos(Scene *s, float aspect){
	int w=0;
	for(int i=0;i<s->noverlayLines;i++){
		if(s->overlayLines[i].category!=2) s->overlayLines[w++]=s->overlayLines[i];
	}
	s->noverlayLines=w;
	for(int ci=0;ci<s->ncameras;ci++){
		Camera *c=&s->cameras[ci];
		add_camera_dummy(s,c->pos,c->look,c->fov,aspect,v3(0.2f,0.8f,0.2f),2);
	}
}

/* ---------------------------------------------------- Shape parsers ------- */

static void parse_nodes(Scene *s, XmlNode *parent, mat4 parentM, mat4 parentR);

typedef void (*shape_parser_fn)(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit);

static void parse_box(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)parentM; (void)pos; (void)rot;
	vec3 sz=xml_attr_v3(n,"size",v3(1,1,1));
	float insetX=0.0f, insetY=0.0f;
	xml_attr_2f(n,"inset",0.0f,0.0f,&insetX,&insetY);
	Mesh mesh=(insetX>0.0f || insetY>0.0f) ? gen_box_inset(sz.x,sz.y,sz.z,insetX,insetY)
		: gen_box(sz.x,sz.y,sz.z);
	apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow,renderable,unlit);
}

static void parse_sphere(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)parentM; (void)pos; (void)rot;
	float r=xml_attr_f(n,"radius",0.5f);
	Mesh mesh=gen_sphere(r,xml_attr_i(n,"rings",16),xml_attr_i(n,"slices",24)); apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow,renderable,unlit);
}

static void parse_cylinder(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)parentM; (void)pos; (void)rot;
	float r=xml_attr_f(n,"radius",0.5f), h=xml_attr_f(n,"height",1.0f);
	float wall=xml_attr_f(n,"tube",0.0f);
	Mesh mesh=wall>0.0f ? gen_cylinder_tube(r,h,wall,xml_attr_i(n,"sides",24))
		: gen_cylinder(r,h,xml_attr_i(n,"sides",24));
	apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow,renderable,unlit);
}

static void parse_prism(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)parentM; (void)pos; (void)rot;
	float r=xml_attr_f(n,"radius",0.5f), h=xml_attr_f(n,"height",1.0f);
	Mesh mesh=gen_prism(r,h,xml_attr_i(n,"sides",6)); apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow,renderable,unlit);
}

static void parse_cone(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)parentM; (void)pos; (void)rot;
	float rb=xml_attr_f(n,"radius",0.5f), rt=xml_attr_f(n,"radiusTop",0.0f), h=xml_attr_f(n,"height",1.0f);
	int sides = xml_attr_i(n,"sides", !strcmp(n->tag,"pyramid")?4:24);
	Mesh mesh=gen_cone(rb,rt,h,sides); apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow,renderable,unlit);
}

static void parse_torus(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)parentM; (void)pos; (void)rot;
	float R_=xml_attr_f(n,"majorRadius",0.5f), r_=xml_attr_f(n,"minorRadius",0.15f);
	Mesh mesh=gen_torus(R_,r_,xml_attr_i(n,"majorSegments",24),xml_attr_i(n,"minorSegments",12)); apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow,renderable,unlit);
}

static void parse_arch(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)parentM; (void)pos; (void)rot;
	float w=xml_attr_f(n,"width",1.0f), h=xml_attr_f(n,"height",1.5f), d=xml_attr_f(n,"depth",0.2f);
	float wall=xml_attr_f(n,"tube",xml_attr_f(n,"thickness",0.0f));
	Mesh mesh=gen_arch(w,h,d,wall,xml_attr_i(n,"segments",16));
	apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow,renderable,unlit);
}

static void parse_group(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)parentM; (void)pos; (void)rot; (void)color; (void)shin; (void)castsShadow; (void)renderable; (void)unlit;
	parse_nodes(s, n, M, R);
}

/* build the boxes that make up a wall with rectangular openings */
typedef struct { float x,width,height,sill; } Opening;
static void build_wall_boxes(Scene *s, mat4 wallM, mat4 wallR, float L,float H,float T,
                              Opening *openings,int nopen, vec3 color,float shin, int castsShadow,int renderable,int unlit);

static int mat4_inverse_affine(mat4 m, mat4 *out){
	vec3 a=v3(m.m[0],m.m[1],m.m[2]), b=v3(m.m[4],m.m[5],m.m[6]);
	vec3 c=v3(m.m[8],m.m[9],m.m[10]), t=v3(m.m[12],m.m[13],m.m[14]);
	float det=vdot(a,vcross(b,c));
	if(fabsf(det)<1e-8f) return 0;
	vec3 r0=vscale(vcross(b,c),1.0f/det);
	vec3 r1=vscale(vcross(c,a),1.0f/det);
	vec3 r2=vscale(vcross(a,b),1.0f/det);
	*out=mat4_identity();
	out->m[0]=r0.x; out->m[4]=r0.y; out->m[8]=r0.z;
	out->m[1]=r1.x; out->m[5]=r1.y; out->m[9]=r1.z;
	out->m[2]=r2.x; out->m[6]=r2.y; out->m[10]=r2.z;
	out->m[12]=-vdot(r0,t); out->m[13]=-vdot(r1,t); out->m[14]=-vdot(r2,t);
	return 1;
}

static void add_negative_openings(Scene *s, mat4 wallM, float L,float H,float T,
		Opening **op,int *nop,int *cop){
	mat4 inv;
	if(!mat4_inverse_affine(wallM,&inv)) return;
	for(int i=0;i<s->nnegativeBoxes;i++){
		NegativeBox *b=&s->negativeBoxes[i];
		mat4 local=mat4_mul(inv,b->transform);
		vec3 ax=vnorm(mat4_xform_dir(local,v3(1,0,0)));
		vec3 ay=vnorm(mat4_xform_dir(local,v3(0,1,0)));
		vec3 az=vnorm(mat4_xform_dir(local,v3(0,0,1)));
		if(fabsf(ax.x)<0.999f || fabsf(ay.y)<0.999f || fabsf(az.z)<0.999f) continue;
		vec3 half=vscale(b->size,0.5f);
		vec3 lo=v3(INFINITY,INFINITY,INFINITY), hi=v3(-INFINITY,-INFINITY,-INFINITY);
		for(int x=-1;x<=1;x+=2) for(int y=-1;y<=1;y+=2) for(int z=-1;z<=1;z+=2){
			vec3 p=mat4_xform_point(local,v3(half.x*x,half.y*y,half.z*z));
			if(p.x<lo.x) lo.x=p.x;
			if(p.x>hi.x) hi.x=p.x;
			if(p.y<lo.y) lo.y=p.y;
			if(p.y>hi.y) hi.y=p.y;
			if(p.z<lo.z) lo.z=p.z;
			if(p.z>hi.z) hi.z=p.z;
		}
		if(lo.z>-T*0.5f+0.001f || hi.z<T*0.5f-0.001f) continue;
		if(hi.x<=-L*0.5f || lo.x>=L*0.5f || hi.y<=0 || lo.y>=H) continue;
		if(lo.x<-L*0.5f) lo.x=-L*0.5f;
		if(hi.x>L*0.5f) hi.x=L*0.5f;
		if(lo.y<0) lo.y=0;
		if(hi.y>H) hi.y=H;
		Opening o={lo.x+L*0.5f,hi.x-lo.x,hi.y-lo.y,lo.y};
		if(o.width>0.001f && o.height>0.001f) DA_PUSH(*op,*nop,*cop,o);
	}
}

static void parse_wall(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)M;
	float L=xml_attr_f(n,"length",4.0f), H=xml_attr_f(n,"height",2.7f), T=xml_attr_f(n,"thickness",0.2f);
	mat4 wallM = mat4_mul(parentM, mat4_mul(mat4_translate(pos), mat4_rot_xyz(rot)));
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
	add_negative_openings(s,wallM,L,H,T,&op,&nop,&cop);
	build_wall_boxes(s, wallM, R, L,H,T, op,nop, color, shin, castsShadow, renderable, unlit);
	free(op);
}

static void parse_line(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)R; (void)parentM; (void)pos; (void)rot; (void)color; (void)shin; (void)castsShadow; (void)renderable; (void)unlit;
	vec3 lcolor=xml_attr_v3(n,"color",v3(0.85f,0.15f,0.15f));
	scene_add_overlay_line(s,
		mat4_xform_point(M,xml_attr_v3(n,"start",v3(0,0,0))),
		mat4_xform_point(M,xml_attr_v3(n,"end",v3(0,1,0))),
		lcolor, 0);
}

static void parse_dummy(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)R; (void)parentM; (void)pos; (void)rot; (void)color; (void)shin; (void)castsShadow; (void)renderable; (void)unlit;
	vec3 worldPos=mat4_xform_point(M,v3(0,0,0));
	vec3 lcolor=xml_attr_v3(n,"color",v3(0.85f,0.15f,0.15f));
	const char *type=xml_attr(n,"type","character");
	if(!strcmp(type,"character")){
		const char *ref=xml_attr(n,"ref",NULL);
		if(ref){
			CharDef *cd=NULL;
			for(int i=0;i<s->ncharDefs;i++) if(!strcmp(s->charDefs[i].name,ref)){ cd=&s->charDefs[i]; break; }
			if(cd) add_character_dummy(s,worldPos,cd,lcolor);
		}
	} else if(!strcmp(type,"lamp")){
		add_lamp_dummy(s,worldPos,xml_attr_f(n,"radius",0.15f),lcolor,1);
	} else if(!strcmp(type,"camera")){
		add_camera_dummy(s,worldPos,xml_attr_v3(n,"look",v3(0,0,-1)),xml_attr_f(n,"fov",60.0f),1.0f,lcolor,2);
	}
}

static XmlNode* load_prefab(Scene *s, const char *name){
	for(int i=0;i<s->nprefabs;i++)
		if(!strcmp(s->prefabs[i].ref,name)) return (XmlNode*)s->prefabs[i].root;
	char path[256];
	snprintf(path,sizeof(path),"prefabs/%s.blk",name);
	char *buf=read_file(path);
	if(!buf) return NULL;
	XmlNode *root=xml_parse(buf);
	free(buf);
	if(!root) return NULL;
	warn_unknown_elements(root,path,1);
	PrefabDef pd; memset(&pd,0,sizeof(pd)); strncpy(pd.ref,name,31); pd.root=root;
	for(int i=0;i<root->nkids;i++){
		if(!strcmp(root->kids[i]->tag,"attach")){
			AttachPoint ap;
			strncpy(ap.name,xml_attr(root->kids[i],"name",""),31);
			ap.pos=xml_attr_v3(root->kids[i],"pos",v3(0,0,0));
			DA_PUSH(pd.attaches,pd.nattaches,pd.cattaches,ap);
		}
	}
	DA_PUSH(s->prefabs,s->nprefabs,s->cprefabs,pd);
	return root;
}

static void collect_negative_boxes(Scene *s, XmlNode *parent, mat4 parentM){
	for(int i=0;i<parent->nkids;i++){
		XmlNode *n=parent->kids[i];
		vec3 pos=xml_attr_v3(n,"pos",v3(0,0,0));
		vec3 rot=xml_attr_v3(n,"rot",v3(0,0,0));
		vec3 scl=xml_attr_v3(n,"scale",v3(1,1,1));
		vec3 pvt=xml_attr_v3(n,"pivotOffset",v3(0,0,0));
		mat4 Tp=mat4_translate(pvt), Tn=mat4_translate(v3(-pvt.x,-pvt.y,-pvt.z));
		mat4 local=mat4_mul(mat4_translate(pos),
			mat4_mul(Tp,mat4_mul(mat4_rot_xyz(rot),mat4_mul(Tn,mat4_scale(scl)))));
		mat4 M=mat4_mul(parentM,local);
		if(!strcmp(n->tag,"bool-negative-box")){
			NegativeBox b={M,xml_attr_v3(n,"size",v3(1,1,1))};
			DA_PUSH(s->negativeBoxes,s->nnegativeBoxes,s->cnegativeBoxes,b);
		} else if(!strcmp(n->tag,"group")){
			collect_negative_boxes(s,n,M);
		} else if(!strcmp(n->tag,"prefab")){
			const char *source=xml_attr(n,"source",NULL);
			XmlNode *proot=source?load_prefab(s,source):NULL;
			if(proot) collect_negative_boxes(s,proot,M);
		}
	}
}

static void parse_prefab(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)parentM; (void)pos; (void)rot; (void)shin; (void)castsShadow; (void)renderable; (void)unlit;
	const char *source=xml_attr(n,"source",NULL);
	if(!source) return;
	XmlNode *proot=load_prefab(s,source);
	if(!proot){ fprintf(stderr,"prefab not found: %s\n",source); return; }
	const char *name=xml_attr(n,"name",NULL);
	if(name){
		InstanceDef inst; memset(&inst,0,sizeof(inst));
		strncpy(inst.name,name,31); strncpy(inst.ref,source,31);
		inst.transform=M; inst.rotMatrix=R;
		DA_PUSH(s->instances,s->ninstances,s->cinstances,inst);
	}
	int oldTintActive=s->prefabTintActive;
	vec3 oldTint=s->prefabTint;
	if(xml_attr(n,"color",NULL)){
		s->prefabTintActive=1;
		s->prefabTint=color;
	}

	int arrayCount=1;
	vec3 arrayTrans=v3(0,0,0), arrayRot=v3(0,0,0);
	for(int k=0;k<n->nkids;k++){
		if(!strcmp(n->kids[k]->tag,"array")){
			XmlNode *arr=n->kids[k];
			arrayCount=xml_attr_i(arr,"count",1);
			arrayTrans=xml_attr_v3(arr,"translation",v3(0,0,0));
			arrayRot=xml_attr_v3(arr,"rotation",v3(0,0,0));
			break;
		}
	}
	for(int step=0;step<arrayCount;step++){
		vec3 off=vscale(arrayTrans,(float)step);
		vec3 r=vscale(arrayRot,(float)step);
		mat4 stepM=mat4_mul(M,mat4_mul(mat4_translate(off),mat4_rot_xyz(r)));
		parse_nodes(s, proot, stepM, R);
	}

	s->prefabTintActive=oldTintActive;
	s->prefabTint=oldTint;
}

static void parse_light(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow, int renderable, int unlit){
	(void)R; (void)parentM; (void)pos; (void)rot; (void)color; (void)shin; (void)castsShadow; (void)renderable; (void)unlit;
	Light L={0};
	L.pos=mat4_xform_point(M,v3(0,0,0));
	L.color=xml_attr_v3(n,"color",v3(1,1,1));
	L.intensity=xml_attr_f(n,"intensity",1.0f);
	L.radius=xml_attr_f(n,"radius",0.0f);
	L.castsShadow=xml_attr_i(n,"castShadows",1);
	DA_PUSH(s->lights,s->nlights,s->clights,L);
}

static const struct {
	const char *tag;
	shape_parser_fn parse;
} shape_parsers[] = {
	{ "box",      parse_box },
	{ "sphere",   parse_sphere },
	{ "cylinder", parse_cylinder },
	{ "prism",    parse_prism },
	{ "cone",     parse_cone },
	{ "pyramid",  parse_cone },
	{ "torus",    parse_torus },
	{ "arch",     parse_arch },
	{ "group",    parse_group },
	{ "light",    parse_light },
	{ "prefab",   parse_prefab },
	{ "wall",     parse_wall },
	{ "line",     parse_line },
	{ "dummy",    parse_dummy },
};

static void parse_nodes(Scene *s, XmlNode *parent, mat4 parentM, mat4 parentR){
	for(int i=0;i<parent->nkids;i++){
		XmlNode *n=parent->kids[i];
		int oldIgnore=s->sanityIgnoreActive, oldFloor=s->sanityFloorActive, oldCheck=s->sanityCheckActive;
		s->sanityIgnoreActive |= xml_attr_i(n,"sanityIgnore",0);
		s->sanityFloorActive |= xml_attr_i(n,"sanityFloor",0);
		s->sanityCheckActive |= xml_attr_i(n,"sanityCheck",0);
		char *tag=n->tag;
		vec3 pos=xml_attr_v3(n,"pos",v3(0,0,0));
		vec3 rot=xml_attr_v3(n,"rot",v3(0,0,0));
		vec3 scl=xml_attr_v3(n,"scale",v3(1,1,1));
		const char *attach=xml_attr(n,"attach",NULL);
		mat4 attachM=mat4_identity(), attachRmat=mat4_identity();
		if(attach){
			const char *colon=strchr(attach,':');
			if(colon){
				int len=(int)(colon-attach);
				char instName[32]; memcpy(instName,attach,(size_t)(len<31?len:31)); instName[len]=0;
				const char *slot=colon+1;
				for(int k=0;k<s->ninstances;k++){
					if(strcmp(s->instances[k].name,instName)) continue;
					for(int m=0;m<s->nprefabs;m++){
						if(strcmp(s->prefabs[m].ref,s->instances[k].ref)) continue;
						for(int p=0;p<s->prefabs[m].nattaches;p++){
							if(strcmp(s->prefabs[m].attaches[p].name,slot)) continue;
							attachM=mat4_mul(s->instances[k].transform,
								mat4_translate(s->prefabs[m].attaches[p].pos));
							attachRmat=s->instances[k].rotMatrix;
							break;
						}
						break;
					}
					break;
				}
			}
		}
		mat4 R = mat4_mul(parentR, mat4_mul(attachRmat, mat4_rot_xyz(rot)));
		vec3 pvt=xml_attr_v3(n,"pivotOffset",v3(0,0,0));
		mat4 M;
		if(pvt.x!=0.0f||pvt.y!=0.0f||pvt.z!=0.0f){
			mat4 Tp=mat4_translate(pvt);
			mat4 Tn=mat4_translate(v3(-pvt.x,-pvt.y,-pvt.z));
			M=mat4_mul(parentM, mat4_mul(attachM, mat4_mul(mat4_translate(pos),
				mat4_mul(Tp, mat4_mul(mat4_rot_xyz(rot), mat4_mul(Tn, mat4_scale(scl)))))));
		} else {
			M=mat4_mul(parentM, mat4_mul(attachM, mat4_mul(mat4_translate(pos),
				mat4_mul(mat4_rot_xyz(rot), mat4_scale(scl)))));
		}
		Material *mat = find_material(s, xml_attr(n,"material",NULL));
		vec3 color = mat? mat->color : xml_attr_v3(n,"color",v3(0.8f,0.8f,0.8f));
		if(s->prefabTintActive && xml_attr_i(n,"tint",0)) color=s->prefabTint;
		float shin = mat? mat->shininess : xml_attr_f(n,"shininess",8.0f);
		int castsShadow = xml_attr_i(n,"castShadow",1);
		int renderable = xml_attr_i(n,"renderable",1);
		int unlit = xml_attr_i(n,"unlit",0);

		for(int j=0;j<(int)(sizeof(shape_parsers)/sizeof(shape_parsers[0]));j++){
			if(!strcmp(tag, shape_parsers[j].tag)){
				shape_parsers[j].parse(s, n, M, R, parentM, pos, rot, color, shin, castsShadow, renderable, unlit);
				break;
			}
		}
		s->sanityIgnoreActive=oldIgnore;
		s->sanityFloorActive=oldFloor;
		s->sanityCheckActive=oldCheck;
	}
}

/* ---------------------------------------------- Top-level scene tags ------ */

typedef void (*scene_tag_parser_fn)(Scene *s, XmlNode *n);

static void parse_camera_tag(Scene *s, XmlNode *n){
	Camera cam={0}; strncpy(cam.name, xml_attr(n,"name","Camera1"), 31);
	strncpy(cam.comment, xml_attr(n,"comment",""), 63);
	cam.pos = xml_attr_v3(n,"pos", s->ncameras>0 ? s->camPos : v3(0,1.6f,5));
	cam.look = xml_attr_v3(n,"look", s->ncameras>0 ? s->camLook : v3(0,1.2f,0));
	cam.fov = xml_attr_f(n,"fov",60.0f);
	DA_PUSH(s->cameras,s->ncameras,s->ccameras,cam);
	if(s->ncameras==1){
		s->camPos=cam.pos; s->camLook=cam.look; s->camFov=cam.fov;
	}
}

static const struct { const char *id; vec3 color; } preset_bgs[] = {
	{ "midnight", {0.02f,0.03f,0.07f} },
	{ "twilight", {0.06f,0.05f,0.10f} },
	{ "dusk",     {0.08f,0.10f,0.14f} },
	{ "dawn",     {0.16f,0.10f,0.14f} },
	{ "overcast", {0.25f,0.27f,0.30f} },
	{ "noon",     {0.40f,0.48f,0.64f} },
	{ "neutral",  {0.18f,0.20f,0.24f} },
	{ "black",    {0.00f,0.00f,0.00f} },
};
static const int npreset_bgs = (int)(sizeof(preset_bgs)/sizeof(preset_bgs[0]));

static void parse_material_tag(Scene *s, XmlNode *n){
	Material m={0}; strncpy(m.id, xml_attr(n,"id","mat"), 31);
	m.color = xml_attr_v3(n,"color",v3(0.8f,0.8f,0.8f));
	m.shininess = xml_attr_f(n,"shininess",8.0f);
	DA_PUSH(s->mats,s->nmats,s->cmats,m);
}

static void parse_sun_tag(Scene *s, XmlNode *n){
	Light L={0};
	L.dir = vnorm(xml_attr_v3(n,"dir",v3(1,-1,0)));
	L.color = xml_attr_v3(n,"color",v3(1,1,1));
	L.intensity = xml_attr_f(n,"intensity",1.0f);
	L.radius = 0.0f;
	L.castsShadow = xml_attr_i(n,"castShadows",1);
	L.isDirectional = 1;
	DA_PUSH(s->lights,s->nlights,s->clights,L);
}

static void parse_chardef_tag(Scene *s, XmlNode *n){
	CharDef cd={0};
	strncpy(cd.name,xml_attr(n,"name",""),31);
	cd.height=xml_attr_f(n,"height",1.0f);
	cd.radius=xml_attr_f(n,"radius",0.10f);
	cd.top=xml_attr_f(n,"top",1.0f);
	cd.neck=xml_attr_f(n,"neck",0.75f);
	cd.pelvis=xml_attr_f(n,"pelvis",0.25f);
	cd.feet=xml_attr_f(n,"feet",0.0f);
	DA_PUSH(s->charDefs,s->ncharDefs,s->ccharDefs,cd);
}

vec3 light_to_source(Light *light, vec3 point){
	return light->isDirectional ? vscale(light->dir,-1.0f) : vsub(light->pos,point);
}

static const struct {
	const char *tag;
	scene_tag_parser_fn parse;
} scene_tags[] = {
	{ "camera",     parse_camera_tag },
	{ "material",   parse_material_tag },
	{ "sun",        parse_sun_tag },
	{ "chardef",    parse_chardef_tag },
};

static int has_shape_parser(const char *tag){
	for(int i=0;i<(int)(sizeof(shape_parsers)/sizeof(shape_parsers[0]));i++)
		if(!strcmp(tag,shape_parsers[i].tag)) return 1;
	return 0;
}

static int has_scene_parser(const char *tag){
	for(int i=0;i<(int)(sizeof(scene_tags)/sizeof(scene_tags[0]));i++)
		if(!strcmp(tag,scene_tags[i].tag)) return 1;
	return 0;
}

static int has_modifier_parser(const char *tag){
	for(int i=0;i<(int)(sizeof(modifier_parsers)/sizeof(modifier_parsers[0]));i++)
		if(!strcmp(tag,modifier_parsers[i].tag)) return 1;
	return 0;
}

static void warn_unsupported_tree(XmlNode *n, const char *path, const char *parent){
	fprintf(stderr,"warning: %s: unsupported XML element <%s> in <%s>\n",path,n->tag,parent);
	for(int i=0;i<n->nkids;i++) warn_unsupported_tree(n->kids[i],path,n->tag);
}

static void warn_unknown_children(XmlNode *parent, const char *path, int root, int prefab){
	for(int i=0;i<parent->nkids;i++){
		XmlNode *n=parent->kids[i];
		int supported=0;
		if(root) supported=has_shape_parser(n->tag) || !strcmp(n->tag,"bool-negative-box") ||
			(prefab ? !strcmp(n->tag,"attach") : has_scene_parser(n->tag));
		else if(!strcmp(parent->tag,"group"))
			supported=has_shape_parser(n->tag) || !strcmp(n->tag,"bool-negative-box");
		else if(!strcmp(parent->tag,"wall")) supported=!strcmp(n->tag,"opening");
		else if(!strcmp(parent->tag,"prefab")) supported=!strcmp(n->tag,"array");
		else if(has_shape_parser(parent->tag)) supported=has_modifier_parser(n->tag);
		if(!supported){
			warn_unsupported_tree(n,path,parent->tag);
			continue;
		}
		warn_unknown_children(n,path,0,prefab);
	}
}

static void warn_unknown_elements(XmlNode *root, const char *path, int prefab){
	const char *expected=prefab?"prefab":"scene";
	if(strcmp(root->tag,expected)){
		warn_unsupported_tree(root,path,"document");
		return;
	}
	warn_unknown_children(root,path,1,prefab);
}

/* --------------------------------------------------------------- IO & load */

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
	s->ambient=v3(0.12f,0.12f,0.14f); s->bg=v3(0.08f,0.10f,0.14f);

	char *buf=read_file(path); if(!buf) return 0;
	XmlNode *root=xml_parse(buf); free(buf);
	if(!root){ fprintf(stderr,"failed to parse %s\n",path); return 0; }
	warn_unknown_elements(root,path,0);

	s->ambient = xml_attr_v3(root,"ambient",s->ambient);
	{
		const char *bg=xml_attr(root,"background",NULL);
		if(bg){
			if(strchr(bg,' ')){
				vec3 c=s->bg; sscanf(bg,"%f %f %f",&c.x,&c.y,&c.z); s->bg=c;
			} else {
				for(int i=0;i<npreset_bgs;i++){
					if(!strcmp(preset_bgs[i].id,bg)){ s->bg=preset_bgs[i].color; break; }
				}
			}
		}
	}

	mat4 I=mat4_identity();
	collect_negative_boxes(s,root,I);

	for(int i=0;i<root->nkids;i++){
		XmlNode *n=root->kids[i];
		for(int j=0;j<(int)(sizeof(scene_tags)/sizeof(scene_tags[0]));j++){
			if(!strcmp(n->tag, scene_tags[j].tag)){
				scene_tags[j].parse(s, n);
				break;
			}
		}
	}
	parse_nodes(s, root, I, I);
	xml_free(root);

	if(s->ncameras==0){
		Camera def={0}; strncpy(def.name,"Camera1",31);
		def.pos=s->camPos; def.look=s->camLook; def.fov=s->camFov;
		DA_PUSH(s->cameras,s->ncameras,s->ccameras,def);
	}

	s->svols = calloc((size_t)s->nlights, sizeof(ShadowVolume));
	int nlamp=0;
	for(int li=0;li<s->nlights;li++){
		if(!s->lights[li].isDirectional){
			add_lamp_dummy(s,s->lights[li].pos,0.15f,v3(1.0f,0.7f,0.2f),1);
			nlamp++;
		}
	}
	for(int ci=0;ci<s->ncameras;ci++){
		Camera *c=&s->cameras[ci];
		add_camera_dummy(s,c->pos,c->look,c->fov,1.0f,v3(0.2f,0.8f,0.2f),2);
	}
	fprintf(stderr,"overlay: %d lines (%d lamps, %d cameras)\n",s->noverlayLines,nlamp,s->ncameras);
	return 1;
}

void scene_select_camera(Scene *s, const char *name){
	for(int i=0;i<s->ncameras;i++){
		if(!strcmp(s->cameras[i].name,name)){
			s->camPos=s->cameras[i].pos;
			s->camLook=s->cameras[i].look;
			s->camFov=s->cameras[i].fov;
			return;
		}
	}
}

typedef struct { vec3 min,max; } Bounds;

static Bounds scene_obj_bounds(SceneObj *o){
	Bounds b={v3(INFINITY,INFINITY,INFINITY),v3(-INFINITY,-INFINITY,-INFINITY)};
	for(int i=0;i<o->mesh.nverts;i++){
		vec3 p=o->mesh.verts[i].pos;
		if(p.x<b.min.x) b.min.x=p.x; if(p.x>b.max.x) b.max.x=p.x;
		if(p.y<b.min.y) b.min.y=p.y; if(p.y>b.max.y) b.max.y=p.y;
		if(p.z<b.min.z) b.min.z=p.z; if(p.z>b.max.z) b.max.z=p.z;
	}
	return b;
}

static float bounds_overlap(float amin,float amax,float bmin,float bmax){
	float lo=amin>bmin?amin:bmin, hi=amax<bmax?amax:bmax;
	return hi-lo;
}

int scene_sanity_check(Scene *s){
	Bounds *bounds=calloc((size_t)s->nobjs,sizeof(*bounds));
	int errors=0;
	for(int i=0;i<s->nobjs;i++) bounds[i]=scene_obj_bounds(&s->objs[i]);
	for(int i=0;i<s->nobjs;i++){
		SceneObj *a=&s->objs[i];
		if(a->sanityIgnore || a->sanityFloor || !a->sanityCheck) continue;
		for(int j=i+1;j<s->nobjs;j++){
			SceneObj *b=&s->objs[j];
			if(b->sanityIgnore || b->sanityFloor || !b->sanityCheck) continue;
			float x=bounds_overlap(bounds[i].min.x,bounds[i].max.x,bounds[j].min.x,bounds[j].max.x);
			float y=bounds_overlap(bounds[i].min.y,bounds[i].max.y,bounds[j].min.y,bounds[j].max.y);
			float z=bounds_overlap(bounds[i].min.z,bounds[i].max.z,bounds[j].min.z,bounds[j].max.z);
			if(x>0.025f && y>0.025f && z>0.025f){
				fprintf(stderr,"sanity: intersecting objects %d and %d (%.3f %.3f %.3f)\n",i,j,x,y,z);
				errors++;
			}
		}
		if(bounds[i].min.y>0.015f){
			int supported=0;
			for(int j=0;j<s->nobjs;j++){
				SceneObj *b=&s->objs[j];
				if(i==j || b->sanityIgnore || (!b->sanityCheck && !b->sanityFloor)) continue;
				if(fabsf(bounds[i].min.y-bounds[j].max.y)>0.025f) continue;
				if(bounds_overlap(bounds[i].min.x,bounds[i].max.x,bounds[j].min.x,bounds[j].max.x)>0.025f &&
				   bounds_overlap(bounds[i].min.z,bounds[i].max.z,bounds[j].min.z,bounds[j].max.z)>0.025f){ supported=1; break; }
			}
			if(!supported){ fprintf(stderr,"sanity: floating object %d, base y=%.3f\n",i,bounds[i].min.y); errors++; }
		}
	}
	free(bounds);
	if(!errors) fprintf(stderr,"sanity: ok (%d objects)\n",s->nobjs);
	return !errors;
}

/* -------------------------------------- build_wall_boxes (below parse_nodes) */
static void build_wall_boxes(Scene *s, mat4 wallM, mat4 wallR, float L,float H,float T,
                              Opening *openings,int nopen, vec3 color,float shin, int castsShadow,int renderable,int unlit){
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
			scene_add_obj(s, box, M, wallR, color, shin, castsShadow, renderable, unlit);
		}
	}
	free(bp);
}
