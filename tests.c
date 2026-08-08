#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "simplegl.h"

static int failures = 0;
#define CHECK(cond, fmt, ...) do{ if(!(cond)){ fprintf(stderr,"FAIL: " fmt "\n", ##__VA_ARGS__); failures++; } }while(0)
#define PASS(msg) fprintf(stderr,"  OK  %s\n", msg)

#ifndef USE_ZPASS
static int shadow_vert_eq(ShadowVertex a, ShadowVertex b){
    return a.x==b.x && a.y==b.y && a.z==b.z && a.w==b.w;
}

static int shadow_volume_closed(ShadowVolume *sv){
    for(int i=0;i<sv->nverts;i++){
        int ni=(i/3)*3+(i+1)%3, mates=0;
        for(int j=0;j<sv->nverts;j++){
            int nj=(j/3)*3+(j+1)%3;
            if(shadow_vert_eq(sv->verts[i],sv->verts[nj]) &&
               shadow_vert_eq(sv->verts[ni],sv->verts[j])) mates++;
        }
        if(mates!=1) return 0;
    }
    return 1;
}
#endif

static void test_edges_sealed(const char *name, Mesh m, int expect_sealed){
    int open_edges=0;
    for(int i=0;i<m.nedges;i++){
        if(m.edges[i].t1 < 0) open_edges++;
    }
    if(expect_sealed){
        CHECK(open_edges==0, "%s: %d unsealed edges (expected 0)", name, open_edges);
        if(open_edges==0) PASS(name);
    } else {
        fprintf(stderr,"  OK  %s (%d open edges, expected non-zero)\n", name, open_edges);
    }
}

static float approx_cylinder_vol(float r, float h){ return M_PIf * r * r * h; }
static float approx_box_vol(float sx, float sy, float sz){ return sx * sy * sz; }
static float approx_sphere_vol(float r){ return 4.0f/3.0f * M_PIf * r * r * r; }
static float approx_torus_vol(float R, float r){ return 2.0f * M_PIf * M_PIf * R * r * r; }

static float mesh_width(Mesh *m){
	float lo=m->verts[0].pos.x,hi=lo;
	for(int i=1;i<m->nverts;i++){
		if(m->verts[i].pos.x<lo) lo=m->verts[i].pos.x;
		if(m->verts[i].pos.x>hi) hi=m->verts[i].pos.x;
	}
	return hi-lo;
}

static float mesh_min_y(Mesh *m){
	float lo=m->verts[0].pos.y;
	for(int i=1;i<m->nverts;i++) if(m->verts[i].pos.y<lo) lo=m->verts[i].pos.y;
	return lo;
}

static int point_inside_arch(float x,float y,float w,float h){
	float hw=w*0.5f,hh=h*0.5f,spring=hh-hw;
	if(x<=-hw+1e-4f || x>=hw-1e-4f || y<=-hh+1e-4f) return 0;
	if(y<=spring) return 1;
	return x*x+(y-spring)*(y-spring)<hw*hw-1e-4f;
}

static int color_eq(vec3 a,vec3 b){
	return fabsf(a.x-b.x)<0.001f && fabsf(a.y-b.y)<0.001f && fabsf(a.z-b.z)<0.001f;
}

static int scene_point_covered(Scene *s,vec3 p){
	for(int i=0;i<s->nobjs;i++){
		Mesh *m=&s->objs[i].mesh;
		vec3 lo=m->verts[0].pos,hi=lo;
		for(int j=1;j<m->nverts;j++){
			vec3 v=m->verts[j].pos;
			if(v.x<lo.x) lo.x=v.x;
			if(v.x>hi.x) hi.x=v.x;
			if(v.y<lo.y) lo.y=v.y;
			if(v.y>hi.y) hi.y=v.y;
			if(v.z<lo.z) lo.z=v.z;
			if(v.z>hi.z) hi.z=v.z;
		}
		if(p.x>=lo.x-0.001f && p.x<=hi.x+0.001f &&
		   p.y>=lo.y-0.001f && p.y<=hi.y+0.001f &&
		   p.z>=lo.z-0.001f && p.z<=hi.z+0.001f) return 1;
	}
	return 0;
}

static int overlay_camera_stats(Scene *s,const char *camera,float *maxY){
	int count=0;
	*maxY=-INFINITY;
	for(int i=0;i<s->noverlayLines;i++){
		OverlayLine *line=&s->overlayLines[i];
		if(line->category || strcmp(line->camera,camera)) continue;
		count++;
		if(line->start.y>*maxY) *maxY=line->start.y;
		if(line->end.y>*maxY) *maxY=line->end.y;
	}
	return count;
}

int main(void){
	fprintf(stderr,"=== Winding Tests ===\n");
	{
		float t=0;
		CHECK(!ray_intersect_aabb(v3(0,0,0),v3(0,0,1),v3(-1,-1,-4),v3(1,1,-2),&t),
		      "ray accepted an AABB behind its origin");
		CHECK(ray_intersect_aabb(v3(0,0,0),v3(0,0,1),v3(-1,-1,2),v3(1,1,4),&t) && fabsf(t-2)<0.001f,
		      "ray missed the forward AABB");
		PASS("ray picking rejects objects behind the camera");
	}
	{
		Scene s={0}; int node;
		mat4 matrix=mat4_mul(mat4_translate(v3(3,2,-4)),mat4_rot_xyz(v3(0,37,0)));
		s.activeEditNode=&node; s.activeEditMatrix=matrix;
		scene_add_obj(&s,gen_box(2,1,4),matrix,mat4_rot_xyz(v3(0,37,0)),v3(1,1,1),8,0,1,0);
		mat4 actual; vec3 bmin,bmax;
		scene_get_obj_oriented_bounds(&s,0,&actual,&bmin,&bmax);
		CHECK(fabsf(bmin.x+1)<0.001f && fabsf(bmax.x-1)<0.001f &&
		      fabsf(bmin.y+0.5f)<0.001f && fabsf(bmax.y-0.5f)<0.001f &&
		      fabsf(bmin.z+2)<0.001f && fabsf(bmax.z-2)<0.001f,
		      "oriented bounds expanded into world-axis bounds");
		CHECK(vlen(vsub(mat4_xform_point(actual,v3(0,0,0)),v3(3,2,-4)))<0.001f,
		      "oriented bounds lost the object's matrix");
		PASS("selection bounds remain in object space");
		scene_free(&s);
	}
	{
		Scene s={0}; int node;
		s.activeEditNode=&node; s.activeEditMatrix=mat4_identity();
		scene_add_obj(&s,gen_box(1,1,1),mat4_identity(),mat4_identity(),v3(1,1,1),8,0,1,0);
		s.selectedObj=0; s.editMode=EDIT_W_MOVE;
		vec3 look=v3(0,0,-1);
		float tangent=tanf(60.0f*M_PIf/360.0f);
		vec3 nearCam=v3(0,0,5),farCam=v3(0,0,20);
		vec3 nearRay=vnorm(vsub(v3(5.0f*tangent*0.125f,0,0),nearCam));
		vec3 farRay=vnorm(vsub(v3(20.0f*tangent*0.125f,0,0),farCam));
		int nearHit=gizmo_pick_handle(&s,nearCam,nearRay,look,60.0f);
		int farHit=gizmo_pick_handle(&s,farCam,farRay,look,60.0f);
		CHECK(nearHit==GIZMO_AXIS_X,
		      "near fixed-size gizmo endpoint was not pickable");
		CHECK(farHit==GIZMO_AXIS_X,
		      "far fixed-size gizmo endpoint changed screen position");
		if(nearHit==GIZMO_AXIS_X && farHit==GIZMO_AXIS_X)
			PASS("gizmo screen size and picking stay fixed across camera distance");
		scene_free(&s);
	}

    {
        Mesh m = gen_box(2.0f, 1.0f, 3.0f);
        float vol = mesh_signed_volume(&m);
        float expect = approx_box_vol(2,1,3);
        CHECK(vol > 0, "box signed volume %.4f (expected %.4f)", vol, expect);
        CHECK(fabsf(vol - expect) / expect < 0.01f, "box volume within 1%% of expected");
        mesh_build_edges(&m);
        test_edges_sealed("box sealed", m, 1);
        mesh_free(&m);
    }

    {
        Mesh m = gen_box_inset(2.0f, 1.4f, 0.3f, 0.2f, 0.2f);
        float vol = mesh_signed_volume(&m);
        CHECK(vol > 0, "box inset signed volume positive (%.4f)", vol);
        mesh_build_edges(&m);
        test_edges_sealed("box inset sealed", m, 1);
        mesh_free(&m);
    }

    {
        Mesh m = gen_cylinder(0.5f, 2.0f, 24);
        float vol = mesh_signed_volume(&m);
        float expect = approx_cylinder_vol(0.5, 2.0);
        fprintf(stderr,"  cylinder vol=%.6f expect=%.6f\n", vol, expect);
        CHECK(vol > 0, "cylinder signed volume positive");
        mesh_build_edges(&m);
        test_edges_sealed("cylinder sealed", m, 1);
        mesh_free(&m);
    }

	{
		Mesh m = gen_cylinder_tube(0.5f, 0.8f, 0.08f, 24);
		float vol = mesh_signed_volume(&m);
		float expect=M_PIf*(0.5f*0.5f-0.42f*0.42f)*0.8f;
		CHECK(vol > 0, "cylinder tube signed volume positive (%.4f)", vol);
		CHECK(fabsf(vol-expect)/expect<0.03f,"cylinder tube volume within 3%% of expected");
		mesh_build_edges(&m);
		test_edges_sealed("cylinder tube sealed", m, 1);
		mesh_free(&m);
	}

	{
		float w=1.2f,h=1.2f,d=0.2f,r=0.6f;
		Mesh m=gen_box_hole_cylinder(w,h,d,r,32);
		float vol=mesh_signed_volume(&m);
		float expect=(w*h-M_PIf*r*r)*d;
		CHECK(vol>0,"box cylinder hole signed volume positive (%.4f)",vol);
		CHECK(fabsf(vol-expect)/expect<0.03f,"box cylinder hole volume within 3%% of expected");
		int crossing=0;
		for(int i=0;i<m.ntris;i++){
			Tri t=m.tris[i];
			vec3 a=m.verts[t.a].pos,b=m.verts[t.b].pos,c=m.verts[t.c].pos;
			if(fabsf(fabsf(a.z)-d*0.5f)>1e-4f || fabsf(a.z-b.z)>1e-4f || fabsf(a.z-c.z)>1e-4f) continue;
			float x=(a.x+b.x+c.x)/3.0f,y=(a.y+b.y+c.y)/3.0f;
			if(x*x+y*y<r*r-1e-3f) crossing++;
		}
		CHECK(crossing==0,"box cylinder hole has %d cap triangles crossing the opening",crossing);
		mesh_build_edges(&m);
		test_edges_sealed("box cylinder hole sealed",m,1);
		mesh_free(&m);
	}

    {
        Mesh m = gen_cylinder_like(4, 0.5f, 0.3f, 1.0f, 0);
        float vol = mesh_signed_volume(&m);
        CHECK(vol > 0, "frustum (flat) signed volume positive (%.4f)", vol);
        mesh_build_edges(&m);
        test_edges_sealed("frustum flat sealed", m, 1);
        mesh_free(&m);
    }

    {
        Mesh m = gen_cylinder_like(16, 0.5f, 0.3f, 1.0f, 1);
        float vol = mesh_signed_volume(&m);
        CHECK(vol > 0, "frustum (smooth) signed volume positive (%.4f)", vol);
        mesh_build_edges(&m);
        test_edges_sealed("frustum smooth sealed", m, 1);
        mesh_free(&m);
    }

    {
        Mesh m = gen_prism(0.5f, 1.5f, 6);
        float vol = mesh_signed_volume(&m);
        CHECK(vol > 0, "prism signed volume positive (%.4f)", vol);
        mesh_build_edges(&m);
        test_edges_sealed("prism sealed", m, 1);
        mesh_free(&m);
    }

    {
        Mesh m = gen_cone(0.6f, 0.0f, 1.5f, 24);
        float vol = mesh_signed_volume(&m);
        float expect = M_PIf * 0.6f * 0.6f * 1.5f / 3.0f;
        CHECK(vol > 0, "cone signed volume positive (%.4f, expect %.4f)", vol, expect);
        mesh_build_edges(&m);
        test_edges_sealed("cone sealed", m, 1);
        mesh_free(&m);
    }

    {
        Mesh m = gen_cone(0.5f, 0.15f, 1.0f, 4);
        float vol = mesh_signed_volume(&m);
        CHECK(vol > 0, "pyramid signed volume positive (%.4f)", vol);
        mesh_build_edges(&m);
        test_edges_sealed("pyramid sealed", m, 1);
        mesh_free(&m);
    }

    {
        Mesh m = gen_sphere(1.0f, 16, 24);
        float vol = mesh_signed_volume(&m);
        float expect = approx_sphere_vol(1.0f);
        fprintf(stderr,"  sphere vol=%.6f expect=%.6f\n", vol, expect);
        CHECK(vol > 0, "sphere signed volume positive");
        mesh_build_edges(&m);
        test_edges_sealed("sphere sealed", m, 1);
        mesh_free(&m);
    }

    {
        Mesh m = gen_torus(0.8f, 0.2f, 24, 12);
        float vol = mesh_signed_volume(&m);
        float expect = approx_torus_vol(0.8, 0.2);
        fprintf(stderr,"  torus vol=%.6f expect=%.6f\n", vol, expect);
        CHECK(vol > 0, "torus signed volume positive");
        mesh_build_edges(&m);
        test_edges_sealed("torus sealed", m, 1);
        mesh_free(&m);
    }

    {
        Mesh m = gen_arch(1.6f, 1.9f, 0.12f, 0.0f, 16, 0.0f);
        float vol = mesh_signed_volume(&m);
        CHECK(vol > 0, "arch signed volume positive (%.4f)", vol);
        mesh_build_edges(&m);
        test_edges_sealed("arch sealed", m, 1);
        mesh_free(&m);
    }

	{
		Mesh m=gen_arch(1.6f,1.9f,0.12f,0.12f,16,0.0f);
		float vol=mesh_signed_volume(&m);
		float outer=1.6f*1.1f+0.5f*M_PIf*0.8f*0.8f;
		float inner=1.36f*0.98f+0.5f*M_PIf*0.68f*0.68f;
		float expect=(outer-inner)*0.12f;
		CHECK(vol>0,"arch tube signed volume positive (%.4f)",vol);
		CHECK(fabsf(vol-expect)/expect<0.03f,"arch tube volume within 3%% of expected");
		mesh_build_edges(&m);
		test_edges_sealed("arch tube sealed",m,1);
		mesh_free(&m);
	}

	{
		Mesh m=gen_arch(1.6f,1.9f,0.30f,0.0f,16,0.08f);
		float lo=m.verts[0].pos.z,hi=lo;
		for(int i=1;i<m.nverts;i++){
			if(m.verts[i].pos.z<lo) lo=m.verts[i].pos.z;
			if(m.verts[i].pos.z>hi) hi=m.verts[i].pos.z;
		}
		CHECK(fabsf(lo+0.15f)<1e-4f && fabsf(hi-0.07f)<1e-4f,
			"arch inset bounds are %.4f..%.4f, expected -0.1500..0.0700",lo,hi);
		mesh_build_edges(&m);
		test_edges_sealed("arch inset sealed",m,1);
		mesh_free(&m);
	}

	{
		float w=1.6f,h=1.9f,d=0.12f;
		Mesh m=gen_box_hole_arch(w,h,d,16);
		float vol=mesh_signed_volume(&m);
		CHECK(vol>0,"box arch hole signed volume positive (%.4f)",vol);
		int crossing=0;
		for(int i=0;i<m.ntris;i++){
			Tri t=m.tris[i];
			vec3 a=m.verts[t.a].pos,b=m.verts[t.b].pos,c=m.verts[t.c].pos;
			if(fabsf(fabsf(a.z)-d*0.5f)>1e-4f || fabsf(a.z-b.z)>1e-4f || fabsf(a.z-c.z)>1e-4f) continue;
			float x=(a.x+b.x+c.x)/3.0f,y=(a.y+b.y+c.y)/3.0f;
			if(point_inside_arch(x,y,w,h)) crossing++;
		}
		CHECK(crossing==0,"box arch hole has %d cap triangles crossing the opening",crossing);
		mesh_build_edges(&m);
		test_edges_sealed("box arch hole sealed",m,1);
		mesh_free(&m);
	}

    {
        Mesh m = gen_cylinder_like(4, 0.5f, 0.0f, 1.0f, 0);
        float vol = mesh_signed_volume(&m);
        CHECK(vol > 0, "pyramid apex-top signed volume positive (%.4f)", vol);
        mesh_build_edges(&m);
        test_edges_sealed("pyramid apex-top sealed", m, 1);
        mesh_free(&m);
    }

	fprintf(stderr,"\n=== Prefab Tests ===\n");

	{
		char path[128]; snprintf(path,sizeof(path),"/tmp/simplegl-unknown-%ld.blks",(long)getpid());
		FILE *fixture=fopen(path,"w");
		CHECK(fixture!=NULL,"could not create unknown-element fixture");
		if(fixture){
			fputs("<scene><ambient/><box><mystery/></box></scene>",fixture);
			fclose(fixture);
			FILE *capture=tmpfile();
			int saved=dup(fileno(stderr));
			CHECK(capture!=NULL && saved>=0,"could not capture unknown-element warnings");
			if(capture && saved>=0){
				fflush(stderr); dup2(fileno(capture),fileno(stderr));
				Scene s={0}; load_scene(path,&s); scene_free(&s);
				fflush(stderr); dup2(saved,fileno(stderr)); close(saved);
				rewind(capture);
				char warnings[1024]={0}; fread(warnings,1,sizeof(warnings)-1,capture);
				CHECK(strstr(warnings,"unsupported XML element <ambient> in <scene>")!=NULL,
				      "unknown top-level element did not warn");
				CHECK(strstr(warnings,"unsupported XML element <mystery> in <box>")!=NULL,
				      "unknown shape child did not warn");
				PASS("unknown XML elements report their parent context");
			}
			if(capture) fclose(capture);
			unlink(path);
		}
	}

	{
		Scene s={0};
		CHECK(load_scene("scenes/test_prefab_tint.blks",&s),"prefab tint fixture failed to load");
		CHECK(s.nobjs==15,"prefab tint fixture: got %d objects, expected 15",s.nobjs);
		if(s.nobjs==15){
			CHECK(s.objs[0].editNode==s.objs[1].editNode && s.objs[0].editNode==s.objs[3].editNode,
			      "one prefab instance did not form one selection target");
			CHECK(s.objs[0].editNode!=s.objs[4].editNode,
			      "separate prefab instances shared a selection target");
			vec3 red=v3(0.70f,0.10f,0.08f),blue=v3(0.08f,0.20f,0.70f),paper=v3(0.76f,0.68f,0.50f);
			vec3 green=v3(0.10f,0.32f,0.18f);
			CHECK(color_eq(s.objs[0].color,red) && color_eq(s.objs[2].color,red),
			      "first book covers did not inherit red instance color");
			CHECK(color_eq(s.objs[4].color,blue) && color_eq(s.objs[6].color,blue),
			      "second book covers did not inherit blue instance color");
			CHECK(color_eq(s.objs[1].color,paper) && color_eq(s.objs[5].color,paper),
			      "book pages were incorrectly tinted");
			CHECK(fabsf(mesh_width(&s.objs[4].mesh)/mesh_width(&s.objs[0].mesh)-2.0f)<0.01f,
			      "prefab scale was not applied");
			CHECK(s.objs[8].mesh.verts[0].pos.y>0.20f,
			      "scaled prefab attachment was not transformed");
			CHECK(color_eq(s.objs[9].color,green) && color_eq(s.objs[11].color,green),
			      "nested repair-book covers did not inherit instance color");
			CHECK(color_eq(s.objs[10].color,paper),"nested repair-book pages were incorrectly tinted");
			s.selectedObj=0; s.selectedNode=s.objs[0].editNode;
			CHECK(scene_enter_selected_prefab(&s) && scene_is_prefab_mode(&s),
			      "double-click prefab isolation could not enter the prefab definition");
			CHECK(scene_exit_prefab(&s) && !scene_is_prefab_mode(&s) && s.nobjs==15,
			      "prefab isolation could not return to the scene");
			PASS("selective prefab tint preserves pages and supports scale");
		}
		scene_free(&s);
	}

	{
		Scene s={0};
		CHECK(load_scene("scenes/test_character_dummy.blks",&s),"character dummy fixture failed to load");
		float maxA,maxB;
		int countA=overlay_camera_stats(&s,"ActionA",&maxA);
		int countB=overlay_camera_stats(&s,"ActionB",&maxB);
		CHECK(countA>0 && countB>0,"camera-scoped character lines were not generated");
		CHECK(fabsf(maxA-0.30f)<0.001f,"named character definition height is %.3f, expected 0.300",maxA);
		CHECK(fabsf(maxB-0.50f)<0.001f,"inline character height is %.3f, expected 0.500",maxB);
		CHECK(!strcmp(s.activeCamera,"ActionA"),"first camera was not activated after load");
		scene_select_camera(&s,"ActionB");
		CHECK(!strcmp(s.activeCamera,"ActionB"),"camera selection did not update overlay scope");
		if(countA>0 && countB>0 && fabsf(maxA-0.30f)<0.001f &&
		   fabsf(maxB-0.50f)<0.001f && !strcmp(s.activeCamera,"ActionB"))
			PASS("character dummies support named proportions, inline height, poses, and camera scope");
		scene_free(&s);
	}

	{
		Scene s={0};
		scene_add_obj(&s,gen_box(4,0.1f,4),mat4_translate(v3(0,-0.05f,0)),mat4_identity(),v3(1,1,1),8,0,0,0);
		s.objs[0].sanityFloor=1;
		s.sanityCheckActive=1;
		scene_add_obj(&s,gen_box(1,1,1),mat4_translate(v3(0,0.5f,0)),mat4_identity(),v3(1,1,1),8,0,0,0);
		CHECK(scene_sanity_check(&s),"supported sanity proxy was rejected");
		if(scene_sanity_check(&s)) PASS("scene sanity accepts a grounded proxy");
		scene_add_obj(&s,gen_box(1,1,1),mat4_translate(v3(0,0.5f,0)),mat4_identity(),v3(1,1,1),8,0,0,0);
		CHECK(!scene_sanity_check(&s),"overlapping sanity proxies were not reported");
		if(!scene_sanity_check(&s)) PASS("scene sanity reports intersecting proxies");
		scene_free(&s);
	}

	{
		Scene s={0};
		CHECK(load_scene("scenes/test_prefab_light.blks",&s),"prefab light fixture failed to load");
		CHECK(s.nobjs==6,"prefab light fixture: got %d objects, expected 6",s.nobjs);
		CHECK(s.nlights==2,"prefab light fixture: got %d lights, expected 2",s.nlights);
		if(s.nobjs==6 && s.nlights==2){
			CHECK(s.objs[2].unlit && !s.objs[2].castsShadow,
			      "first bulb is not unlit and shadow-free");
			CHECK(s.objs[5].unlit && !s.objs[5].castsShadow,
			      "scaled bulb is not unlit and shadow-free");
			CHECK(s.lights[0].castsShadow && s.lights[1].castsShadow,
			      "prefab lights do not cast shadows");
			CHECK(fabsf(s.lights[0].pos.x-1.0f)<0.001f &&
			      fabsf(s.lights[0].pos.y-3.36f)<0.001f &&
			      fabsf(s.lights[0].pos.z+2.0f)<0.001f,
			      "first prefab light transform is wrong");
			CHECK(fabsf(s.lights[1].pos.x+1.0f)<0.001f &&
			      fabsf(s.lights[1].pos.y-2.68f)<0.001f &&
			      fabsf(s.lights[1].pos.z-1.0f)<0.001f,
			      "scaled prefab light transform is wrong");
			CHECK(s.lights[0].pos.y<mesh_min_y(&s.objs[1].mesh),
			      "first light is not below the shade lip");
			CHECK(s.lights[1].pos.y<mesh_min_y(&s.objs[4].mesh),
			      "scaled light is not below the shade lip");
			PASS("prefab lights transform with instances and bulbs stay unlit and shadow-free");
		}
		scene_free(&s);
	}

	{
		Scene s={0};
		CHECK(load_scene("scenes/test_wall_negative.blks",&s),"wall negative fixture failed to load");
		CHECK(s.nobjs==4,"wall negative fixture: got %d wall boxes, expected 4",s.nobjs);
		CHECK(!scene_point_covered(&s,v3(1,1.5f,-1.5f)),
		      "prefab negative box did not cut the rotated wall");
		CHECK(scene_point_covered(&s,v3(1,0.5f,-1.5f)),
		      "wall below prefab negative box is missing");
		if(s.nobjs==4 && !scene_point_covered(&s,v3(1,1.5f,-1.5f)) &&
		   scene_point_covered(&s,v3(1,0.5f,-1.5f)))
			PASS("prefab negative box cuts a rotated wall before wall construction");
		scene_free(&s);
	}

	fprintf(stderr,"\n=== Lathe Tests ===\n");

	{
		Shape2D profile={0};
		vec3 pts[]={v3(0.5f,0,0),v3(0.5f,0.2f,0),v3(0.2f,1.5f,0),v3(0.15f,2.0f,0),v3(0.0f,2.5f,0)};
		for(int i=0;i<5;i++){ vec3 p=pts[i]; DA_PUSH(profile.pts,profile.npts,profile.cpts,p); }
		profile.closed=0;
		shape2d_compute_normals(&profile);
		Mesh m=gen_lathe(&profile,24);
		CHECK(m.nverts>0,"lathe open profile produced no vertices");
		CHECK(m.ntris>0,"lathe open profile produced no triangles");
		float vol=mesh_signed_volume(&m);
		CHECK(vol>0,"lathe open profile signed volume positive (%.4f)",vol);
		if(vol>0) PASS("lathe open profile");
		mesh_free(&m);
		shape2d_free(&profile);
	}

	{
		Shape2D profile={0};
		vec3 pts[]={v3(0.3f,0,0),v3(0.5f,0,0),v3(0.5f,0.2f,0),v3(0.2f,1.5f,0),v3(0.12f,2.5f,0),v3(0.0f,2.5f,0)};
		for(int i=0;i<6;i++){ vec3 p=pts[i]; DA_PUSH(profile.pts,profile.npts,profile.cpts,p); }
		profile.closed=0;
		shape2d_compute_normals(&profile);
		Mesh m=gen_lathe(&profile,24);
		float vol=mesh_signed_volume(&m);
		CHECK(vol>0,"lathe bottle profile signed volume positive (%.4f)",vol);
		if(vol>0) PASS("lathe bottle profile");
		mesh_free(&m);
		shape2d_free(&profile);
	}

	{
		Shape2D profile={0};
		vec3 pts[]={v3(0.5f,0,0),v3(0.5f,1.0f,0)};
		for(int i=0;i<2;i++){ vec3 p=pts[i]; DA_PUSH(profile.pts,profile.npts,profile.cpts,p); }
		profile.closed=0;
		shape2d_compute_normals(&profile);
		Mesh m=gen_lathe(&profile,24);
		float vol=mesh_signed_volume(&m);
		float expect=approx_cylinder_vol(0.5f,1.0f);
		CHECK(fabsf(vol-expect)/expect<0.05f,"lathe simple cylinder volume within 5%% of expected (%.4f vs %.4f)",vol,expect);
		if(fabsf(vol-expect)/expect<0.05f) PASS("lathe cylinder");
		mesh_free(&m);
		shape2d_free(&profile);
	}

	fprintf(stderr,"\n=== Loft Tests ===\n");

	{
		LoftPath path={0};
		DA_PUSH(path.pts,path.npts,path.cpts,v3(0,0,0));
		DA_PUSH(path.pts,path.npts,path.cpts,v3(0,1,0));
		DA_PUSH(path.pts,path.npts,path.cpts,v3(0,2,0));
		Shape2D cross={0};
		for(int j=0;j<16;j++){
			float v=(float)j/16*2.0f*M_PIf;
			vec3 p=v3(cosf(v)*0.3f,sinf(v)*0.3f,0);
			DA_PUSH(cross.pts,cross.npts,cross.cpts,p);
		}
		cross.closed=1;
		shape2d_compute_normals(&cross);
		Mesh m=gen_loft(&path,&cross,0);
		CHECK(m.nverts>0,"loft straight path produced no vertices");
		CHECK(m.ntris>0,"loft straight path produced no triangles");
		float vol=mesh_signed_volume(&m);
		float expect=M_PIf*0.3f*0.3f*2.0f;
		CHECK(fabsf(vol-expect)/expect<0.05f,"loft straight cylinder volume within 5%% of expected (%.4f vs %.4f)",vol,expect);
		mesh_build_edges(&m);
		test_edges_sealed("loft straight cylinder sealed",m,1);
		mesh_free(&m);
		free(path.pts);
		shape2d_free(&cross);
	}

	{
		float R=0.8f, r=0.2f;
		int majorSeg=24, minorSeg=12;
		Mesh m=gen_torus(R,r,majorSeg,minorSeg);
		float vol=mesh_signed_volume(&m);
		float expect=approx_torus_vol(R,r);
		CHECK(vol>0,"torus via loft signed volume positive");
		CHECK(fabsf(vol-expect)/expect<0.06f,"torus via loft volume within 6%% of expected (%.4f vs %.4f)",vol,expect);
		mesh_build_edges(&m);
		test_edges_sealed("torus via loft sealed",m,1);
		mesh_free(&m);
	}

	{
		float r=0.15f;
		LoftPath path={0};
		float pathR=1.0f;
		int pathSegs=24;
		for(int i=0;i<pathSegs;i++){
			float u=(float)i/pathSegs*2.0f*M_PIf;
			vec3 p=v3(pathR*cosf(u),0,pathR*sinf(u));
			DA_PUSH(path.pts,path.npts,path.cpts,p);
		}
		Shape2D cross={0};
		int crossSegs=8;
		for(int j=0;j<crossSegs;j++){
			float v=(float)j/crossSegs*2.0f*M_PIf;
			vec3 p=v3(cosf(v)*r,sinf(v)*r,0);
			DA_PUSH(cross.pts,cross.npts,cross.cpts,p);
		}
		cross.closed=1;
		shape2d_compute_normals(&cross);
		Mesh m=gen_loft(&path,&cross,1);
		float vol=mesh_signed_volume(&m);
		float expect=approx_torus_vol(pathR,r);
		CHECK(vol>0,"gen_loft torus signed volume positive (%.4f vs %.4f)",vol,expect);
		mesh_build_edges(&m);
		test_edges_sealed("gen_loft torus sealed",m,1);
		mesh_free(&m);
		free(path.pts);
		shape2d_free(&cross);
	}

	{
		Shape2D circle={0};
		for(int j=0;j<8;j++){
			float v=(float)j/8*2.0f*M_PIf;
			vec3 p=v3(cosf(v),sinf(v),0);
			DA_PUSH(circle.pts,circle.npts,circle.cpts,p);
		}
		circle.closed=1;
		shape2d_compute_normals(&circle);
		CHECK(circle.nrm!=NULL,"shape normals array allocated");
		int all_unit=1;
		for(int j=0;j<circle.npts;j++){
			if(fabsf(vlen(circle.nrm[j])-1.0f)>0.01f){ all_unit=0; break; }
		}
		CHECK(all_unit,"shape circle normals are unit length");
		shape2d_free(&circle);
	}

	fprintf(stderr,"\n=== Capsule Tests ===\n");

	{
		Mesh m=gen_capsule(0.3f,1.0f,12,24);
		float vol=mesh_signed_volume(&m);
		CHECK(vol>0,"capsule signed volume positive (%.4f)",vol);
		mesh_build_edges(&m);
		test_edges_sealed("capsule sealed",m,1);
		mesh_free(&m);
	}

	fprintf(stderr,"\n=== Modifier Tests ===\n");

	{
		Mesh m=gen_box(1.0f,0.2f,1.0f);
		int ov=m.nverts;
		mesh_apply_extrude(&m,0.3f,'y');
		CHECK(m.nverts>ov,"extrude added vertices");
		PASS("extrude modifier");
		mesh_free(&m);
	}

	{
		Mesh m=gen_sphere(0.5f,8,12);
		int ov=m.nverts;
		mesh_apply_mirror(&m,'x',0.001f);
		CHECK(m.nverts==ov*2,"mirror doubled vertex count");
		float vol=mesh_signed_volume(&m);
		CHECK(vol>0,"mirrored sphere signed volume positive");
		mesh_build_edges(&m);
		test_edges_sealed("mirrored half sphere sealed",m,1);
		mesh_free(&m);
	}

	{
		Mesh m=gen_sphere(0.5f,8,12);
		mesh_apply_noise(&m,0.05f,42);
		float vol=mesh_signed_volume(&m);
		CHECK(vol>0,"noisy sphere still positive volume (%.4f)",vol);
		PASS("noise modifier");
		mesh_free(&m);
	}

	{
		Mesh m=gen_box(1.0f,0.1f,1.0f);
		int ov=m.nverts;
		mesh_apply_shell(&m,0.05f);
		CHECK(m.nverts>ov,"shell added vertices to box");
		PASS("shell modifier");
		mesh_free(&m);
	}

	fprintf(stderr,"\n=== Shadow Volume Tests ===\n");
	{
		Scene s={0}; int nodeA,nodeB;
		Light light={0}; light.dir=vnorm(v3(1,-1,0)); light.castsShadow=1; light.isDirectional=1;
		DA_PUSH(s.lights,s.nlights,s.clights,light);
		s.svols=calloc(1,sizeof(ShadowVolume));
		s.activeEditNode=&nodeA; s.activeEditMatrix=mat4_identity();
		scene_add_obj(&s,gen_box(1,1,1),mat4_identity(),mat4_identity(),v3(1,1,1),8,1,1,0);
		s.activeEditNode=&nodeB; s.activeEditMatrix=mat4_translate(v3(3,0,0));
		scene_add_obj(&s,gen_box(1,1,1),s.activeEditMatrix,mat4_identity(),v3(1,1,1),8,1,1,0);
		scene_build_all_shadow_volumes(&s);
		ShadowVertex *unchanged=s.objs[1].shadowParts[0].verts;
		int unchangedCount=s.objs[1].shadowParts[0].nverts;
		mesh_transform(&s.objs[0].mesh,mat4_translate(v3(0,1,0)),mat4_identity());
		mesh_compute_face_normals(&s.objs[0].mesh); mesh_build_edges(&s.objs[0].mesh);
		scene_rebuild_node_shadow_volumes(&s,&nodeA);
		CHECK(s.objs[1].shadowParts[0].verts==unchanged && s.objs[1].shadowParts[0].nverts==unchangedCount,
		      "partial shadow update rebuilt an unrelated object");
		PASS("shadow updates are limited to the edited target subtree");
		scene_free(&s);
	}

	{
		Light sun={.dir=vnorm(v3(0.75f,-0.55f,0.35f)),.isDirectional=1};
		vec3 toSource=light_to_source(&sun,v3(4,2,-3));
		CHECK(toSource.x < 0 && toSource.y > 0 && toSource.z < 0,
		      "sun direction does not point back toward source");
		if(toSource.x < 0 && toSource.y > 0 && toSource.z < 0)
			PASS("sun travel direction converts to source direction");
	}

	{
		Scene s={0};
		s.lights=calloc(1,sizeof(Light)); s.nlights=s.clights=1;
		s.lights[0]=(Light){.pos=v3(0,3,0),.castsShadow=1};
		s.svols=calloc(1,sizeof(ShadowVolume));
		scene_add_obj(&s,gen_box(2,2,0.2f),mat4_identity(),mat4_identity(),v3(1,1,1),8,1,0,0);
		scene_build_all_shadow_volumes(&s);
		CHECK(!s.objs[0].renderable && s.objs[0].castsShadow,
		      "shadow-only object flags were not preserved");
		CHECK(s.svols[0].nverts > 0,"non-renderable object missing from shadow volume");
		if(!s.objs[0].renderable && s.objs[0].castsShadow && s.svols[0].nverts > 0)
			PASS("non-renderable object still casts shadows");
		scene_free(&s);
	}

    {
        Mesh m = gen_box(1.0f, 1.0f, 1.0f);
        mesh_compute_face_normals(&m);
        mesh_build_edges(&m);
        vec3 lightPos = v3(0, 5, 0);
        ShadowVolume sv;
        build_shadow_volume(&m, lightPos, v3(0,0,0), 0, &sv);

        int silEdges = 0;
        char *facing = malloc((size_t)m.ntris);
        for(int i = 0; i < m.ntris; i++){
            vec3 p = m.verts[m.tris[i].a].pos;
            facing[i] = vdot(m.triN[i], vsub(lightPos, p)) > 0.0f;
        }
        for(int i = 0; i < m.nedges; i++){
            Edge *e = &m.edges[i];
            int f0 = facing[e->t0], f1 = (e->t1 >= 0) ? facing[e->t1] : 0;
            if((e->t1 < 0) ? f0 : (f0 != f1)) silEdges++;
        }
        free(facing);

#ifdef USE_ZPASS
        int expectedVerts = silEdges*6;
#else
        int expectedVerts = silEdges*6 + m.ntris*3;
#endif
        CHECK(sv.nverts == expectedVerts,
              "box shadow vol verts: got %d, expected %d (sides + caps)",
              sv.nverts, expectedVerts);
        if(sv.nverts == expectedVerts) PASS("box shadow volume has expected geometry");

        int points=0, directions=0;
        for(int i=0;i<sv.nverts;i++){
            if(sv.verts[i].w==1.0f) points++;
            if(sv.verts[i].w==0.0f) directions++;
        }
#ifdef USE_ZPASS
        CHECK(points>0 && directions>0, "z-pass shadow volume lacks finite or infinite vertices");
        if(points>0 && directions>0) PASS("z-pass side quads use homogeneous infinity");
#else
        CHECK(points>0 && directions>0, "z-fail shadow volume lacks finite or infinite vertices");
        if(points>0 && directions>0) PASS("z-fail shadow volume uses homogeneous infinity");
        CHECK(shadow_volume_closed(&sv), "shadow volume is not consistently oriented and closed");
        if(shadow_volume_closed(&sv)) PASS("shadow volume is consistently oriented and closed");
#endif

        CHECK(sv.nverts % 3 == 0, "shadow volume vertex count divisible by 3");
        if(sv.nverts % 3 == 0) PASS("shadow volume divisible by 3");

        free(sv.verts);
        mesh_free(&m);
    }

    {
        Mesh m = gen_sphere(0.5f, 12, 16);
        mesh_compute_face_normals(&m);
        mesh_build_edges(&m);
        vec3 lightPos = v3(0, 3, 0);
        ShadowVolume sv;
        build_shadow_volume(&m, lightPos, v3(0,0,0), 0, &sv);
        CHECK(sv.nverts > 0, "sphere shadow volume non-empty");
        CHECK(sv.nverts % 3 == 0, "sphere shadow volume divisible by 3");
        if(sv.nverts > 0 && sv.nverts % 3 == 0)
            PASS("sphere shadow volume well-formed");
        free(sv.verts);
        mesh_free(&m);
    }

    {
        Mesh m = gen_box(4.0f, 2.8f, 0.2f);
        mesh_compute_face_normals(&m);
        mesh_build_edges(&m);
        vec3 lightPos = v3(0.5f, 2.5f, -1.5f);
        ShadowVolume sv;
        build_shadow_volume(&m, lightPos, v3(0,0,0), 0, &sv);
        CHECK(sv.nverts > 0, "wall-sized box shadow volume non-empty");
        CHECK(sv.nverts % 3 == 0, "wall-sized box shadow volume divisible by 3");
        if(sv.nverts > 0 && sv.nverts % 3 == 0)
            PASS("wall-sized box shadow volume well-formed");
        free(sv.verts);
        mesh_free(&m);
    }

    if(failures) fprintf(stderr,"\n=== FAILED: %d test(s) ===\n", failures);
    else fprintf(stderr,"\n=== ALL TESTS PASSED ===\n");
    return failures;
}
