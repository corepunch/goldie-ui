#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "materials.h"

typedef struct { float r,g,b; } MVec3;

static MVec3 mv3(float r,float g,float b){ MVec3 v={r,g,b}; return v; }
static float mclamp(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }
static float mlerp(float a,float b,float t){ return a+(b-a)*t; }
static float msmooth(float t){ return t*t*(3.0f-2.0f*t); }
static MVec3 mmix(MVec3 a,MVec3 b,float t){ t=mclamp(t,0,1); return mv3(mlerp(a.r,b.r,t),mlerp(a.g,b.g,t),mlerp(a.b,b.b,t)); }
static MVec3 mscale(MVec3 a,float s){ return mv3(a.r*s,a.g*s,a.b*s); }
static MVec3 madd(MVec3 a,MVec3 b){ return mv3(a.r+b.r,a.g+b.g,a.b+b.b); }

static uint32_t hash2i(int x,int y,uint32_t seed){
	uint32_t h=(uint32_t)x*374761393u+(uint32_t)y*668265263u+seed*2246822519u;
	h=(h^(h>>13))*1274126177u; h^=h>>16; return h;
}
static float hash01(int x,int y,uint32_t seed){
	return (float)(hash2i(x,y,seed)&0xFFFFFFu)/(float)0x1000000u;
}
static void hashGrad(int x,int y,uint32_t seed,float*gx,float*gy){
	float a=hash01(x,y,seed)*6.2831853f;
	*gx=cosf(a); *gy=sinf(a);
}

static float gnoise(float x,float y,int period,uint32_t seed){
	int ix0=(int)floorf(x), iy0=(int)floorf(y);
	float fx=x-ix0, fy=y-iy0;
	int ix1=ix0+1, iy1=iy0+1;
	int wx0=((ix0%period)+period)%period, wy0=((iy0%period)+period)%period;
	int wx1=((ix1%period)+period)%period, wy1=((iy1%period)+period)%period;
	float g00x,g00y,g10x,g10y,g01x,g01y,g11x,g11y;
	hashGrad(wx0,wy0,seed,&g00x,&g00y); hashGrad(wx1,wy0,seed,&g10x,&g10y);
	hashGrad(wx0,wy1,seed,&g01x,&g01y); hashGrad(wx1,wy1,seed,&g11x,&g11y);
	float d00=g00x*fx+g00y*fy, d10=g10x*(fx-1)+g10y*fy;
	float d01=g01x*fx+g01y*(fy-1), d11=g11x*(fx-1)+g11y*(fy-1);
	float u=msmooth(fx), v=msmooth(fy);
	return mlerp(mlerp(d00,d10,u),mlerp(d01,d11,u),v)*0.7071f+0.5f;
}

static float fbm(float u,float v,int octaves,float gain,int baseFreq,uint32_t seed){
	float sum=0,amp=1,norm=0; int freq=baseFreq;
	for(int o=0;o<octaves;o++){
		sum+=gnoise(u*freq,v*freq,freq,seed+(uint32_t)o*101u)*amp;
		norm+=amp; amp*=gain; freq*=2;
	}
	return sum/norm;
}

static float warpedFbm(float u,float v,int octaves,int baseFreq,uint32_t seed,float warpAmt){
	float qx=fbm(u+0.0f,v+0.0f,octaves,0.5f,baseFreq,seed+11);
	float qy=fbm(u+5.2f,v+1.3f,octaves,0.5f,baseFreq,seed+37);
	float rx=fbm(u+warpAmt*qx+1.7f,v+warpAmt*qy+9.2f,octaves,0.5f,baseFreq,seed+53);
	float ry=fbm(u+warpAmt*qx+8.3f,v+warpAmt*qy+2.8f,octaves,0.5f,baseFreq,seed+71);
	return fbm(u+warpAmt*rx,v+warpAmt*ry,octaves,0.5f,baseFreq,seed);
}

static void worley(float u,float v,int cells,uint32_t seed,float*F1,float*F2,uint32_t*cellId){
	float px=u*cells, py=v*cells;
	int cx=(int)floorf(px), cy=(int)floorf(py);
	float f1=1e9f,f2=1e9f; uint32_t id=0;
	for(int oy=-1;oy<=1;oy++)for(int ox=-1;ox<=1;ox++){
		int gx=cx+ox, gy=cy+oy;
		int wx=((gx%cells)+cells)%cells, wy=((gy%cells)+cells)%cells;
		uint32_t h=hash2i(wx,wy,seed);
		float jx=gx+((h&0xFFFFu)/65536.0f), jy=gy+(((h>>16)&0xFFFFu)/65536.0f);
		float dx=jx-px, dy=jy-py, d=sqrtf(dx*dx+dy*dy);
		if(d<f1){ f2=f1; f1=d; id=h; } else if(d<f2) f2=d;
	}
	*F1=f1; *F2=f2; *cellId=id;
}

static MVec3 matWood(float u,float v,uint32_t seed){
	MVec3 light=mv3(0.55f,0.37f,0.20f), dark=mv3(0.22f,0.13f,0.06f);
	float warp=warpedFbm(u*0.6f,v*0.15f,4,4,seed+900,1.2f);
	float wx=u+(warp-0.5f)*0.35f;
	float rings=sinf((wx*40.0f)*(float)M_PI);
	rings=powf((rings+1.0f)*0.5f,3.0f);
	float mottle=fbm(u,v,3,0.55f,6,seed+300);
	float f1,f2; uint32_t id; worley(u,v,3,seed+40,&f1,&f2,&id);
	float knot=(id%97u==0)?mclamp(1.0f-f1*2.2f,0,1):0.0f;
	float mix=mclamp(0.35f*rings+0.65f*mottle,0,1);
	MVec3 col=mmix(dark,light,mix);
	col=mmix(col,mscale(dark,0.4f),knot*0.9f);
	float edge=fminf(v,1.0f-v);
	float seam=1.0f-msmooth(mclamp(edge/0.035f,0,1));
	col=mscale(col,1.0f-0.55f*seam);
	float speck=(hash01((int)(u*256),(int)(v*256),seed+55)-0.5f)*0.04f;
	return madd(col,mv3(speck,speck,speck));
}

static MVec3 matConcrete(float u,float v,uint32_t seed){
	float base=0.60f;
	float blotch=fbm(u,v,4,0.5f,4,seed+10);
	float f1,f2,f1b,f2b; uint32_t id,idb;
	worley(u,v,22,seed+20,&f1,&f2,&id);
	worley(u,v,4,seed+21,&f1b,&f2b,&idb);
	float aggregate=1.0f-mclamp(f1*2.2f,0,1);
	float crackEdge=f2b-f1b;
	float crack=1.0f-msmooth(mclamp(crackEdge*22.0f,0,1));
	crack*=(hash01((int)(u*20),(int)(v*20),seed+88)>0.80f)?1.0f:0.0f;
	float tone=base+(blotch-0.5f)*0.30f+(aggregate-0.5f)*0.06f;
	tone-=crack*0.16f;
	float speck=(hash01((int)(u*256),(int)(v*256),seed+77)-0.5f)*0.035f;
	tone+=speck;
	float tint=fbm(u,v,2,0.5f,3,seed+400)-0.5f;
	return mv3(tone+tint*0.02f,tone,tone-tint*0.02f);
}

static MVec3 matMarble(float u,float v,uint32_t seed){
	MVec3 stoneA=mv3(0.90f,0.89f,0.87f),stoneB=mv3(0.55f,0.56f,0.60f),vein=mv3(0.20f,0.22f,0.28f);
	float w=warpedFbm(u,v,4,2,seed,2.2f);
	float band=sinf((u*3.0f+w*4.0f)*(float)M_PI);
	band=(band+1.0f)*0.5f;
	MVec3 col=mmix(stoneA,stoneB,fbm(u,v,2,0.5f,3,seed+60)*0.35f);
	float veinLine=powf(1.0f-fabsf(band-0.5f)*2.0f,12.0f);
	col=mmix(col,vein,veinLine*0.9f);
	return col;
}

static MVec3 matBrick(float u,float v,uint32_t seed){
	const float rows=6.0f,cols=10.0f,mortar=0.10f;
	float row=floorf(v*rows);
	float rowOffset=fmodf(row,2.0f)*0.5f;
	float bx=u*cols+rowOffset, col=floorf(bx);
	float fx=bx-col, fy=v*rows-row;
	float edge=fminf(fminf(fx,1.0f-fx)/mortar,fminf(fy,1.0f-fy)/(mortar*rows/cols*2.2f));
	float isMortar=1.0f-msmooth(mclamp(edge,0,1));
	uint32_t id=hash2i((int)col,(int)row,seed+5);
	float shade=0.7f+0.3f*hash01((int)col,(int)row,seed+9);
	MVec3 brickCol=mmix(mv3(0.55f,0.22f,0.15f),mv3(0.72f,0.35f,0.22f),shade);
	float mottle=fbm(u*cols,v*rows,2,0.5f,4,seed+id%50u);
	brickCol=madd(brickCol,mv3((mottle-0.5f)*0.08f,(mottle-0.5f)*0.06f,(mottle-0.5f)*0.05f));
	MVec3 mortarCol=mv3(0.72f,0.70f,0.65f);
	float mortarSpeck=hash01((int)(u*256),(int)(v*256),seed+66)*0.06f;
	mortarCol=madd(mortarCol,mv3(mortarSpeck,mortarSpeck,mortarSpeck));
	return mmix(brickCol,mortarCol,isMortar);
}

static MVec3 matRustedMetal(float u,float v,uint32_t seed){
	MVec3 steel=mv3(0.55f,0.56f,0.58f),rustA=mv3(0.55f,0.24f,0.10f),rustB=mv3(0.30f,0.12f,0.05f);
	float brush=fbm(u,0.0f,5,0.6f,90,seed+1);
	brush=powf(brush,1.6f)+0.15f*fbm(u,v*0.05f,3,0.5f,4,seed+2);
	float sheen=0.15f+0.85f*mclamp(brush,0,1);
	MVec3 base=mscale(steel,sheen);
	float f1,f2; uint32_t id; worley(u,v,7,seed+30,&f1,&f2,&id);
	float rustMask=mclamp(1.2f-f1*1.4f,0,1);
	rustMask*=(hash01((int)(id&1023),0,seed+31)>0.4f)?1.0f:0.0f;
	float rustDetail=fbm(u,v,3,0.5f,10,seed+33);
	MVec3 rustCol=mmix(rustB,rustA,rustDetail);
	MVec3 col=mmix(base,rustCol,rustMask*0.85f);
	float scratch=powf(fbm(u*3.0f,0.0f,2,0.5f,80,seed+900),6.0f);
	col=madd(col,mv3(scratch*0.15f,scratch*0.15f,scratch*0.15f));
	return col;
}

static MVec3 matPlaster(float u,float v,uint32_t seed){
	float blotch=fbm(u,v,4,0.5f,4,seed+50);
	float f1,f2; uint32_t id; worley(u,v,30,seed+60,&f1,&f2,&id);
	float grain=mclamp(f1*3.0f,0,1);
	float tone=0.85f+(blotch-0.5f)*0.12f+(grain-0.5f)*0.03f;
	float speck=(hash01((int)(u*256),(int)(v*256),seed+80)-0.5f)*0.02f;
	return mv3(tone+speck,tone+speck*0.7f,tone+speck*0.5f);
}

static MVec3 matFabric(float u,float v,uint32_t seed){
	const float threadFreq=56.0f;
	float jitter=fbm(u,v,2,0.5f,12,seed+44)-0.5f;
	float warp=sinf((u+jitter*0.01f)*threadFreq*(float)M_PI);
	float weft=sinf((v+jitter*0.01f)*threadFreq*(float)M_PI);
	float weave=(warp*weft>0.0f)?0.85f:0.58f;
	float threadNoise=fbm(u,v,2,0.5f,20,seed+3);
	MVec3 dye=mv3(0.55f,0.10f,0.14f);
	MVec3 col=mscale(dye,weave*(0.85f+0.3f*threadNoise));
	float fuzz=(hash01((int)(u*256),(int)(v*256),seed+91)-0.5f)*0.03f;
	return madd(col,mv3(fuzz,fuzz,fuzz));
}

typedef MVec3 (*MatFn)(float,float,uint32_t);

static const char *MAT_NAMES[NUM_MATERIALS]={
	"wood","concrete","stone","brick","metal","plaster","marble","fabric"
};
static const MatFn MAT_FNS[NUM_MATERIALS]={
	matWood,matConcrete,matMarble,matBrick,matRustedMetal,matPlaster,matMarble,matFabric
};
static const uint32_t MAT_SEEDS[NUM_MATERIALS]={
	1337,4242,77,55,9,50,77,8
};

int materials_index_for_name(const char *name){
	if(!name) return -1;
	for(int i=0;i<NUM_MATERIALS;i++) if(!strcmp(MAT_NAMES[i],name)) return i;
	if(!strcmp(name,"stone")) return 2;
	if(!strcmp(name,"bronze")||!strcmp(name,"iron")) return 4;
	if(!strcmp(name,"glass")) return 5;
	if(!strcmp(name,"wall")) return 3;
	return -1;
}

static void upload_texture(const MatFn fn,uint32_t seed,unsigned int tex){
	unsigned char *pixels=malloc((size_t)TEX_SIZE*TEX_SIZE*3);
	for(int y=0;y<TEX_SIZE;y++){
		float v=(float)y/(float)TEX_SIZE;
		for(int x=0;x<TEX_SIZE;x++){
			float u=(float)x/(float)TEX_SIZE;
			MVec3 c=fn(u,v,seed);
			int idx=(y*TEX_SIZE+x)*3;
			pixels[idx+0]=(unsigned char)(mclamp(c.r,0,1)*255.0f+0.5f);
			pixels[idx+1]=(unsigned char)(mclamp(c.g,0,1)*255.0f+0.5f);
			pixels[idx+2]=(unsigned char)(mclamp(c.b,0,1)*255.0f+0.5f);
		}
	}
	glBindTexture(GL_TEXTURE_2D,tex);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,TEX_SIZE,TEX_SIZE,0,GL_RGB,GL_UNSIGNED_BYTE,pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	free(pixels);
}

unsigned int materials_create_white_texture(void){
	unsigned char white[4]={255,255,255,255};
	unsigned int tex;
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_2D,tex);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,1,1,0,GL_RGB,GL_UNSIGNED_BYTE,white);
	return tex;
}

void materials_init(unsigned int *textures){
	glGenTextures(NUM_MATERIALS,textures);
	for(int i=0;i<NUM_MATERIALS;i++) upload_texture(MAT_FNS[i],MAT_SEEDS[i],textures[i]);
}

void materials_free(unsigned int *textures){
	glDeleteTextures(NUM_MATERIALS,textures);
}
