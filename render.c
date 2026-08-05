#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <stdio.h>
#include <string.h>
#include "simplegl.h"

#ifndef USE_ZPASS
static int extension_present(const char *name){
    const char *base=(const char*)glGetString(GL_EXTENSIONS), *ext=base;
    size_t n=strlen(name);
    if(!ext || !n || strchr(name,' ')) return 0;
    while((ext=strstr(ext,name))){
        if((ext==base || ext[-1]==' ') &&
           (ext[n]==' ' || ext[n]=='\0')) return 1;
        ext+=n;
    }
    return 0;
}

static int shadows_supported(void){
    static int checked, supported;
    if(!checked){
        supported=extension_present("GL_ARB_depth_clamp") ||
                  extension_present("GL_NV_depth_clamp") ||
                  extension_present("GL_EXT_depth_clamp");
        if(supported) fprintf(stderr,"stencil shadows: supported (depth clamp available)\n");
        else fprintf(stderr,"stencil shadows: not supported (depth clamp unavailable); rendering lights without shadows\n");
        checked=1;
    }
    return supported;
}
#endif

static void begin_shadow_pass(void){
#ifdef USE_ZPASS
    glStencilOpSeparate(GL_BACK,GL_KEEP,GL_KEEP,GL_INCR_WRAP);
    glStencilOpSeparate(GL_FRONT,GL_KEEP,GL_KEEP,GL_DECR_WRAP);
#else
    glEnable(GL_DEPTH_CLAMP);
    glStencilOpSeparate(GL_BACK,GL_KEEP,GL_INCR_WRAP,GL_KEEP);
    glStencilOpSeparate(GL_FRONT,GL_KEEP,GL_DECR_WRAP,GL_KEEP);
#endif
}

static void end_shadow_pass(void){
#ifndef USE_ZPASS
    glDisable(GL_DEPTH_CLAMP);
#endif
}

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
    for(int i=0;i<sv->nverts;i++) glVertex4fv(&sv->verts[i].x);
    glEnd();
}

void render_frame(Scene *s, int w,int h, mat4 proj, mat4 view, int debugMode){
    glViewport(0,0,w,h);
    glClearColor(s->bg.x,s->bg.y,s->bg.z,1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION); glLoadMatrixf(proj.m);
    glMatrixMode(GL_MODELVIEW);  glLoadMatrixf(view.m);

    if(debugMode==DBG_WHITE_WIREFRAME){
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);
        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);
        glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
        for(int i=0;i<s->nobjs;i++) if(s->objs[i].renderable)
            draw_mesh_flat(&s->objs[i].mesh,v3(1,1,1));
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        return;
    }

    /* Pass 1: Render scene with ambient lighting (fills depth buffer) */
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    glDisable(GL_LIGHTING);
    for(int i=0;i<s->nobjs;i++) if(s->objs[i].renderable)
        draw_mesh_flat(&s->objs[i].mesh, s->objs[i].unlit ? s->objs[i].color
                                                         : vmul(s->objs[i].color, s->ambient));

    if(debugMode==DBG_SHOW_STENCIL){
        for(int li=0; li<s->nlights; li++){
            Light *L=&s->lights[li];
            if(!L->castsShadow || s->svols[li].nverts==0) continue;
#ifndef USE_ZPASS
            if(!shadows_supported()) continue;
#endif
            glClear(GL_STENCIL_BUFFER_BIT);
            glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
            glDepthMask(GL_FALSE);
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS,0,0xFF);
            glDisable(GL_CULL_FACE);
            begin_shadow_pass();
            draw_shadow_volume(&s->svols[li]);
            end_shadow_pass();
            glDisable(GL_STENCIL_TEST);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);

            glDisable(GL_DEPTH_TEST);
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_NOTEQUAL,0,0xFF);
            glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
            glColor3f(1,0,0);
            glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
            glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
            glBegin(GL_QUADS);
            glVertex2f(-1,-1); glVertex2f(1,-1); glVertex2f(1,1); glVertex2f(-1,1);
            glEnd();
            glPopMatrix();
            glMatrixMode(GL_PROJECTION); glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glDisable(GL_STENCIL_TEST);
            glEnable(GL_DEPTH_TEST);
            return;
        }
        return;
    }

    /* Pass 2+3: Per-light shadow volume + lit pass */
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL); glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    GLfloat zero[4]={0,0,0,1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
    glLightfv(GL_LIGHT0, GL_AMBIENT, zero);

    for(int li=0; li<s->nlights; li++){
        Light *L=&s->lights[li];
        GLfloat lp[4];
        if(L->isDirectional){
            vec3 toSource=light_to_source(L,v3(0,0,0));
            lp[0]=toSource.x; lp[1]=toSource.y; lp[2]=toSource.z; lp[3]=0.0f;
        } else {
            lp[0]=L->pos.x; lp[1]=L->pos.y; lp[2]=L->pos.z; lp[3]=1.0f;
        }
		GLfloat lc[4]={L->color.x*L->intensity, L->color.y*L->intensity, L->color.z*L->intensity, 1.0f};
		glLightfv(GL_LIGHT0, GL_POSITION, lp);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, lc);
		glLightfv(GL_LIGHT0, GL_SPECULAR, lc);
		if(!L->isDirectional && L->radius > 0.0f){
			float r=L->radius;
			glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
			glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 2.0f/r);
			glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 1.0f/(r*r));
		} else {
			glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
			glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.0f);
			glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.0f);
		}

        int drawShadows=debugMode!=DBG_NO_SHADOWS && L->castsShadow && s->svols[li].nverts>0;
#ifndef USE_ZPASS
        drawShadows=drawShadows && shadows_supported();
#endif
        if(drawShadows){
            glClear(GL_STENCIL_BUFFER_BIT);
            glDisable(GL_LIGHTING);
            glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
            glDepthMask(GL_FALSE);
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS,0,0xFF);
            glDisable(GL_CULL_FACE);
            begin_shadow_pass();
            draw_shadow_volume(&s->svols[li]);
            end_shadow_pass();

            if(debugMode==DBG_WIRE_SHADOWVOL){
                glDisable(GL_STENCIL_TEST);
                glEnable(GL_POLYGON_OFFSET_LINE);
                glPolygonOffset(-1,-1);
                glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
                glDepthFunc(GL_LEQUAL);
                glColor3f(1,0,0);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                draw_shadow_volume(&s->svols[li]);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                glDisable(GL_POLYGON_OFFSET_LINE);
                glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
                glDepthFunc(GL_LESS);
                glEnable(GL_STENCIL_TEST);
            }

            /* Pass 3: Lit — draw where stencil==0 (not in shadow) */
            glEnable(GL_CULL_FACE);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            glStencilFunc(GL_EQUAL,0,0xFF);
            glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE);
            glEnable(GL_LIGHTING);
            for(int i=0;i<s->nobjs;i++) if(s->objs[i].renderable && !s->objs[i].unlit)
                draw_mesh_lit(&s->objs[i].mesh, s->objs[i].color, s->objs[i].shininess);
            glDisable(GL_BLEND);
            glDisable(GL_STENCIL_TEST);
            glDepthFunc(GL_LESS);
        } else {
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE);
            for(int i=0;i<s->nobjs;i++) if(s->objs[i].renderable && !s->objs[i].unlit)
                draw_mesh_lit(&s->objs[i].mesh, s->objs[i].color, s->objs[i].shininess);
            glDisable(GL_BLEND);
            glDepthFunc(GL_LESS);
        }
    }
    glDepthMask(GL_TRUE);
}
