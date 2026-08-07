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

static void draw_line(vec3 a, vec3 b, vec3 color){
	glColor3f(color.x,color.y,color.z);
	glVertex3f(a.x,a.y,a.z);
	glVertex3f(b.x,b.y,b.z);
}

static void draw_axis_arrow(vec3 center, vec3 axis, float len, vec3 color){
	float hLen=len*0.15f, hWid=len*0.08f;
	vec3 tip=vadd(center, vscale(axis,len));
	vec3 base=vadd(center, vscale(axis,len-hLen));
	vec3 u,v;
	if(fabsf(axis.x)<0.9f){
		u=vnorm(vcross(axis,v3(1,0,0)));
	} else {
		u=vnorm(vcross(axis,v3(0,1,0)));
	}
	v=vnorm(vcross(axis,u));
	vec3 uOff=vscale(u,hWid), vOff=vscale(v,hWid);
	vec3 dv=vadd(uOff,vOff);
	draw_line(center, tip, color);
	draw_line(tip, vadd(base,uOff), color);
	draw_line(tip, vadd(base,dv), color);
	draw_line(tip, vadd(base,vOff), color);
	draw_line(tip, vsub(base,uOff), color);
	draw_line(tip, vsub(base,dv), color);
	draw_line(tip, vsub(base,vOff), color);
}

static void draw_circle(vec3 center, vec3 normal, float r, vec3 color, int segs){
	vec3 u,v;
	if(fabsf(normal.x)<0.9f) u=vnorm(vcross(normal,v3(1,0,0)));
	else u=vnorm(vcross(normal,v3(0,1,0)));
	v=vnorm(vcross(normal,u));
	vec3 prev=vadd(center,vscale(u,r));
	for(int i=1;i<=segs;i++){
		float a=2.0f*M_PIf*(float)i/(float)segs;
		vec3 next=vadd(center,vadd(vscale(u,r*cosf(a)),vscale(v,r*sinf(a))));
		draw_line(prev,next,color);
		prev=next;
	}
}

static void draw_wire_box(vec3 center, vec3 half, vec3 color){
	vec3 p[8]={
		vadd(center,v3(-half.x,-half.y,-half.z)),
		vadd(center,v3( half.x,-half.y,-half.z)),
		vadd(center,v3( half.x, half.y,-half.z)),
		vadd(center,v3(-half.x, half.y,-half.z)),
		vadd(center,v3(-half.x,-half.y, half.z)),
		vadd(center,v3( half.x,-half.y, half.z)),
		vadd(center,v3( half.x, half.y, half.z)),
		vadd(center,v3(-half.x, half.y, half.z)),
	};
	int edges[12][2]={
		{0,1},{1,2},{2,3},{3,0},
		{4,5},{5,6},{6,7},{7,4},
		{0,4},{1,5},{2,6},{3,7}
	};
	for(int i=0;i<12;i++) draw_line(p[edges[i][0]],p[edges[i][1]],color);
}

static vec3 pick_color(vec3 base, int isHovered){
	if(isHovered) return v3(1,1,0);
	return base;
}

static void draw_plane_quad(vec3 center, vec3 u, vec3 v, float s, vec3 color){
	vec3 a=vadd(vadd(center,vscale(u,s)),vscale(v,s));
	vec3 b=vadd(vsub(center,vscale(u,s)),vscale(v,s));
	vec3 c=vsub(vsub(center,vscale(u,s)),vscale(v,s));
	vec3 d=vadd(vsub(center,vscale(u,s)),vscale(v,s));
	(void)c; (void)d;
	/* draw as a small square outline offset from center along both axes */
	vec3 off=vadd(vscale(u,s*2.5f),vscale(v,s*2.5f));
	vec3 p0=vadd(center,vadd(vscale(u,s*1.5f),vscale(v,s*1.5f)));
	vec3 p1=vadd(center,vadd(vscale(u,s*3.5f),vscale(v,s*1.5f)));
	vec3 p2=vadd(center,vadd(vscale(u,s*3.5f),vscale(v,s*3.5f)));
	vec3 p3=vadd(center,vadd(vscale(u,s*1.5f),vscale(v,s*3.5f)));
	(void)off; (void)a; (void)b;
	draw_line(p0,p1,color); draw_line(p1,p2,color);
	draw_line(p2,p3,color); draw_line(p3,p0,color);
}

static void draw_editor_gizmos(Scene *s){
	if(s->selectedObj<0 || s->selectedObj>=s->nobjs) return;
	if(!s->objs[s->selectedObj].renderable) return;

	vec3 bmin,bmax;
	scene_get_obj_bounds(s,s->selectedObj,&bmin,&bmax);
	vec3 center=vscale(vadd(bmin,bmax),0.5f);
	vec3 half=vscale(vsub(bmax,bmin),0.5f);
	float gizmoSize=vlen(half)*0.7f;
	if(gizmoSize<0.3f) gizmoSize=0.3f;

	int hov=s->hoveredHandle;
	vec3 white=v3(1,1,1);
	vec3 red  =pick_color(v3(1,0.2f,0.2f), hov==GIZMO_AXIS_X);
	vec3 green=pick_color(v3(0.2f,1,0.2f), hov==GIZMO_AXIS_Y);
	vec3 blue =pick_color(v3(0.2f,0.2f,1), hov==GIZMO_AXIS_Z);
	vec3 cXY  =pick_color(v3(1,1,0.3f),    hov==GIZMO_PLANE_XY);
	vec3 cXZ  =pick_color(v3(0.3f,1,1),    hov==GIZMO_PLANE_XZ);
	vec3 cYZ  =pick_color(v3(1,0.3f,1),    hov==GIZMO_PLANE_YZ);
	vec3 sx=v3(1,0,0), sy=v3(0,1,0), sz=v3(0,0,1);
	float qs=gizmoSize*0.12f;

	draw_wire_box(center,half,white);

	if(s->editMode==EDIT_W_MOVE){
		draw_axis_arrow(center,sx,gizmoSize,red);
		draw_axis_arrow(center,sy,gizmoSize,green);
		draw_axis_arrow(center,sz,gizmoSize,blue);
		/* plane-quad handles */
		draw_plane_quad(center,sx,sy,qs,cXY);
		draw_plane_quad(center,sx,sz,qs,cXZ);
		draw_plane_quad(center,sy,sz,qs,cYZ);
	} else if(s->editMode==EDIT_E_ROTATE){
		draw_circle(center,sy,gizmoSize,green,48);
		draw_circle(center,sz,gizmoSize,blue,48);
		draw_circle(center,sx,gizmoSize,red,48);
	} else if(s->editMode==EDIT_R_SCALE){
		float h=gizmoSize*0.12f;
		vec3 cCtr=pick_color(white, hov==GIZMO_CENTER);
		draw_line(center,vadd(center,vscale(sx,gizmoSize)),red);
		draw_line(center,vadd(center,vscale(sy,gizmoSize)),green);
		draw_line(center,vadd(center,vscale(sz,gizmoSize)),blue);
		draw_wire_box(vadd(center,vscale(sx,gizmoSize)),v3(h,h,h),red);
		draw_wire_box(vadd(center,vscale(sy,gizmoSize)),v3(h,h,h),green);
		draw_wire_box(vadd(center,vscale(sz,gizmoSize)),v3(h,h,h),blue);
		draw_wire_box(center,v3(h,h,h),cCtr);
	}
}

void render_frame(Scene *s, int w,int h, mat4 proj, mat4 view, vec3 camPos, int flags){
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

    /* Bounding box: depth-tested so it sits on the mesh surface correctly */
    if(s->selectedObj>=0 && s->selectedObj<s->nobjs && s->objs[s->selectedObj].renderable){
        vec3 bmin,bmax;
        scene_get_obj_bounds(s,s->selectedObj,&bmin,&bmax);
        vec3 bc=vscale(vadd(bmin,bmax),0.5f);
        vec3 bh=vscale(vsub(bmax,bmin),0.5f);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
        glDisable(GL_LIGHTING);
        glBegin(GL_LINES);
        draw_wire_box(bc,bh,v3(1,1,1));
        glEnd();
        glDepthMask(GL_TRUE);
    }

    /* Gizmo handles: drawn without depth test so they're always visible */
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    draw_editor_gizmos(s);
    glEnd();
}
