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
	for(int i=0;i<s->nprefabs;i++) xml_free((XmlNode*)s->prefabs[i].root);
	for(int i=0;i<s->nobjs;i++) mesh_free(&s->objs[i].mesh);
	for(int i=0;i<s->nlights;i++) free(s->svols[i].verts);
	free(s->lights); free(s->mats); free(s->objs); free(s->svols); free(s->cameras); free(s->prefabs);
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

/* ---------------------------------------------------- Modifiers ---------- */

static char* read_file(const char*path);

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

static const struct {
	const char *tag;
	modifier_parser_fn parse;
} modifier_parsers[] = {
	{ "taper",   parse_mod_taper },
	{ "twist",   parse_mod_twist },
	{ "bend",    parse_mod_bend },
	{ "stretch", parse_mod_stretch },
	{ "skew",    parse_mod_skew },
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

/* ---------------------------------------------------- Shape parsers ------- */

static void parse_nodes(Scene *s, XmlNode *parent, mat4 parentM, mat4 parentR);

typedef void (*shape_parser_fn)(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow);

static void parse_box(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow){
	(void)parentM; (void)pos; (void)rot;
	vec3 sz=xml_attr_v3(n,"size",v3(1,1,1));
	Mesh mesh=gen_box(sz.x,sz.y,sz.z); apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow);
}

static void parse_sphere(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow){
	(void)parentM; (void)pos; (void)rot;
	float r=xml_attr_f(n,"radius",0.5f);
	Mesh mesh=gen_sphere(r,xml_attr_i(n,"rings",16),xml_attr_i(n,"slices",24)); apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow);
}

static void parse_cylinder(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow){
	(void)parentM; (void)pos; (void)rot;
	float r=xml_attr_f(n,"radius",0.5f), h=xml_attr_f(n,"height",1.0f);
	Mesh mesh=gen_cylinder(r,h,xml_attr_i(n,"sides",24)); apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow);
}

static void parse_prism(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow){
	(void)parentM; (void)pos; (void)rot;
	float r=xml_attr_f(n,"radius",0.5f), h=xml_attr_f(n,"height",1.0f);
	Mesh mesh=gen_prism(r,h,xml_attr_i(n,"sides",6)); apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow);
}

static void parse_cone(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow){
	(void)parentM; (void)pos; (void)rot;
	float rb=xml_attr_f(n,"radius",0.5f), rt=xml_attr_f(n,"radiusTop",0.0f), h=xml_attr_f(n,"height",1.0f);
	int sides = xml_attr_i(n,"sides", !strcmp(n->tag,"pyramid")?4:24);
	Mesh mesh=gen_cone(rb,rt,h,sides); apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow);
}

static void parse_torus(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow){
	(void)parentM; (void)pos; (void)rot;
	float R_=xml_attr_f(n,"majorRadius",0.5f), r_=xml_attr_f(n,"minorRadius",0.15f);
	Mesh mesh=gen_torus(R_,r_,xml_attr_i(n,"majorSegments",24),xml_attr_i(n,"minorSegments",12)); apply_modifiers(&mesh,n);
	scene_add_obj(s, mesh, M,R, color,shin,castsShadow);
}

static void parse_group(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow){
	(void)parentM; (void)pos; (void)rot; (void)color; (void)shin; (void)castsShadow;
	parse_nodes(s, n, M, R);
}

/* build the boxes that make up a wall with rectangular openings */
typedef struct { float x,width,height,sill; } Opening;
static void build_wall_boxes(Scene *s, mat4 wallM, mat4 wallR, float L,float H,float T,
                              Opening *openings,int nopen, vec3 color,float shin);

static void parse_wall(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow){
	(void)M; (void)castsShadow;
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
	build_wall_boxes(s, wallM, R, L,H,T, op,nop, color, shin);
	free(op);
}

static XmlNode* load_prefab(Scene *s, const char *name){
	for(int i=0;i<s->nprefabs;i++)
		if(!strcmp(s->prefabs[i].ref,name)) return (XmlNode*)s->prefabs[i].root;
	char path[256];
	snprintf(path,sizeof(path),"prefabs/%s.xml",name);
	char *buf=read_file(path);
	if(!buf) return NULL;
	XmlNode *root=xml_parse(buf);
	free(buf);
	if(!root) return NULL;
	PrefabDef pd; strncpy(pd.ref,name,31); pd.root=root;
	DA_PUSH(s->prefabs,s->nprefabs,s->cprefabs,pd);
	return root;
}

static void parse_prefab(Scene *s, XmlNode *n, mat4 M, mat4 R, mat4 parentM, vec3 pos, vec3 rot, vec3 color, float shin, int castsShadow){
	(void)M; (void)color; (void)shin; (void)castsShadow;
	const char *ref=xml_attr(n,"ref",NULL);
	if(!ref) return;
	XmlNode *proot=load_prefab(s,ref);
	if(!proot){ fprintf(stderr,"prefab not found: %s\n",ref); return; }
	mat4 prefabM = mat4_mul(parentM, mat4_mul(mat4_translate(pos), mat4_rot_xyz(rot)));
	parse_nodes(s, proot, prefabM, R);
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
	{ "group",    parse_group },
	{ "prefab",   parse_prefab },
	{ "wall",     parse_wall },
};

static void parse_nodes(Scene *s, XmlNode *parent, mat4 parentM, mat4 parentR){
	for(int i=0;i<parent->nkids;i++){
		XmlNode *n=parent->kids[i];
		char *tag=n->tag;
		vec3 pos=xml_attr_v3(n,"pos",v3(0,0,0));
		vec3 rot=xml_attr_v3(n,"rot",v3(0,0,0));
		vec3 scl=xml_attr_v3(n,"scale",v3(1,1,1));
		mat4 R = mat4_mul(parentR, mat4_rot_xyz(rot));
		mat4 M = mat4_mul(parentM, mat4_mul(mat4_translate(pos), mat4_mul(mat4_rot_xyz(rot), mat4_scale(scl))));
		Material *mat = find_material(s, xml_attr(n,"material",NULL));
		vec3 color = mat? mat->color : xml_attr_v3(n,"color",v3(0.8f,0.8f,0.8f));
		float shin = mat? mat->shininess : xml_attr_f(n,"shininess",8.0f);
		int castsShadow = xml_attr_i(n,"castShadow",1);

		for(int j=0;j<(int)(sizeof(shape_parsers)/sizeof(shape_parsers[0]));j++){
			if(!strcmp(tag, shape_parsers[j].tag)){
				shape_parsers[j].parse(s, n, M, R, parentM, pos, rot, color, shin, castsShadow);
				break;
			}
		}
	}
}

/* ---------------------------------------------- Top-level scene tags ------ */

typedef void (*scene_tag_parser_fn)(Scene *s, XmlNode *n);

static void parse_camera_tag(Scene *s, XmlNode *n){
	Camera cam={0}; strncpy(cam.name, xml_attr(n,"name","Camera1"), 31);
	cam.pos = xml_attr_v3(n,"pos", s->ncameras>0 ? s->camPos : v3(0,1.6f,5));
	cam.look = xml_attr_v3(n,"look", s->ncameras>0 ? s->camLook : v3(0,1.2f,0));
	cam.fov = xml_attr_f(n,"fov",60.0f);
	DA_PUSH(s->cameras,s->ncameras,s->ccameras,cam);
	if(s->ncameras==1){
		s->camPos=cam.pos; s->camLook=cam.look; s->camFov=cam.fov;
	}
}

static void parse_ambient_tag(Scene *s, XmlNode *n){
	s->ambient = xml_attr_v3(n,"color",s->ambient);
}

static void parse_background_tag(Scene *s, XmlNode *n){
	s->bg = xml_attr_v3(n,"color",s->bg);
}

static void parse_material_tag(Scene *s, XmlNode *n){
	Material m={0}; strncpy(m.id, xml_attr(n,"id","mat"), 31);
	m.color = xml_attr_v3(n,"color",v3(0.8f,0.8f,0.8f));
	m.shininess = xml_attr_f(n,"shininess",8.0f);
	DA_PUSH(s->mats,s->nmats,s->cmats,m);
}

static void parse_light_tag(Scene *s, XmlNode *n){
	Light L={0};
	L.pos = xml_attr_v3(n,"pos",v3(0,3,0));
	L.color = xml_attr_v3(n,"color",v3(1,1,1));
	L.intensity = xml_attr_f(n,"intensity",1.0f);
	L.castsShadow = xml_attr_i(n,"castShadows",1);
	DA_PUSH(s->lights,s->nlights,s->clights,L);
}

static const struct {
	const char *tag;
	scene_tag_parser_fn parse;
} scene_tags[] = {
	{ "camera",     parse_camera_tag },
	{ "ambient",    parse_ambient_tag },
	{ "background", parse_background_tag },
	{ "material",   parse_material_tag },
	{ "light",      parse_light_tag },
};

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
	s->ambient=v3(0.12f,0.12f,0.14f); s->bg=v3(0.05f,0.06f,0.08f);

	char *buf=read_file(path); if(!buf) return 0;
	XmlNode *root=xml_parse(buf); free(buf);
	if(!root){ fprintf(stderr,"failed to parse %s\n",path); return 0; }

	for(int i=0;i<root->nkids;i++){
		XmlNode *n=root->kids[i];
		for(int j=0;j<(int)(sizeof(scene_tags)/sizeof(scene_tags[0]));j++){
			if(!strcmp(n->tag, scene_tags[j].tag)){
				scene_tags[j].parse(s, n);
				break;
			}
		}
	}
	mat4 I=mat4_identity();
	parse_nodes(s, root, I, I);
	xml_free(root);

	if(s->ncameras==0){
		Camera def={0}; strncpy(def.name,"Camera1",31);
		def.pos=s->camPos; def.look=s->camLook; def.fov=s->camFov;
		DA_PUSH(s->cameras,s->ncameras,s->ccameras,def);
	}

	s->svols = calloc((size_t)s->nlights, sizeof(ShadowVolume));
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

/* -------------------------------------- build_wall_boxes (below parse_nodes) */
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
			scene_add_obj(s, box, M, wallR, color, shin, 0);
		}
	}
	free(bp);
}
