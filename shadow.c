#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include "simplegl.h"

void build_shadow_volume(Mesh *m, vec3 lightPos, vec3 lightDir, int isDir, ShadowVolume *sv){
	memset(sv,0,sizeof(*sv));
	int nt=m->ntris;
	char *facing=alloca((size_t)nt);
	Light light={.pos=lightPos,.dir=lightDir,.isDirectional=isDir};
	for(int i=0;i<nt;i++){
		vec3 p=m->verts[m->tris[i].a].pos;
		vec3 toLight=light_to_source(&light,p);
		facing[i] = vdot(m->triN[i], toLight) > 0.0f;
	}
	#define PUSHP(vv) DA_PUSH(sv->verts,sv->nverts,sv->cverts,((ShadowVertex){(vv).x,(vv).y,(vv).z,1.0f}))
	#define PUSHD(vv) \
		if(isDir) DA_PUSH(sv->verts,sv->nverts,sv->cverts,((ShadowVertex){(vv).x+lightDir.x*1000.0f,(vv).y+lightDir.y*1000.0f,(vv).z+lightDir.z*1000.0f,1.0f})); \
		else DA_PUSH(sv->verts,sv->nverts,sv->cverts,((ShadowVertex){(vv).x-lightPos.x,(vv).y-lightPos.y,(vv).z-lightPos.z,0.0f}))

	for(int i=0;i<m->nedges;i++){
		Edge *e=&m->edges[i];
		int f0=facing[e->t0], f1=(e->t1>=0)?facing[e->t1]:0;
		int silhouette = (e->t1<0) ? f0 : (f0!=f1);
		if(!silhouette) continue;
		vec3 v0,v1;
		if(f0){ v0=e->p0; v1=e->p1; } else { v0=e->p1; v1=e->p0; }
#ifdef USE_ZPASS
		PUSHP(v0); PUSHP(v1); PUSHD(v1);
		PUSHP(v0); PUSHD(v1); PUSHD(v0);
#else
		PUSHP(v1); PUSHP(v0); PUSHD(v0);
		PUSHP(v1); PUSHD(v0); PUSHD(v1);
#endif
	}

#ifndef USE_ZPASS
	for(int i=0;i<nt;i++){
		Tri t=m->tris[i];
		vec3 a=m->verts[t.a].pos, b=m->verts[t.b].pos, c=m->verts[t.c].pos;
		if(facing[i]){ PUSHP(a); PUSHP(b); PUSHP(c); }
		else { PUSHD(a); PUSHD(b); PUSHD(c); }
	}
#endif

	#undef PUSHP
	#undef PUSHD
}

static void scene_combine_shadow_parts(Scene *s,int li){
	int total=0;
	for(int oi=0;oi<s->nobjs;oi++){
		SceneObj *o=&s->objs[oi];
		if(li<o->nshadowParts) total+=o->shadowParts[li].nverts;
	}
	if(total==0){ free(s->svols[li].verts); memset(&s->svols[li],0,sizeof(ShadowVolume)); return; }
	if(s->svols[li].cverts < total){
		free(s->svols[li].verts);
		s->svols[li].verts=malloc((size_t)total*sizeof(ShadowVertex));
		s->svols[li].cverts=total;
	}
	s->svols[li].nverts=0;
	for(int oi=0;oi<s->nobjs;oi++){
		SceneObj *o=&s->objs[oi];
		if(li>=o->nshadowParts) continue;
		ShadowVolume *part=&o->shadowParts[li];
		if(part->nverts>0){
			memcpy(s->svols[li].verts+s->svols[li].nverts,part->verts,(size_t)part->nverts*sizeof(ShadowVertex));
			s->svols[li].nverts+=part->nverts;
		}
	}
}

static void scene_resize_shadow_parts(SceneObj *o,int nlights){
	for(int i=0;i<o->nshadowParts;i++) free(o->shadowParts[i].verts);
	free(o->shadowParts);
	o->shadowParts=calloc((size_t)nlights,sizeof(ShadowVolume));
	o->nshadowParts=nlights;
}

static void scene_build_object_shadow_part(Scene *s,SceneObj *o,int li){
	free(o->shadowParts[li].verts);
	memset(&o->shadowParts[li],0,sizeof(ShadowVolume));
	if(!o->castsShadow || !s->lights[li].castsShadow) return;
	build_shadow_volume(&o->mesh,s->lights[li].pos,s->lights[li].dir,
		s->lights[li].isDirectional,&o->shadowParts[li]);
}

void scene_build_all_shadow_volumes(Scene *s){
	for(int oi=0;oi<s->nobjs;oi++){
		SceneObj *o=&s->objs[oi];
		scene_resize_shadow_parts(o,s->nlights);
		for(int li=0;li<s->nlights;li++) scene_build_object_shadow_part(s,o,li);
	}
	for(int li=0;li<s->nlights;li++){
		scene_combine_shadow_parts(s,li);
		fprintf(stderr,"  light %d shadow volume: %d verts = %d tris\n",li,s->svols[li].nverts,s->svols[li].nverts/3);
	}
}

void scene_rebuild_node_shadow_volumes(Scene *s,void *editNode){
	for(int oi=0;oi<s->nobjs;oi++){
		SceneObj *o=&s->objs[oi];
		if(o->editNode!=editNode) continue;
		if(o->nshadowParts!=s->nlights) scene_resize_shadow_parts(o,s->nlights);
		for(int li=0;li<s->nlights;li++) scene_build_object_shadow_part(s,o,li);
	}
	for(int li=0;li<s->nlights;li++) scene_combine_shadow_parts(s,li);
}
