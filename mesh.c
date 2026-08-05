#include "simplegl.h"

void mesh_free(Mesh *m){
    free(m->verts); free(m->tris); free(m->edges); free(m->triN);
    memset(m,0,sizeof(*m));
}
int mesh_add_vert(Mesh *m, vec3 p, vec3 n){
    Vertex v={p,n}; DA_PUSH(m->verts,m->nverts,m->cverts,v); return m->nverts-1;
}
void mesh_add_tri(Mesh *m,int a,int b,int c){
    Tri t={a,b,c}; DA_PUSH(m->tris,m->ntris,m->ctris,t);
}
void mesh_transform(Mesh *m, mat4 posM, mat4 rotM){
    for(int i=0;i<m->nverts;i++){
        m->verts[i].pos = mat4_xform_point(posM, m->verts[i].pos);
        m->verts[i].nrm = vnorm(mat4_xform_dir(rotM, m->verts[i].nrm));
    }
}
void mesh_compute_face_normals(Mesh *m){
    free(m->triN); m->triN = malloc(sizeof(vec3)*(size_t)m->ntris);
    for(int i=0;i<m->ntris;i++){
        Tri t=m->tris[i];
        vec3 a=m->verts[t.a].pos, b=m->verts[t.b].pos, c=m->verts[t.c].pos;
        m->triN[i] = vnorm(vcross(vsub(b,a),vsub(c,a)));
    }
}
void mesh_build_edges(Mesh *m){
    int *weld = malloc(sizeof(int)*(size_t)m->nverts);
    for(int i=0;i<m->nverts;i++){
        weld[i]=i;
        for(int j=0;j<i;j++){
            if(vlen(vsub(m->verts[i].pos,m->verts[j].pos)) < 1e-4f){ weld[i]=weld[j]; break; }
        }
    }
    free(m->edges); m->edges=NULL; m->nedges=m->cedges=0;
    for(int i=0;i<m->ntris;i++){
        Tri t=m->tris[i];
        int pairs[3][2]={{t.a,t.b},{t.b,t.c},{t.c,t.a}};
        for(int e=0;e<3;e++){
            int v0=pairs[e][0], v1=pairs[e][1];
            int w0=weld[v0], w1=weld[v1];
            int found=-1;
            for(int k=0;k<m->nedges;k++){
                Edge *ed=&m->edges[k];
                if(ed->t1<0){
                    vec3 ep0=ed->p0, ep1=ed->p1;
                    int ew0 = (vlen(vsub(ep0,m->verts[v1].pos))<1e-4f);
                    int ew1 = (vlen(vsub(ep1,m->verts[v0].pos))<1e-4f);
                    (void)w0; (void)w1;
                    if(ew0 && ew1){ found=k; break; }
                }
            }
            if(found>=0){ m->edges[found].t1=i; }
            else{
                Edge ne={ m->verts[v0].pos, m->verts[v1].pos, i, -1 };
                DA_PUSH(m->edges,m->nedges,m->cedges,ne);
            }
        }
    }
    free(weld);
}
float mesh_signed_volume(Mesh *m){
    float vol = 0.0f;
    for(int i=0;i<m->ntris;i++){
        Tri t=m->tris[i];
        vec3 a=m->verts[t.a].pos, b=m->verts[t.b].pos, c=m->verts[t.c].pos;
        vol += vdot(vcross(a,b),c);
    }
    return vol / 6.0f;
}
void mesh_flip_winding(Mesh *m){
    for(int i=0;i<m->ntris;i++){ int t=m->tris[i].b; m->tris[i].b=m->tris[i].c; m->tris[i].c=t; }
    for(int i=0;i<m->nedges;i++){ vec3 p=m->edges[i].p0; m->edges[i].p0=m->edges[i].p1; m->edges[i].p1=p; }
}

/* ------------------------------------------------------- primitive gens */
static void add_quad(Mesh *m, vec3 a,vec3 b,vec3 c,vec3 d, vec3 n){
    int ia=mesh_add_vert(m,a,n), ib=mesh_add_vert(m,b,n),
        ic=mesh_add_vert(m,c,n), id=mesh_add_vert(m,d,n);
    mesh_add_tri(m,ia,ic,ib); mesh_add_tri(m,ia,id,ic);
}

Mesh gen_box(float sx,float sy,float sz){
    Mesh m={0};
    float x=sx*0.5f,y=sy*0.5f,z=sz*0.5f;
    vec3 p[8]={ v3(-x,-y,-z),v3(x,-y,-z),v3(x,y,-z),v3(-x,y,-z),
                v3(-x,-y, z),v3(x,-y, z),v3(x,y, z),v3(-x,y, z) };
    add_quad(&m,p[0],p[1],p[2],p[3], v3(0,0,-1));
    add_quad(&m,p[5],p[4],p[7],p[6], v3(0,0, 1));
    add_quad(&m,p[4],p[0],p[3],p[7], v3(-1,0,0));
    add_quad(&m,p[1],p[5],p[6],p[2], v3( 1,0,0));
    add_quad(&m,p[4],p[5],p[1],p[0], v3(0,-1,0));
    add_quad(&m,p[3],p[2],p[6],p[7], v3(0, 1,0));
    return m;
}

Mesh gen_cylinder_like(int sides,float rBot,float rTop,float height,int smooth){
    Mesh m={0}; if(sides<3) sides=3;
    float hy=height*0.5f;
    for(int i=0;i<sides;i++){
        float a0=(float)i/sides*2.0f*M_PIf, a1=(float)(i+1)/sides*2.0f*M_PIf;
        vec3 b0=v3(cosf(a0)*rBot,-hy,sinf(a0)*rBot), b1=v3(cosf(a1)*rBot,-hy,sinf(a1)*rBot);
        vec3 t0=v3(cosf(a0)*rTop, hy,sinf(a0)*rTop), t1=v3(cosf(a1)*rTop, hy,sinf(a1)*rTop);
        vec3 flatN=vnorm(vcross(vsub(t0,b0),vsub(b1,b0)));
        if(smooth){
            vec3 n0=vnorm(v3(cosf(a0),0,sinf(a0))), n1=vnorm(v3(cosf(a1),0,sinf(a1)));
            int ib0=mesh_add_vert(&m,b0,n0), ib1=mesh_add_vert(&m,b1,n1);
            int it0=mesh_add_vert(&m,t0,n0), it1=mesh_add_vert(&m,t1,n1);
            if(rBot>1e-6f) mesh_add_tri(&m,ib0,it1,ib1);
            if(rTop>1e-6f || rBot>1e-6f) mesh_add_tri(&m,ib0,it0,it1);
        } else {
            add_quad(&m,b0,b1,t1,t0,flatN);
        }
    }
    if(rBot>1e-6f){
        vec3 center=v3(0,-hy,0), n=v3(0,-1,0);
        for(int i=0;i<sides;i++){
            float a0=(float)i/sides*2.0f*M_PIf, a1=(float)(i+1)/sides*2.0f*M_PIf;
            vec3 b0=v3(cosf(a0)*rBot,-hy,sinf(a0)*rBot), b1=v3(cosf(a1)*rBot,-hy,sinf(a1)*rBot);
            int ic=mesh_add_vert(&m,center,n), i0=mesh_add_vert(&m,b0,n), i1=mesh_add_vert(&m,b1,n);
            mesh_add_tri(&m,ic,i0,i1);
        }
    }
    if(rTop>1e-6f){
        vec3 center=v3(0,hy,0), n=v3(0,1,0);
        for(int i=0;i<sides;i++){
            float a0=(float)i/sides*2.0f*M_PIf, a1=(float)(i+1)/sides*2.0f*M_PIf;
            vec3 t0=v3(cosf(a0)*rTop,hy,sinf(a0)*rTop), t1=v3(cosf(a1)*rTop,hy,sinf(a1)*rTop);
            int ic=mesh_add_vert(&m,center,n), i0=mesh_add_vert(&m,t1,n), i1=mesh_add_vert(&m,t0,n);
            mesh_add_tri(&m,ic,i0,i1);
        }
    }
    return m;
}
Mesh gen_cylinder(float r,float h,int sides){ return gen_cylinder_like(sides<=0?24:sides,r,r,h,1); }
Mesh gen_prism(float r,float h,int sides){ return gen_cylinder_like(sides<3?6:sides,r,r,h,0); }
Mesh gen_cone(float rBase,float rTop,float h,int sides){
    return gen_cylinder_like(sides<3?4:sides, rBase, rTop, h, sides>=16);
}

Mesh gen_sphere(float r,int rings,int slices){
    Mesh m={0}; if(rings<3) rings=12; if(slices<3) slices=16;
    for(int i=0;i<=rings;i++){
        float v=(float)i/rings, phi=v*M_PIf;
        for(int j=0;j<=slices;j++){
            float u=(float)j/slices, th=u*2.0f*M_PIf;
            vec3 n=v3(sinf(phi)*cosf(th), cosf(phi), sinf(phi)*sinf(th));
            mesh_add_vert(&m, vscale(n,r), n);
        }
    }
    int stride=slices+1;
    for(int i=0;i<rings;i++) for(int j=0;j<slices;j++){
        int a=i*stride+j, b=a+1, c=(i+1)*stride+j, d=c+1;
        mesh_add_tri(&m,a,b,d); mesh_add_tri(&m,a,d,c);
    }
    return m;
}

Mesh gen_torus(float R,float r,int majorSeg,int minorSeg){
	Mesh m={0}; if(majorSeg<3) majorSeg=24; if(minorSeg<3) minorSeg=12;
	for(int i=0;i<=majorSeg;i++){
		float u=(float)i/majorSeg*2.0f*M_PIf;
		for(int j=0;j<=minorSeg;j++){
			float v=(float)j/minorSeg*2.0f*M_PIf;
			vec3 n=v3(cosf(u)*cosf(v), sinf(v), sinf(u)*cosf(v));
			vec3 p=v3((R+r*cosf(v))*cosf(u), r*sinf(v), (R+r*cosf(v))*sinf(u));
			mesh_add_vert(&m,p,n);
		}
	}
	int stride=minorSeg+1;
	for(int i=0;i<majorSeg;i++) for(int j=0;j<minorSeg;j++){
		int a=i*stride+j, b=a+1, c=(i+1)*stride+j, d=c+1;
		mesh_add_tri(&m,a,b,d); mesh_add_tri(&m,a,d,c);
	}
	return m;
}

/* ---------------------------------------------------------- modifiers ------ */

static void mesh_find_bounds(Mesh *m, char axis, float *minV, float *maxV){
	*minV=1e9f; *maxV=-1e9f;
	for(int i=0;i<m->nverts;i++){
		float v = axis=='x' ? m->verts[i].pos.x
		       : axis=='y' ? m->verts[i].pos.y : m->verts[i].pos.z;
		if(v<*minV) *minV=v; if(v>*maxV) *maxV=v;
	}
	if(*maxV-*minV < 1e-6f){ *minV=-0.5f; *maxV=0.5f; }
}

void mesh_apply_taper(Mesh *m, float amount, float curvature, char axis){
	float mn,mx; mesh_find_bounds(m,axis,&mn,&mx);
	float range=mx-mn;
	for(int i=0;i<m->nverts;i++){
		float *pa, *pb, *pc;
		switch(axis){
			case 'x': pa=&m->verts[i].pos.x; pb=&m->verts[i].pos.y; pc=&m->verts[i].pos.z; break;
			case 'y': pa=&m->verts[i].pos.y; pb=&m->verts[i].pos.x; pc=&m->verts[i].pos.z; break;
			default:  pa=&m->verts[i].pos.z; pb=&m->verts[i].pos.x; pc=&m->verts[i].pos.y; break;
		}
		float t=(*pa-mn)/range;
		float s=1.0f+amount*powf(2.0f*t-1.0f, curvature>0?curvature:1.0f);
		*pb*=s; *pc*=s;
	}
}

void mesh_apply_twist(Mesh *m, float angle_deg, char axis){
	float mn,mx; mesh_find_bounds(m,axis,&mn,&mx);
	float range=mx-mn;
	float rad=angle_deg*M_PIf/180.0f;
	for(int i=0;i<m->nverts;i++){
		float *pa, *pb, *pc, *nb, *nc;
		switch(axis){
			case 'x': pa=&m->verts[i].pos.x; pb=&m->verts[i].pos.y; pc=&m->verts[i].pos.z;
			          nb=&m->verts[i].nrm.y; nc=&m->verts[i].nrm.z; break;
			case 'y': pa=&m->verts[i].pos.y; pb=&m->verts[i].pos.x; pc=&m->verts[i].pos.z;
			          nb=&m->verts[i].nrm.x; nc=&m->verts[i].nrm.z; break;
			default:  pa=&m->verts[i].pos.z; pb=&m->verts[i].pos.x; pc=&m->verts[i].pos.y;
			          nb=&m->verts[i].nrm.x; nc=&m->verts[i].nrm.y; break;
		}
		float t=(*pa-mn)/range;
		float a=rad*t, ca=cosf(a), sa=sinf(a);
		float b=*pb, c=*pc;
		*pb=b*ca - c*sa; *pc=b*sa + c*ca;
		float n_b=*nb, n_c=*nc;
		*nb=n_b*ca - n_c*sa; *nc=n_b*sa + n_c*ca;
	}
}

void mesh_apply_bend(Mesh *m, float angle_deg, char axis){
	float mn,mx; mesh_find_bounds(m,axis,&mn,&mx);
	float range=mx-mn;
	float alpha=angle_deg*M_PIf/180.0f;
	float R=range/alpha;
	if(fabsf(alpha)<1e-6f) return;
	for(int i=0;i<m->nverts;i++){
		float *pa, *pb, *na, *nb;
		switch(axis){
			case 'x': pa=&m->verts[i].pos.x; pb=&m->verts[i].pos.y;
			          na=&m->verts[i].nrm.x; nb=&m->verts[i].nrm.y; break;
			case 'y': pa=&m->verts[i].pos.y; pb=&m->verts[i].pos.x;
			          na=&m->verts[i].nrm.y; nb=&m->verts[i].nrm.x; break;
			default:  pa=&m->verts[i].pos.z; pb=&m->verts[i].pos.x;
			          na=&m->verts[i].nrm.z; nb=&m->verts[i].nrm.x; break;
		}
		float along=*pa-mn;
		float theta=along/R;
		float cr=cosf(theta), sr=sinf(theta);
		float radial=*pb;
		*pb = (R+radial)*sr;
		*pa = mn + R - (R+radial)*cr;
		float n_al=*na, n_rd=*nb;
		*na = n_rd*sr + n_al*cr;
		*nb = n_rd*cr - n_al*sr;
	}
}

void mesh_apply_stretch(Mesh *m, float amount, float amplify, char axis){
	float mn,mx; mesh_find_bounds(m,axis,&mn,&mx);
	float range=mx-mn;
	for(int i=0;i<m->nverts;i++){
		float *pa, *pb, *pc;
		switch(axis){
			case 'x': pa=&m->verts[i].pos.x; pb=&m->verts[i].pos.y; pc=&m->verts[i].pos.z; break;
			case 'y': pa=&m->verts[i].pos.y; pb=&m->verts[i].pos.x; pc=&m->verts[i].pos.z; break;
			default:  pa=&m->verts[i].pos.z; pb=&m->verts[i].pos.x; pc=&m->verts[i].pos.y; break;
		}
		float t=(*pa-mn)/range;
		float s=amount*(t*t - t);
		*pb*=1.0f-s*amplify; *pc*=1.0f-s*amplify;
		*pa = mn + t*range*(1.0f+s);
	}
}

void mesh_apply_skew(Mesh *m, float amount, char axis){
	float mn,mx; mesh_find_bounds(m,axis,&mn,&mx);
	float range=mx-mn;
	for(int i=0;i<m->nverts;i++){
		float *pa, *pb, *pc;
		switch(axis){
			case 'x': pa=&m->verts[i].pos.x; pb=&m->verts[i].pos.y; pc=&m->verts[i].pos.z; break;
			case 'y': pa=&m->verts[i].pos.y; pb=&m->verts[i].pos.x; pc=&m->verts[i].pos.z; break;
			default:  pa=&m->verts[i].pos.z; pb=&m->verts[i].pos.x; pc=&m->verts[i].pos.y; break;
		}
		float t=(*pa-mn)/range;
		*pb+=amount*t; *pc+=amount*t;
	}
}

void mesh_apply_array(Mesh *m, int count, vec3 off, vec3 rot){
	if(count<2) return;
	int ov=m->nverts, ot=m->ntris;
	for(int c=1;c<count;c++){
		mat4 step=mat4_mul(mat4_translate(v3(off.x*c,off.y*c,off.z*c)),
			mat4_rot_xyz(v3(rot.x*c,rot.y*c,rot.z*c)));
		for(int v=0;v<ov;v++){
			vec3 p=mat4_xform_point(step, m->verts[v].pos);
			vec3 n=mat4_xform_dir(mat4_rot_xyz(v3(rot.x*c,rot.y*c,rot.z*c)), m->verts[v].nrm);
			mesh_add_vert(m,p,n);
		}
		for(int t=0;t<ot;t++)
			mesh_add_tri(m,m->tris[t].a+c*ov,m->tris[t].b+c*ov,m->tris[t].c+c*ov);
	}
}
