#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "simplegl.h"
#include "shader.h"

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
static vec3 srgb_to_linear(vec3 color){
    return v3(powf(color.x,2.2f),powf(color.y,2.2f),powf(color.z,2.2f));
}
static void draw_shadow_volume(ShadowVolume *sv){
    glBegin(GL_TRIANGLES);
    for(int i=0;i<sv->nverts;i++) glVertex4fv(&sv->verts[i].x);
    glEnd();
}

void render_frame(Scene *s,int w,int h,mat4 proj,mat4 view,vec3 camPos,vec3 camLook,int flags){
    glEnable(GL_FRAMEBUFFER_SRGB);
    glViewport(0,0,w,h);
    vec3 bg=srgb_to_linear(s->bg);
    glClearColor(bg.x,bg.y,bg.z,1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION); glLoadMatrixf(proj.m);
    glMatrixMode(GL_MODELVIEW);  glLoadMatrixf(view.m);

    /* Pass 1: ambient fill (fills depth buffer) */
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    glDisable(GL_LIGHTING);
    vec3 ambient=srgb_to_linear(s->ambient);
    for(int i=0;i<s->nobjs;i++) if(s->objs[i].renderable){
        vec3 color=srgb_to_linear(s->objs[i].color);
        draw_mesh_flat(&s->objs[i].mesh, s->objs[i].unlit ? color : vmul(color,ambient));
    }

    /* Per-light pass: shadow volume + PBR lit */
    mat4 viewProj = mat4_mul(proj, view);
    shader_bind();
    shader_set_viewproj(viewProj);
    shader_set_camera_pos(camPos);

    for(int li=0; li<s->nlights; li++){
        Light *L=&s->lights[li];
        shader_set_light(L);

        int drawShadows=!(flags&DBG_NO_SHADOWS) && L->castsShadow && s->svols[li].nverts>0;
#ifndef USE_ZPASS
        drawShadows=drawShadows && shadows_supported();
#endif
        if(drawShadows){
            glClear(GL_STENCIL_BUFFER_BIT);
            shader_unbind();
            glDisable(GL_LIGHTING);
            glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
            glDepthMask(GL_FALSE);
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS,0,0xFF);
            glDisable(GL_CULL_FACE);
            begin_shadow_pass();
            draw_shadow_volume(&s->svols[li]);
            end_shadow_pass();

            if(flags&DBG_WIRE_SHADOWVOL){
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

            /* Lit via PBR shader where stencil==0 */
            shader_bind();
            glEnable(GL_CULL_FACE);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            glStencilFunc(GL_EQUAL,0,0xFF);
            glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE);
            for(int i=0;i<s->nobjs;i++) if(s->objs[i].renderable && !s->objs[i].unlit){
                shader_set_material(s->objs[i].color, s->objs[i].shininess);
                shader_set_texture(s->objs[i].texIndex>=0?s->materialTextures[s->objs[i].texIndex]:s->whiteTexture);
                shader_draw_mesh(&s->objs[i].mesh);
            }
            glDisable(GL_BLEND);
            glDisable(GL_STENCIL_TEST);
            glDepthFunc(GL_LESS);
        } else {
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE);
            for(int i=0;i<s->nobjs;i++) if(s->objs[i].renderable && !s->objs[i].unlit){
                shader_set_material(s->objs[i].color, s->objs[i].shininess);
                shader_set_texture(s->objs[i].texIndex>=0?s->materialTextures[s->objs[i].texIndex]:s->whiteTexture);
                shader_draw_mesh(&s->objs[i].mesh);
            }
            glDisable(GL_BLEND);
            glDepthFunc(GL_LESS);
        }
    }

    shader_unbind();
    glDepthMask(GL_TRUE);

    /* Pass 4: overlay lines */
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glBegin(GL_LINES);
    for(int i=0;i<s->noverlayLines;i++){
        OverlayLine *ln=&s->overlayLines[i];
        if((flags&DBG_HIDE_CHARS) && ln->category==0) continue;
        if((flags&DBG_HIDE_LIGHTS) && ln->category>=1) continue;
        if(ln->camera[0] && strcmp(ln->camera,s->activeCamera)) continue;
        vec3 color=srgb_to_linear(ln->color);
        glColor3f(color.x,color.y,color.z);
        glVertex3f(ln->start.x,ln->start.y,ln->start.z);
        glVertex3f(ln->end.x,ln->end.y,ln->end.z);
    }
    glEnd();

    if(flags&DBG_SHOW_STENCIL){
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
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
        glDepthMask(GL_TRUE);
    }

    gizmo_draw(s,camPos,camLook,s->camFov);
}
