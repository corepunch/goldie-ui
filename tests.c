#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mesh.h"
#include "math.h"

static int failures = 0;
#define CHECK(cond, fmt, ...) do{ if(!(cond)){ fprintf(stderr,"FAIL: " fmt "\n", ##__VA_ARGS__); failures++; } }while(0)
#define PASS(msg) fprintf(stderr,"  OK  %s\n", msg)

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

    if(failures) fprintf(stderr,"\n=== FAILED: %d test(s) ===\n", failures);
    else fprintf(stderr,"\n=== ALL TESTS PASSED ===\n");
    return failures;
}
