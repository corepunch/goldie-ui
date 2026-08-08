#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <math.h>
#include "simplegl.h"

static vec3 handle_color(vec3 color,int hovered){
	return hovered?v3(1.0f,0.92f,0.12f):color;
}

static int gizmo_geometry(Scene *s,vec3 camPos,vec3 camLook,float camFov,
	vec3 *center,float *radius,mat4 *matrix,vec3 *bmin,vec3 *bmax){
	if(s->selectedObj<0 || s->selectedObj>=s->nobjs || !s->objs[s->selectedObj].renderable) return 0;
	scene_get_obj_oriented_bounds(s,s->selectedObj,matrix,bmin,bmax);
	*center=mat4_xform_point(*matrix,vscale(vadd(*bmin,*bmax),0.5f));
	float depth=vdot(vsub(*center,camPos),vnorm(camLook));
	if(depth<=0.0f) return 0;
	*radius=depth*tanf(camFov*M_PIf/360.0f)*0.125f;
	return *radius>1e-6f;
}

static void line(vec3 a,vec3 b,vec3 color){
	glColor3f(color.x,color.y,color.z);
	glVertex3fv(&a.x); glVertex3fv(&b.x);
}

static void basis(vec3 normal,vec3 *u,vec3 *v){
	*u=fabsf(normal.x)<0.9f?vnorm(vcross(normal,v3(1,0,0))):vnorm(vcross(normal,v3(0,1,0)));
	*v=vnorm(vcross(normal,*u));
}

static void draw_circle(vec3 center,vec3 normal,float radius,vec3 color){
	vec3 u,v; basis(normal,&u,&v);
	vec3 prev=vadd(center,vscale(u,radius));
	glBegin(GL_LINES);
	for(int i=1;i<=64;i++){
		float a=2.0f*M_PIf*(float)i/64.0f;
		vec3 next=vadd(center,vadd(vscale(u,radius*cosf(a)),vscale(v,radius*sinf(a))));
		line(prev,next,color); prev=next;
	}
	glEnd();
}

static void draw_cone(vec3 tip,vec3 axis,float length,float width,vec3 color){
	vec3 u,v; basis(axis,&u,&v);
	vec3 base=vsub(tip,vscale(axis,length));
	vec3 prev=vadd(base,vscale(u,width));
	glBegin(GL_LINES);
	for(int i=1;i<=8;i++){
		float a=2.0f*M_PIf*(float)i/8.0f;
		vec3 next=vadd(base,vadd(vscale(u,width*cosf(a)),vscale(v,width*sinf(a))));
		line(prev,next,color); line(tip,prev,color); prev=next;
	}
	glEnd();
}

static void draw_axis_arrow(vec3 center,vec3 axis,float radius,vec3 color){
	vec3 base=vadd(center,vscale(axis,radius*0.74f));
	vec3 tip=vadd(center,vscale(axis,radius));
	glBegin(GL_LINES); line(center,base,color); glEnd();
	draw_cone(tip,axis,radius*0.26f,radius*0.085f,color);
}

static void box_points(vec3 center,float half,vec3 p[8]){
	p[0]=vadd(center,v3(-half,-half,-half)); p[1]=vadd(center,v3(half,-half,-half));
	p[2]=vadd(center,v3(half,half,-half)); p[3]=vadd(center,v3(-half,half,-half));
	p[4]=vadd(center,v3(-half,-half,half)); p[5]=vadd(center,v3(half,-half,half));
	p[6]=vadd(center,v3(half,half,half)); p[7]=vadd(center,v3(-half,half,half));
}

static void draw_box(vec3 center,float half,vec3 color){
	vec3 p[8]; box_points(center,half,p);
	int e[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
	glBegin(GL_LINES);
	for(int i=0;i<12;i++) line(p[e[i][0]],p[e[i][1]],color);
	glEnd();
}

static void draw_bounds(mat4 matrix,vec3 bmin,vec3 bmax){
	vec3 p[8]={v3(bmin.x,bmin.y,bmin.z),v3(bmax.x,bmin.y,bmin.z),
		v3(bmax.x,bmax.y,bmin.z),v3(bmin.x,bmax.y,bmin.z),
		v3(bmin.x,bmin.y,bmax.z),v3(bmax.x,bmin.y,bmax.z),
		v3(bmax.x,bmax.y,bmax.z),v3(bmin.x,bmax.y,bmax.z)};
	int e[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
	for(int i=0;i<8;i++) p[i]=mat4_xform_point(matrix,p[i]);
	glBegin(GL_LINES);
	for(int i=0;i<12;i++) line(p[e[i][0]],p[e[i][1]],v3(0.88f,0.88f,0.88f));
	glEnd();
}

static void plane_points(vec3 center,vec3 u,vec3 v,float radius,vec3 p[4]){
	float lo=radius*0.18f,hi=radius*0.38f;
	p[0]=vadd(center,vadd(vscale(u,lo),vscale(v,lo)));
	p[1]=vadd(center,vadd(vscale(u,hi),vscale(v,lo)));
	p[2]=vadd(center,vadd(vscale(u,hi),vscale(v,hi)));
	p[3]=vadd(center,vadd(vscale(u,lo),vscale(v,hi)));
}

static void draw_plane(vec3 center,vec3 u,vec3 v,float radius,vec3 color){
	vec3 p[4]; plane_points(center,u,v,radius,p);
	glBegin(GL_LINES);
	for(int i=0;i<4;i++) line(p[i],p[(i+1)%4],color);
	glEnd();
}

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
	glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND); glLineWidth(1.0f); draw_bounds(matrix,bmin,bmax);
	if(s->editMode==EDIT_W_MOVE){
		draw_axis_arrow(center,x,radius,red); draw_axis_arrow(center,y,radius,green); draw_axis_arrow(center,z,radius,blue);
		draw_plane(center,x,y,radius,xy); draw_plane(center,x,z,radius,xz); draw_plane(center,y,z,radius,yz);
	} else if(s->editMode==EDIT_E_ROTATE){
		draw_circle(center,x,radius,red); draw_circle(center,y,radius,green); draw_circle(center,z,radius,blue);
	} else if(s->editMode==EDIT_R_SCALE){
		float half=radius*0.065f;
		glBegin(GL_LINES);
		line(center,vadd(center,vscale(x,radius)),red); line(center,vadd(center,vscale(y,radius)),green); line(center,vadd(center,vscale(z,radius)),blue);
		glEnd();
		draw_box(vadd(center,vscale(x,radius)),half,red); draw_box(vadd(center,vscale(y,radius)),half,green);
		draw_box(vadd(center,vscale(z,radius)),half,blue);
		draw_box(center,half*1.15f,handle_color(v3(0.88f,0.88f,0.88f),s->hoveredHandle==GIZMO_CENTER));
	}
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
	vec3 normal=vcross(u,v);
	float denom=vdot(rd,normal);
	if(fabsf(denom)<1e-6f) return -1.0f;
	float t=vdot(vsub(center,ro),normal)/denom;
	if(t<0.0f) return -1.0f;
	vec3 d=vsub(vadd(ro,vscale(rd,t)),center);
	float a=vdot(d,u),b=vdot(d,v),lo=radius*0.14f,hi=radius*0.42f;
	return a>=lo && a<=hi && b>=lo && b<=hi?t:-1.0f;
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
		for(int i=0;i<3;i++){
			take_hit(ray_axis(ro,rd,center,axis[i],radius,threshold),id[i],&bestT,&best);
			take_hit(ray_sphere(ro,rd,vadd(center,vscale(axis[i],radius)),radius*0.12f),id[i],&bestT,&best);
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
	return best;
}
