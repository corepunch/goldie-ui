#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "simplegl.h"

void build_shadow_volume(Mesh *m, vec3 lightPos, vec3 lightDir, int isDir, ShadowVolume *sv){
	memset(sv,0,sizeof(*sv));
	int nt=m->ntris;
	char *facing=malloc((size_t)nt);
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
	free(facing);
}

void scene_build_all_shadow_volumes(Scene *s){
	for(int li=0; li<s->nlights; li++){
		ShadowVolume combined={0};
		if(s->lights[li].castsShadow){
			for(int oi=0; oi<s->nobjs; oi++){
				SceneObj *o=&s->objs[oi];
				if(!o->castsShadow) continue;
				ShadowVolume part;
				build_shadow_volume(&o->mesh, s->lights[li].pos, s->lights[li].dir, s->lights[li].isDirectional, &part);
				for(int k=0;k<part.nverts;k++) DA_PUSH(combined.verts,combined.nverts,combined.cverts,part.verts[k]);
				free(part.verts);
			}
		}
		fprintf(stderr,"  light %d shadow volume: %d verts = %d tris\n", li, combined.nverts, combined.nverts/3);
		s->svols[li]=combined;
	}
}
