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
static void add_quad_n4(Mesh *m, vec3 a,vec3 na, vec3 b,vec3 nb, vec3 c,vec3 nc, vec3 d,vec3 nd){
    int ia=mesh_add_vert(m,a,na), ib=mesh_add_vert(m,b,nb),
        ic=mesh_add_vert(m,c,nc), id=mesh_add_vert(m,d,nd);
    mesh_add_tri(m,ia,ic,ib); mesh_add_tri(m,ia,id,ic);
}

static void mesh_append(Mesh *dst, Mesh src){
	int base=dst->nverts;
	for(int i=0;i<src.nverts;i++) mesh_add_vert(dst,src.verts[i].pos,src.verts[i].nrm);
	for(int i=0;i<src.ntris;i++) mesh_add_tri(dst,src.tris[i].a+base,src.tris[i].b+base,src.tris[i].c+base);
	mesh_free(&src);
}

static void mesh_append_xform(Mesh *dst, Mesh src, mat4 M, mat4 R){
	mesh_transform(&src,M,R);
	mesh_append(dst,src);
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

Mesh gen_box_inset(float sx,float sy,float sz,float insetX,float insetY){
	Mesh m={0};
	float halfX=sx*0.5f, halfY=sy*0.5f;
	float innerX=halfX-insetX, innerY=halfY-insetY;
	if(insetX<=1e-6f || insetY<=1e-6f || innerX<=1e-6f || innerY<=1e-6f) return gen_box(sx,sy,sz);
	mesh_append_xform(&m,gen_box(insetX*2.0f,sy,sz),mat4_translate(v3(-halfX+insetX,0,0)),mat4_identity());
	mesh_append_xform(&m,gen_box(insetX*2.0f,sy,sz),mat4_translate(v3(halfX-insetX,0,0)),mat4_identity());
	mesh_append_xform(&m,gen_box(innerX*2.0f,insetY*2.0f,sz),mat4_translate(v3(0,halfY-insetY,0)),mat4_identity());
	mesh_append_xform(&m,gen_box(innerX*2.0f,insetY*2.0f,sz),mat4_translate(v3(0,-halfY+insetY,0)),mat4_identity());
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
Mesh gen_cylinder_tube(float r,float h,float wall,int sides){
	Mesh m={0};
	if(sides<3) sides=24;
	if(wall<=1e-6f || wall>=r-1e-6f) return gen_cylinder(r,h,sides);
	float inner=r-wall, hy=h*0.5f;
	for(int i=0;i<sides;i++){
		float a0=(float)i/sides*2.0f*M_PIf, a1=(float)(i+1)/sides*2.0f*M_PIf;
		vec3 o0b=v3(cosf(a0)*r,-hy,sinf(a0)*r), o1b=v3(cosf(a1)*r,-hy,sinf(a1)*r);
		vec3 o0t=v3(cosf(a0)*r, hy,sinf(a0)*r), o1t=v3(cosf(a1)*r, hy,sinf(a1)*r);
		vec3 i0b=v3(cosf(a0)*inner,-hy,sinf(a0)*inner), i1b=v3(cosf(a1)*inner,-hy,sinf(a1)*inner);
		vec3 i0t=v3(cosf(a0)*inner, hy,sinf(a0)*inner), i1t=v3(cosf(a1)*inner, hy,sinf(a1)*inner);
		vec3 on0=vnorm(v3(cosf(a0),0,sinf(a0))), on1=vnorm(v3(cosf(a1),0,sinf(a1)));
		vec3 in0=vscale(on0,-1.0f), in1=vscale(on1,-1.0f);
		int io0b=mesh_add_vert(&m,o0b,on0), io1b=mesh_add_vert(&m,o1b,on1);
		int io0t=mesh_add_vert(&m,o0t,on0), io1t=mesh_add_vert(&m,o1t,on1);
		mesh_add_tri(&m,io0b,io1t,io1b);
		mesh_add_tri(&m,io0b,io0t,io1t);
		int ii0b=mesh_add_vert(&m,i0b,in0), ii1b=mesh_add_vert(&m,i1b,in1);
		int ii0t=mesh_add_vert(&m,i0t,in0), ii1t=mesh_add_vert(&m,i1t,in1);
		mesh_add_tri(&m,ii0b,ii1b,ii1t);
		mesh_add_tri(&m,ii0b,ii1t,ii0t);
		add_quad(&m,o0t,o1t,i1t,i0t,v3(0,1,0));
		add_quad(&m,o1b,o0b,i0b,i1b,v3(0,-1,0));
	}
	return m;
}
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

/* Roman arch: semicircle (radius=width/2) on rectangular stem, centered at origin.
 * wall>0: hollow frame.  inset: recess front face in +Z. */
Mesh gen_arch(float width,float height,float depth,float wall,int segments,float inset){
	Mesh m={0};
	if(segments<6) segments=16;
	float halfW=width*0.5f, halfH=height*0.5f;
	float outerR=halfW;
	float spring=halfH-outerR;  /* Y of semicircle center */
	float stemH=height-outerR;  /* height of rectangular stem below spring */
	float halfD=depth*0.5f;
	if(outerR<=1e-6f || stemH<1e-6f || depth<=1e-6f) return gen_box(width,height,depth);

	int isFrame = (wall>1e-6f && wall<outerR-1e-6f);
	float innerR = isFrame ? (outerR-wall) : 0.0f;

	if(inset<0.0f) inset=0.0f;
	if(inset>depth-1e-4f) inset=depth-1e-4f;
	float frontZ = halfD - inset;
	float backZ  = -halfD;

	/* Pre-compute arc sample positions (segments+1 points, angle PI..0) */
	int n=segments;
	vec3 *oF = malloc(sizeof(vec3)*(size_t)(n+1)); /* outer arc, front face */
	vec3 *oB = malloc(sizeof(vec3)*(size_t)(n+1)); /* outer arc, back face  */
	vec3 *iF = malloc(sizeof(vec3)*(size_t)(n+1)); /* inner arc, front face (frame only) */
	vec3 *iB = malloc(sizeof(vec3)*(size_t)(n+1)); /* inner arc, back face  (frame only) */
	vec3 *oN = malloc(sizeof(vec3)*(size_t)(n+1)); /* outward radial normals */
	vec3 *iN = malloc(sizeof(vec3)*(size_t)(n+1)); /* inward radial normals  */
	for(int i=0;i<=n;i++){
		float a = M_PIf - (float)i/(float)n * M_PIf; /* PI→0, left to right */
		float ca=cosf(a), sa=sinf(a);
		oF[i] = v3(ca*outerR, spring+sa*outerR, frontZ);
		oB[i] = v3(ca*outerR, spring+sa*outerR, backZ);
		oN[i] = v3(ca, sa, 0.0f);
		if(isFrame){
			iF[i] = v3(ca*innerR, spring+sa*innerR, frontZ);
			iB[i] = v3(ca*innerR, spring+sa*innerR, backZ);
			iN[i] = v3(-ca, -sa, 0.0f);
		}
	}

	/* add_quad(a,b,c,d,n): tris (a,c,b),(a,d,c). */
	vec3 fN=v3(0,0,1), bN=v3(0,0,-1);
	float botY=-halfH;

	if(!isFrame){
		/* ---- SOLID ARCH ---- */

		for(int i=0;i<n;i++)
			add_quad_n4(&m, oB[i],oN[i], oB[i+1],oN[i+1], oF[i+1],oN[i+1], oF[i],oN[i]);

		/* front face (+Z): triangle fan from bl */
		{
			vec3 bl=v3(-halfW,botY,frontZ), br=v3(halfW,botY,frontZ);
			int ibl=mesh_add_vert(&m,bl,fN);
			int ibr=mesh_add_vert(&m,br,fN);
			int iprev=mesh_add_vert(&m,oF[n],fN);
			mesh_add_tri(&m, ibl, ibr, iprev);
			for(int i=n-1;i>=0;i--){
				int iv=mesh_add_vert(&m,oF[i],fN);
				mesh_add_tri(&m, ibl, iprev, iv);
				iprev=iv;
			}
		}

		/* back face (-Z): triangle fan from bl */
		{
			vec3 bl=v3(-halfW,botY,backZ), br=v3(halfW,botY,backZ);
			int ibl=mesh_add_vert(&m,bl,bN);
			int ibr=mesh_add_vert(&m,br,bN);
			int iprev=mesh_add_vert(&m,oB[0],bN);
			for(int i=0;i<n;i++){
				int iv=mesh_add_vert(&m,oB[i+1],bN);
				mesh_add_tri(&m, ibl, iprev, iv);
				iprev=iv;
			}
			mesh_add_tri(&m, ibl, iprev, ibr);
		}

		/* left side (-X) */
		{
			vec3 lbF=v3(-halfW,botY,frontZ), lbB=v3(-halfW,botY,backZ);
			add_quad(&m, lbF, lbB, oB[0], oF[0], v3(-1,0,0));
		}
		/* right side (+X) */
		{
			vec3 rbF=v3(halfW,botY,frontZ), rbB=v3(halfW,botY,backZ);
			add_quad(&m, rbB, rbF, oF[n], oB[n], v3(1,0,0));
		}
		/* bottom (-Y) */
		{
			vec3 blF=v3(-halfW,botY,frontZ), brF=v3(halfW,botY,frontZ);
			vec3 blB=v3(-halfW,botY,backZ),  brB=v3(halfW,botY,backZ);
			add_quad(&m, blF, brF, brB, blB, v3(0,-1,0));
		}

	} else {
		/* ---- FRAME ARCH ---- */

		for(int i=0;i<n;i++)
			add_quad_n4(&m, oB[i],oN[i], oB[i+1],oN[i+1], oF[i+1],oN[i+1], oF[i],oN[i]);
		for(int i=0;i<n;i++)
			add_quad_n4(&m, iB[i+1],iN[i+1], iB[i],iN[i], iF[i],iN[i], iF[i+1],iN[i+1]);
		for(int i=0;i<n;i++)
			add_quad(&m, oF[i], oF[i+1], iF[i+1], iF[i], fN);
		for(int i=0;i<n;i++)
			add_quad(&m, oB[i+1], oB[i], iB[i], iB[i+1], bN);

		/* Arc end caps connecting outer to inner at the spring line */
		{
			add_quad(&m, oF[0], iF[0], iB[0], oB[0], v3(-1,0,0));
			add_quad(&m, oB[n], iB[n], iF[n], oF[n], v3(1,0,0));
		}

		/* ---- U-shaped base: legs + sill as one sealed piece ----
		 * Cross-section from front (U-shape):
		 *   outerL..innerL = left leg column
		 *   innerL..innerR = inner opening (sill top visible)
		 *   innerR..outerR = right leg column
		 *
		 * Vertex naming: [l|r][O|I] = left/right outer/inner x,
		 *   [b|s|p] = botY / sillTop / spring y,  [F|B] = front/back z.
		 */
		{
			float lO=-outerR, lI=-innerR, rI=innerR, rO=outerR;
			float bY=botY, sY=botY+wall, pY=spring;

			/* front face (+Z): 5 quads matching all internal boundaries.
			 * Bottom row split at lI, rI: [lO,lI] + [lI,rI] + [rI,rO] x [bY,sY]
			 * Upper pillars: [lO,lI] + [rI,rO] x [sY,pY] */
			add_quad(&m, v3(lI,bY,frontZ), v3(lO,bY,frontZ), v3(lO,sY,frontZ), v3(lI,sY,frontZ), fN);
			add_quad(&m, v3(rI,bY,frontZ), v3(lI,bY,frontZ), v3(lI,sY,frontZ), v3(rI,sY,frontZ), fN);
			add_quad(&m, v3(rO,bY,frontZ), v3(rI,bY,frontZ), v3(rI,sY,frontZ), v3(rO,sY,frontZ), fN);
			add_quad(&m, v3(lI,sY,frontZ), v3(lO,sY,frontZ), v3(lO,pY,frontZ), v3(lI,pY,frontZ), fN);
			add_quad(&m, v3(rO,sY,frontZ), v3(rI,sY,frontZ), v3(rI,pY,frontZ), v3(rO,pY,frontZ), fN);

			/* back face (-Z): mirror */
			add_quad(&m, v3(lO,bY,backZ), v3(lI,bY,backZ), v3(lI,sY,backZ), v3(lO,sY,backZ), bN);
			add_quad(&m, v3(lI,bY,backZ), v3(rI,bY,backZ), v3(rI,sY,backZ), v3(lI,sY,backZ), bN);
			add_quad(&m, v3(rI,bY,backZ), v3(rO,bY,backZ), v3(rO,sY,backZ), v3(rI,sY,backZ), bN);
			add_quad(&m, v3(lO,sY,backZ), v3(lI,sY,backZ), v3(lI,pY,backZ), v3(lO,pY,backZ), bN);
			add_quad(&m, v3(rI,sY,backZ), v3(rO,sY,backZ), v3(rO,pY,backZ), v3(rI,pY,backZ), bN);

			/* outer left (-X): split at sY to match front/back decomposition */
			add_quad(&m, v3(lO,bY,frontZ), v3(lO,bY,backZ), v3(lO,sY,backZ), v3(lO,sY,frontZ), v3(-1,0,0));
			add_quad(&m, v3(lO,sY,frontZ), v3(lO,sY,backZ), v3(lO,pY,backZ), v3(lO,pY,frontZ), v3(-1,0,0));
			/* outer right (+X): split at sY */
			add_quad(&m, v3(rO,bY,backZ), v3(rO,bY,frontZ), v3(rO,sY,frontZ), v3(rO,sY,backZ), v3(1,0,0));
			add_quad(&m, v3(rO,sY,backZ), v3(rO,sY,frontZ), v3(rO,pY,frontZ), v3(rO,pY,backZ), v3(1,0,0));
			/* inner left (+X face at x=lI): [sY, pY] */
			add_quad(&m, v3(lI,sY,backZ), v3(lI,sY,frontZ), v3(lI,pY,frontZ), v3(lI,pY,backZ), v3(1,0,0));
			/* inner right (-X face at x=rI): [sY, pY] */
			add_quad(&m, v3(rI,sY,frontZ), v3(rI,sY,backZ), v3(rI,pY,backZ), v3(rI,pY,frontZ), v3(-1,0,0));

			/* bottom (-Y): split to match front/back decomposition */
			add_quad(&m, v3(lO,bY,frontZ), v3(lI,bY,frontZ), v3(lI,bY,backZ), v3(lO,bY,backZ), v3(0,-1,0));
			add_quad(&m, v3(lI,bY,frontZ), v3(rI,bY,frontZ), v3(rI,bY,backZ), v3(lI,bY,backZ), v3(0,-1,0));
			add_quad(&m, v3(rI,bY,frontZ), v3(rO,bY,frontZ), v3(rO,bY,backZ), v3(rI,bY,backZ), v3(0,-1,0));

			/* sill top (+Y at y=sY): only inner opening [lI, rI] */
			add_quad(&m, v3(lI,sY,backZ), v3(rI,sY,backZ), v3(rI,sY,frontZ), v3(lI,sY,frontZ), v3(0,1,0));

			/* leg tops (+Y at y=pY): pair with arc end caps */
			add_quad(&m, v3(lO,pY,backZ), v3(lI,pY,backZ), v3(lI,pY,frontZ), v3(lO,pY,frontZ), v3(0,1,0));
			add_quad(&m, v3(rI,pY,backZ), v3(rO,pY,backZ), v3(rO,pY,frontZ), v3(rI,pY,frontZ), v3(0,1,0));
		}
	}

	free(oF); free(oB); free(iF); free(iB); free(oN); free(iN);
	return m;
}

/* Box with a circular through-hole centered at (cx,cy) relative to the box center.
 * Hole axis is Z (goes through the full depth). cx,cy are offsets from box center.
 * Front/back faces are triangulated as an annulus via angular sweep from the hole center:
 * for each arc segment, shoot rays to the box boundary and collect any box corners that
 * fall in the angular wedge, then fan-triangulate the resulting polygon. */
Mesh gen_box_hole_cylinder(float w, float h, float depth, float cx, float cy, float r, int sides){
	Mesh m={0};
	if(sides<8) sides=32;
	float hw=w*0.5f, hh=h*0.5f, hd=depth*0.5f;
	vec3 fN=v3(0,0,1), bN=v3(0,0,-1);

	vec3 *aF=malloc(sizeof(vec3)*(size_t)sides);
	vec3 *aB=malloc(sizeof(vec3)*(size_t)sides);
	vec3 *aN=malloc(sizeof(vec3)*(size_t)sides);
	for(int i=0;i<sides;i++){
		float a=2.0f*M_PIf*(float)i/(float)sides;
		float ca=cosf(a), sa=sinf(a);
		aF[i]=v3(cx+ca*r, cy+sa*r,  hd);
		aB[i]=v3(cx+ca*r, cy+sa*r, -hd);
		aN[i]=v3(ca, sa, 0.0f);
	}

	/* Front (+Z) and back (-Z) annular faces */
	for(int face=0;face<2;face++){
		float fz=(face==0)?hd:-hd;
		vec3 fn=(face==0)?fN:bN;
		vec3 bTL=v3(-hw, hh,fz), bTR=v3( hw, hh,fz);
		vec3 bBL=v3(-hw,-hh,fz), bBR=v3( hw,-hh,fz);
		vec3 cpts[4]={bTL,bTR,bBR,bBL};
		float cangs[4]={
			atan2f( hh-cy,-hw-cx), atan2f( hh-cy, hw-cx),
			atan2f(-hh-cy, hw-cx), atan2f(-hh-cy,-hw-cx)
		};

		for(int i=0;i<sides;i++){
			int j=(i+1)%sides;
			vec3 p0=(face==0)?aF[i]:aB[i];
			vec3 p1=(face==0)?aF[j]:aB[j];
			float a0=2.0f*M_PIf*(float)i/(float)sides;
			float a1=2.0f*M_PIf*(float)j/(float)sides;
			float ca0=cosf(a0), sa0=sinf(a0);
			float ca1=cosf(a1), sa1=sinf(a1);

			/* Ray from hole center in direction (ca,sa) to box boundary */
			float t0=1e9f, t1=1e9f;
			if(ca0<-1e-6f){ float t=(-hw-cx)/ca0; if(t>0&&t<t0){ float y=cy+sa0*t; if(y>=-hh&&y<=hh) t0=t; } }
			if(ca0> 1e-6f){ float t=( hw-cx)/ca0; if(t>0&&t<t0){ float y=cy+sa0*t; if(y>=-hh&&y<=hh) t0=t; } }
			if(sa0<-1e-6f){ float t=(-hh-cy)/sa0; if(t>0&&t<t0){ float x=cx+ca0*t; if(x>=-hw&&x<=hw) t0=t; } }
			if(sa0> 1e-6f){ float t=( hh-cy)/sa0; if(t>0&&t<t0){ float x=cx+ca0*t; if(x>=-hw&&x<=hw) t0=t; } }
			if(ca1<-1e-6f){ float t=(-hw-cx)/ca1; if(t>0&&t<t1){ float y=cy+sa1*t; if(y>=-hh&&y<=hh) t1=t; } }
			if(ca1> 1e-6f){ float t=( hw-cx)/ca1; if(t>0&&t<t1){ float y=cy+sa1*t; if(y>=-hh&&y<=hh) t1=t; } }
			if(sa1<-1e-6f){ float t=(-hh-cy)/sa1; if(t>0&&t<t1){ float x=cx+ca1*t; if(x>=-hw&&x<=hw) t1=t; } }
			if(sa1> 1e-6f){ float t=( hh-cy)/sa1; if(t>0&&t<t1){ float x=cx+ca1*t; if(x>=-hw&&x<=hw) t1=t; } }

			vec3 b0=v3(cx+ca0*t0, cy+sa0*t0, fz);
			vec3 b1=v3(cx+ca1*t1, cy+sa1*t1, fz);

			/* Collect box corners falling in the angular sector [a0, a1] */
			float angStart=atan2f(sa0,ca0), angEnd=atan2f(sa1,ca1);
			while(angEnd<angStart) angEnd+=2.0f*M_PIf;
			vec3 corners[4]; int ncorners=0;
			for(int k=0;k<4;k++){
				float a=cangs[k]; while(a<angStart) a+=2.0f*M_PIf;
				if(a<angEnd) corners[ncorners++]=cpts[k];
			}
			/* Sort by angle (insertion sort, at most 4 items) */
			for(int a=1;a<ncorners;a++){
				vec3 kv=corners[a]; float ka=atan2f(kv.y-cy,kv.x-cx); while(ka<angStart) ka+=2.0f*M_PIf;
				int b=a-1;
				while(b>=0){
					float ba=atan2f(corners[b].y-cy,corners[b].x-cx); while(ba<angStart) ba+=2.0f*M_PIf;
					if(ba<=ka) break;
					corners[b+1]=corners[b]; b--;
				}
				corners[b+1]=kv;
			}

			/* Fan polygon: [p0, b0, corners..., b1, p1] */
			vec3 chain[8]; int nc=0;
			chain[nc++]=p0; chain[nc++]=b0;
			for(int k=0;k<ncorners;k++) chain[nc++]=corners[k];
			chain[nc++]=b1; chain[nc++]=p1;
			for(int k=1;k+1<nc;k++){
				int ia=mesh_add_vert(&m,chain[0],fn);
				int ib,ic;
				if(face==0){ ib=mesh_add_vert(&m,chain[k],fn);   ic=mesh_add_vert(&m,chain[k+1],fn); }
				else        { ib=mesh_add_vert(&m,chain[k+1],fn); ic=mesh_add_vert(&m,chain[k],fn);   }
				mesh_add_tri(&m,ia,ib,ic);
			}
		}
	}

	/* Cylindrical tunnel (inward normals) */
	for(int i=0;i<sides;i++){
		int j=(i+1)%sides;
		vec3 inN0=vscale(aN[i],-1.0f), inN1=vscale(aN[j],-1.0f);
		add_quad_n4(&m, aF[i],inN0, aF[j],inN1, aB[j],inN1, aB[i],inN0);
	}

	/* 4 outer box side walls */
	add_quad(&m, v3(-hw,-hh,-hd), v3( hw,-hh,-hd), v3( hw,-hh, hd), v3(-hw,-hh, hd), v3(0,-1,0));
	add_quad(&m, v3( hw,-hh,-hd), v3( hw, hh,-hd), v3( hw, hh, hd), v3( hw,-hh, hd), v3(1,0,0));
	add_quad(&m, v3( hw, hh,-hd), v3(-hw, hh,-hd), v3(-hw, hh, hd), v3( hw, hh, hd), v3(0,1,0));
	add_quad(&m, v3(-hw, hh,-hd), v3(-hw,-hh,-hd), v3(-hw,-hh, hd), v3(-hw, hh, hd), v3(-1,0,0));

	free(aF); free(aB); free(aN);
	return m;
}

/* Box with a roman-arch through-hole: hole axis is Z (through the full depth).
 * Arch radius = w/2 (semicircle sits on rectangular stem). Arch opening spans full width
 * and full height of the box — the only solid face region is the lunette above the arc.
 * Front/back lunettes are fan-triangulated from the top-left corner. */
Mesh gen_box_hole_arch(float w, float h, float depth, int sides){
	Mesh m={0};
	if(sides<8) sides=16;
	float hw=w*0.5f, hh=h*0.5f, hd=depth*0.5f;
	float archR=hw;
	float springY=hh-archR; /* arc center Y relative to box center */
	vec3 fN=v3(0,0,1), bN=v3(0,0,-1);

	/* Arc sample points: angle PI→0 (left to right), sides+1 points */
	vec3 *aF=malloc(sizeof(vec3)*(size_t)(sides+1));
	vec3 *aB=malloc(sizeof(vec3)*(size_t)(sides+1));
	vec3 *aN=malloc(sizeof(vec3)*(size_t)(sides+1));
	for(int i=0;i<=sides;i++){
		float a=M_PIf-(float)i/(float)sides*M_PIf;
		float ca=cosf(a), sa=sinf(a);
		aF[i]=v3(ca*archR, springY+sa*archR,  hd);
		aB[i]=v3(ca*archR, springY+sa*archR, -hd);
		aN[i]=v3(ca, sa, 0.0f);
	}

	/* Front (+Z) and back (-Z) lunette faces: polygon [TL, TR, arc[0..sides]]
	 * fan-triangulated from TL. */
	for(int face=0;face<2;face++){
		float fz=(face==0)?hd:-hd;
		vec3 fn=(face==0)?fN:bN;
		vec3 *arc=(face==0)?aF:aB;
		vec3 TL=v3(-hw,hh,fz), TR=v3(hw,hh,fz);
		int iTL=mesh_add_vert(&m,TL,fn), iTR=mesh_add_vert(&m,TR,fn);
		int iprev=mesh_add_vert(&m,arc[0],fn);
		if(face==0) mesh_add_tri(&m,iTL,iTR,iprev);
		else         mesh_add_tri(&m,iTL,iprev,iTR);
		for(int i=0;i<sides;i++){
			int inext=mesh_add_vert(&m,arc[i+1],fn);
			if(face==0) mesh_add_tri(&m,iTL,iprev,inext);
			else         mesh_add_tri(&m,iTL,inext,iprev);
			iprev=inext;
		}
	}

	/* Arch tunnel surface (inward normals) */
	for(int i=0;i<sides;i++){
		vec3 inN0=vscale(aN[i],-1.0f), inN1=vscale(aN[i+1],-1.0f);
		add_quad_n4(&m, aF[i],inN0, aF[i+1],inN1, aB[i+1],inN1, aB[i],inN0);
	}

	/* Vertical tunnel walls at x=±hw from bottom to spring line (inward-facing) */
	add_quad(&m, v3(-hw,-hh,-hd), v3(-hw,springY,-hd), v3(-hw,springY, hd), v3(-hw,-hh, hd), v3( 1,0,0));
	add_quad(&m, v3( hw,-hh, hd), v3( hw,springY, hd), v3( hw,springY,-hd), v3( hw,-hh,-hd), v3(-1,0,0));

	/* Tunnel floor (inward +Y) */
	add_quad(&m, v3(-hw,-hh, hd), v3( hw,-hh, hd), v3( hw,-hh,-hd), v3(-hw,-hh,-hd), v3(0,1,0));

	/* Outer side walls */
	add_quad(&m, v3(-hw,-hh,-hd), v3(-hw,-hh, hd), v3(-hw, hh, hd), v3(-hw, hh,-hd), v3(-1,0,0));
	add_quad(&m, v3( hw,-hh, hd), v3( hw,-hh,-hd), v3( hw, hh,-hd), v3( hw, hh, hd), v3( 1,0,0));
	add_quad(&m, v3(-hw, hh,-hd), v3( hw, hh,-hd), v3( hw, hh, hd), v3(-hw, hh, hd), v3(0, 1,0));
	add_quad(&m, v3( hw,-hh,-hd), v3(-hw,-hh,-hd), v3(-hw,-hh, hd), v3( hw,-hh, hd), v3(0,-1,0));

	free(aF); free(aB); free(aN);
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
