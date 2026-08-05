#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "shadow.h"
#include "scene.h"
#include "mesh.h"
#include "math.h"

#define SHADOW_EXTRUDE 100.0f

void build_shadow_volume(Mesh *m, vec3 lightPos, ShadowVolume *sv){
    memset(sv,0,sizeof(*sv));
    int nt=m->ntris;
    char *facing=malloc((size_t)nt);
    for(int i=0;i<nt;i++){
        vec3 p=m->verts[m->tris[i].a].pos;
        facing[i] = vdot(m->triN[i], vsub(lightPos,p)) > 0.0f;
    }
    #define PUSHV(vv) DA_PUSH(sv->verts,sv->nverts,sv->cverts,(vv))

    /* Side quads only — z-pass needs no caps */
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

    #undef PUSHV
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
                build_shadow_volume(&o->mesh, s->lights[li].pos, &part);
                for(int k=0;k<part.nverts;k++) DA_PUSH(combined.verts,combined.nverts,combined.cverts,part.verts[k]);
                free(part.verts);
            }
        }
        fprintf(stderr,"  light %d shadow volume: %d verts = %d tris\n", li, combined.nverts, combined.nverts/3);
        s->svols[li]=combined;
    }
}
