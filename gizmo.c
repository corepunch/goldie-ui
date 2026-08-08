#include <orion/user/gl_compat.h>
#include <math.h>
#include "simplegl.h"

typedef struct {
	float x,y,z,r,g,b;
} GizmoVert;

typedef struct {
	GizmoVert *v;
	int count, cap;
} GizmoLines;

static void gl_add(GizmoLines *gl, vec3 a, vec3 b, vec3 color){
	DA_PUSH(gl->v, gl->count, gl->cap, ((GizmoVert){a.x,a.y,a.z,color.x,color.y,color.z}));
	DA_PUSH(gl->v, gl->count, gl->cap, ((GizmoVert){b.x,b.y,b.z,color.x,color.y,color.z}));
}

static vec3 handle_color(vec3 color,int hovered){
	return hovered?v3(1.0f,0.92f,0.12f):color;
}

static int gizmo_geometry(Scene *s,vec3 camPos,vec3 camLook,float camFov,
	vec3 *center,float *radius,mat4 *matrix,vec3 *bmin,vec3 *bmax){
	if(s->selectedObj<0 || s->selectedObj>=s->nobjs || !s->objs[s->selectedObj].renderable) return 0;
	scene_get_obj_oriented_bounds(s,s->selectedObj,matrix,bmin,bmax);
	*center=mat4_xform_point(*matrix,v3(0,0,0));
	float depth=vdot(vsub(*center,camPos),vnorm(camLook));
	if(depth<=0.0f) return 0;
	*radius=depth*tanf(camFov*M_PIf/360.0f)*0.125f;
	return *radius>1e-6f;
}

static void basis(vec3 normal,vec3 *u,vec3 *v){
	*u=fabsf(normal.x)<0.9f?vnorm(vcross(normal,v3(1,0,0))):vnorm(vcross(normal,v3(0,1,0)));
	*v=vnorm(vcross(normal,*u));
}

static void gl_circle(GizmoLines *gl,vec3 center,vec3 normal,float radius,vec3 color){
	vec3 u,v; basis(normal,&u,&v);
	vec3 prev=vadd(center,vscale(u,radius));
	for(int i=1;i<=64;i++){
		float a=2.0f*M_PIf*(float)i/64.0f;
		vec3 next=vadd(center,vadd(vscale(u,radius*cosf(a)),vscale(v,radius*sinf(a))));
		gl_add(gl,prev,next,color); prev=next;
	}
}

static void gl_cone(GizmoLines *gl,vec3 tip,vec3 axis,float length,float width,vec3 color){
	vec3 u,v; basis(axis,&u,&v);
	vec3 base=vsub(tip,vscale(axis,length));
	vec3 prev=vadd(base,vscale(u,width));
	for(int i=1;i<=8;i++){
		float a=2.0f*M_PIf*(float)i/8.0f;
		vec3 next=vadd(base,vadd(vscale(u,width*cosf(a)),vscale(v,width*sinf(a))));
		gl_add(gl,prev,next,color); gl_add(gl,tip,prev,color); prev=next;
	}
}

static void gl_axis_arrow(GizmoLines *gl,vec3 center,vec3 axis,float radius,vec3 color){
	vec3 base=vadd(center,vscale(axis,radius*0.74f));
	gl_add(gl,center,base,color);
	gl_cone(gl,vadd(center,vscale(axis,radius)),axis,radius*0.26f,radius*0.085f,color);
}

static void gl_axis_line(GizmoLines *gl,vec3 center,vec3 axis,float radius,vec3 color){
	vec3 ext=vscale(axis,radius*100.0f);
	gl_add(gl,vsub(center,ext),vadd(center,ext),color);
}

static void gl_plane(GizmoLines *gl,vec3 center,vec3 u,vec3 v,float radius,vec3 color){
	float hi=radius*0.38f;
	vec3 far=vadd(center,vadd(vscale(u,hi),vscale(v,hi)));
	gl_add(gl,vadd(center,vscale(v,hi)),far,color);
	gl_add(gl,vadd(center,vscale(u,hi)),far,color);
}

static void gl_box(GizmoLines *gl,vec3 center,float half,vec3 color){
	vec3 p[8]={
		vadd(center,v3(-half,-half,-half)),vadd(center,v3(half,-half,-half)),
		vadd(center,v3(half,half,-half)),vadd(center,v3(-half,half,-half)),
		vadd(center,v3(-half,-half,half)),vadd(center,v3(half,-half,half)),
		vadd(center,v3(half,half,half)),vadd(center,v3(-half,half,half))};
	int e[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
	for(int i=0;i<12;i++) gl_add(gl,p[e[i][0]],p[e[i][1]],color);
}

static void gl_bounds_corners(GizmoLines *gl,mat4 matrix,vec3 bmin,vec3 bmax){
	vec3 s=v3(bmax.x-bmin.x,bmax.y-bmin.y,bmax.z-bmin.z);
	float ex=s.x*0.1f,ey=s.y*0.1f,ez=s.z*0.1f;
	vec3 color=v3(0.88f,0.88f,0.88f);
	for(int ci=0;ci<8;ci++){
		vec3 c=v3((ci&1)?bmax.x:bmin.x,(ci&2)?bmax.y:bmin.y,(ci&4)?bmax.z:bmin.z);
		float sx=(ci&1)?-1.0f:1.0f,sy=(ci&2)?-1.0f:1.0f,sz=(ci&4)?-1.0f:1.0f;
		vec3 wc=mat4_xform_point(matrix,c);
		gl_add(gl,wc,mat4_xform_point(matrix,vadd(c,v3(ex*sx,0,0))),color);
		gl_add(gl,wc,mat4_xform_point(matrix,vadd(c,v3(0,ey*sy,0))),color);
		gl_add(gl,wc,mat4_xform_point(matrix,vadd(c,v3(0,0,ez*sz))),color);
	}
}

static int axis_visible(int lock,int handle){
	if(!lock) return 1;
	switch(lock){
		case GIZMO_AXIS_X: return handle==GIZMO_AXIS_X;
		case GIZMO_AXIS_Y: return handle==GIZMO_AXIS_Y;
		case GIZMO_AXIS_Z: return handle==GIZMO_AXIS_Z;
		case GIZMO_PLANE_XY: return handle==GIZMO_AXIS_X||handle==GIZMO_AXIS_Y||handle==GIZMO_PLANE_XY;
		case GIZMO_PLANE_XZ: return handle==GIZMO_AXIS_X||handle==GIZMO_AXIS_Z||handle==GIZMO_PLANE_XZ;
		case GIZMO_PLANE_YZ: return handle==GIZMO_AXIS_Y||handle==GIZMO_AXIS_Z||handle==GIZMO_PLANE_YZ;
	}
	return 0;
}

static GLuint s_gizmo_vao, s_gizmo_vbo;
extern GLuint s_line_prog;
extern GLint  s_line_proj_loc, s_line_view_loc;
static void ensure_gizmo_buf(void) { if (!s_gizmo_vao) { glGenVertexArrays(1, &s_gizmo_vao); glGenBuffers(1, &s_gizmo_vbo); } }
extern void ensure_line_prog(void);

void gizmo_draw(Scene *s,vec3 camPos,vec3 camLook,float camFov){
	vec3 center,bmin,bmax; float radius; mat4 matrix;
	if(!gizmo_geometry(s,camPos,camLook,camFov,&center,&radius,&matrix,&bmin,&bmax)) return;
	vec3 x=v3(1,0,0),y=v3(0,1,0),z=v3(0,0,1);
	vec3 red=handle_color(v3(0.92f,0.12f,0.08f),s->hoveredHandle==GIZMO_AXIS_X);
	vec3 green=handle_color(v3(0.12f,0.82f,0.10f),s->hoveredHandle==GIZMO_AXIS_Y);
	vec3 blue=handle_color(v3(0.10f,0.35f,1.0f),s->hoveredHandle==GIZMO_AXIS_Z);
	vec3 xy=handle_color(v3(0.92f,0.82f,0.08f),s->hoveredHandle==GIZMO_PLANE_XY);
	vec3 xz=handle_color(v3(0.82f,0.18f,0.72f),s->hoveredHandle==GIZMO_PLANE_XZ);
	vec3 yz=handle_color(v3(0.10f,0.75f,0.78f),s->hoveredHandle==GIZMO_PLANE_YZ);
	glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND); glLineWidth(1.0f);
	GizmoLines gl={0};
	gl_bounds_corners(&gl,matrix,bmin,bmax);
	if(s->editMode==EDIT_W_MOVE){
		if(axis_visible(s->axisLock,GIZMO_AXIS_X)){
			if(s->axisLock) gl_axis_line(&gl,center,x,radius,red); else gl_axis_arrow(&gl,center,x,radius,red);
		}
		if(axis_visible(s->axisLock,GIZMO_AXIS_Y)){
			if(s->axisLock) gl_axis_line(&gl,center,y,radius,green); else gl_axis_arrow(&gl,center,y,radius,green);
		}
		if(axis_visible(s->axisLock,GIZMO_AXIS_Z)){
			if(s->axisLock) gl_axis_line(&gl,center,z,radius,blue); else gl_axis_arrow(&gl,center,z,radius,blue);
		}
		if(axis_visible(s->axisLock,GIZMO_PLANE_XY)) gl_plane(&gl,center,x,y,radius,xy);
		if(axis_visible(s->axisLock,GIZMO_PLANE_XZ)) gl_plane(&gl,center,x,z,radius,xz);
		if(axis_visible(s->axisLock,GIZMO_PLANE_YZ)) gl_plane(&gl,center,y,z,radius,yz);
	} else if(s->editMode==EDIT_E_ROTATE){
		if(axis_visible(s->axisLock,GIZMO_AXIS_X)) gl_circle(&gl,center,x,radius,red);
		if(axis_visible(s->axisLock,GIZMO_AXIS_Y)) gl_circle(&gl,center,y,radius,green);
		if(axis_visible(s->axisLock,GIZMO_AXIS_Z)) gl_circle(&gl,center,z,radius,blue);
	} else if(s->editMode==EDIT_R_SCALE){
		float half=radius*0.065f;
		if(axis_visible(s->axisLock,GIZMO_AXIS_X)){
			if(s->axisLock) gl_axis_line(&gl,center,x,radius,red);
			else { gl_add(&gl,center,vadd(center,vscale(x,radius)),red); gl_box(&gl,vadd(center,vscale(x,radius)),half,red); }
		}
		if(axis_visible(s->axisLock,GIZMO_AXIS_Y)){
			if(s->axisLock) gl_axis_line(&gl,center,y,radius,green);
			else { gl_add(&gl,center,vadd(center,vscale(y,radius)),green); gl_box(&gl,vadd(center,vscale(y,radius)),half,green); }
		}
		if(axis_visible(s->axisLock,GIZMO_AXIS_Z)){
			if(s->axisLock) gl_axis_line(&gl,center,z,radius,blue);
			else { gl_add(&gl,center,vadd(center,vscale(z,radius)),blue); gl_box(&gl,vadd(center,vscale(z,radius)),half,blue); }
		}
		if(!s->axisLock) gl_box(&gl,center,half*1.15f,handle_color(v3(0.88f,0.88f,0.88f),s->hoveredHandle==GIZMO_CENTER));
	}
	if(gl.count>0){
		ensure_gizmo_buf();
		ensure_line_prog();
		if(s_line_prog){
			glUseProgram(s_line_prog);
			glBindVertexArray(s_gizmo_vao);
			glBindBuffer(GL_ARRAY_BUFFER, s_gizmo_vbo);
			glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(gl.count*sizeof(GizmoVert)), gl.v, GL_DYNAMIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GizmoVert), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GizmoVert), (void*)(3*sizeof(float)));
			glDrawArrays(GL_LINES, 0, gl.count);
			glDisableVertexAttribArray(0);
			glDisableVertexAttribArray(1);
			glBindVertexArray(0);
			glUseProgram(0);
		}
	}
	free(gl.v);
	glLineWidth(1.0f);
}

static float ray_axis(vec3 ro,vec3 rd,vec3 base,vec3 axis,float length,float threshold){
	vec3 ao=vsub(ro,base);
	float da=vdot(rd,axis),oa=vdot(ao,axis),od=vdot(ao,rd);
	float denom=1.0f-da*da;
	if(denom<1e-6f) return -1.0f;
	float t=(da*oa-od)/denom;
	float along=oa+da*t;
	if(t<0.0f || along<0.0f || along>length) return -1.0f;
	vec3 hit=vadd(ro,vscale(rd,t)),onAxis=vadd(base,vscale(axis,along));
	return vlen(vsub(hit,onAxis))<=threshold?t:-1.0f;
}

static float ray_sphere(vec3 ro,vec3 rd,vec3 center,float radius){
	vec3 oc=vsub(ro,center);
	float b=vdot(oc,rd),c=vdot(oc,oc)-radius*radius,disc=b*b-c;
	if(disc<0.0f) return -1.0f;
	float root=sqrtf(disc),t=-b-root;
	if(t<0.0f) t=-b+root;
	return t>=0.0f?t:-1.0f;
}

static float ray_plane_quad(vec3 ro,vec3 rd,vec3 center,vec3 u,vec3 v,float radius){
	float hi=radius*0.42f;
	vec3 far=vadd(center,vadd(vscale(u,hi),vscale(v,hi)));
	vec3 a=vadd(center,vscale(v,hi));
	vec3 b=vadd(center,vscale(u,hi));
	vec3 segs[2][2]={{a,far},{b,far}};
	float bestT=1e30f;
	for(int i=0;i<2;i++){
		vec3 ab=vsub(segs[i][1],segs[i][0]);
		vec3 ao=vsub(ro,segs[i][0]);
		vec3 cross=vcross(rd,ab);
		float denom=vdot(cross,cross);
		float t=vdot(vcross(ao,ab),cross)/denom;
		if(t<0.0f || t>bestT) continue;
		float along=vdot(vadd(ro,vscale(rd,t)),ab)-vdot(segs[i][0],ab);
		if(along<0.0f || along>vdot(ab,ab)) continue;
		vec3 hit=vadd(ro,vscale(rd,t));
		vec3 proj=vadd(segs[i][0],vscale(ab,along/vdot(ab,ab)));
		if(vlen(vsub(hit,proj))<=radius*0.105f) bestT=t;
	}
	return bestT<1e30f?bestT:-1.0f;
}

static void take_hit(float t,int handle,float *bestT,int *bestHandle){
	if(t>=0.0f && t<*bestT){ *bestT=t; *bestHandle=handle; }
}

int gizmo_pick_handle(Scene *s,vec3 ro,vec3 rd,vec3 camLook,float camFov){
	vec3 center,bmin,bmax; float radius; mat4 matrix;
	if(!gizmo_geometry(s,ro,camLook,camFov,&center,&radius,&matrix,&bmin,&bmax)) return GIZMO_NONE;
	vec3 axis[3]={v3(1,0,0),v3(0,1,0),v3(0,0,1)};
	int id[3]={GIZMO_AXIS_X,GIZMO_AXIS_Y,GIZMO_AXIS_Z};
	float bestT=1e30f,threshold=radius*0.105f;
	int best=GIZMO_NONE;
	if(s->editMode==EDIT_W_MOVE || s->editMode==EDIT_R_SCALE){
		float axLen=s->axisLock?radius*100.0f:radius;
		for(int i=0;i<3;i++){
			take_hit(ray_axis(ro,rd,center,axis[i],axLen,threshold),id[i],&bestT,&best);
			if(!s->axisLock) take_hit(ray_sphere(ro,rd,vadd(center,vscale(axis[i],radius)),radius*0.12f),id[i],&bestT,&best);
		}
		if(s->editMode==EDIT_W_MOVE){
			take_hit(ray_plane_quad(ro,rd,center,axis[0],axis[1],radius),GIZMO_PLANE_XY,&bestT,&best);
			take_hit(ray_plane_quad(ro,rd,center,axis[0],axis[2],radius),GIZMO_PLANE_XZ,&bestT,&best);
			take_hit(ray_plane_quad(ro,rd,center,axis[1],axis[2],radius),GIZMO_PLANE_YZ,&bestT,&best);
		} else take_hit(ray_sphere(ro,rd,center,radius*0.11f),GIZMO_CENTER,&bestT,&best);
	} else if(s->editMode==EDIT_E_ROTATE){
		for(int a=0;a<3;a++){
			vec3 u,v; basis(axis[a],&u,&v);
			for(int i=0;i<64;i++){
				float angle=2.0f*M_PIf*(float)i/64.0f;
				vec3 point=vadd(center,vadd(vscale(u,radius*cosf(angle)),vscale(v,radius*sinf(angle))));
				take_hit(ray_sphere(ro,rd,point,threshold),id[a],&bestT,&best);
			}
		}
	}
	if(s->axisLock && !axis_visible(s->axisLock,best)) best=GIZMO_NONE;
	return best;
}
