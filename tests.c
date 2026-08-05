#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mesh.h"
#include "math.h"
#include "shadow.h"
#include "scene.h"

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

int main(void){
    fprintf(stderr,"=== Winding Tests ===\n");

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
        Mesh m = gen_cylinder_like(4, 0.5f, 0.0f, 1.0f, 0);
        float vol = mesh_signed_volume(&m);
        CHECK(vol > 0, "pyramid apex-top signed volume positive (%.4f)", vol);
        mesh_build_edges(&m);
        test_edges_sealed("pyramid apex-top sealed", m, 1);
        mesh_free(&m);
    }

    fprintf(stderr,"\n=== Shadow Volume Tests ===\n");

    {
        Mesh m = gen_box(1.0f, 1.0f, 1.0f);
        mesh_compute_face_normals(&m);
        mesh_build_edges(&m);
        vec3 lightPos = v3(0, 5, 0);
        ShadowVolume sv;
        build_shadow_volume(&m, lightPos, &sv);

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
        build_shadow_volume(&m, lightPos, &sv);
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
        build_shadow_volume(&m, lightPos, &sv);
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
