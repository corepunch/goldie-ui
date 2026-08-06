#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "shader.h"

static GLuint prog;
static GLuint vlocPos, vlocNrm;
static GLint ulocViewProj, ulocViewPos, ulocLightPos, ulocLightColor, ulocLightRadius;
static GLint ulocColor, ulocShininess;

static const char *vs_src =
"attribute vec3 aPos;\n"
"attribute vec3 aNrm;\n"
"uniform mat4 uViewProj;\n"
"varying vec3 vWorldPos;\n"
"varying vec3 vWorldNrm;\n"
"void main(){\n"
"    vWorldPos=aPos;\n"
"    vWorldNrm=aNrm;\n"
"    gl_Position=uViewProj*vec4(aPos,1.0);\n"
"}\n";

static const char *fs_src =
"varying vec3 vWorldPos;\n"
"varying vec3 vWorldNrm;\n"
"uniform vec3 uViewPos;\n"
"uniform vec4 uLightPos;\n"
"uniform vec3 uLightColor;\n"
"uniform float uLightRadius;\n"
"uniform vec3 uColor;\n"
"uniform float uShininess;\n"
"vec3 linearToSrgb(vec3 c){\n"
"    vec3 lo=c*12.92;\n"
"    vec3 hi=1.055*pow(c,vec3(1.0/2.4))-0.055;\n"
"    return mix(lo,hi,step(vec3(0.0031308),c));\n"
"}\n"
"vec3 srgbToLinear(vec3 c){\n"
"    vec3 lo=c/12.92;\n"
"    vec3 hi=pow((c+0.055)/1.055,vec3(2.4));\n"
"    return mix(lo,hi,step(vec3(0.04045),c));\n"
"}\n"
"void main(){\n"
"    vec3 N=normalize(vWorldNrm);\n"
"    vec3 V=normalize(uViewPos-vWorldPos);\n"
"    vec3 L=normalize(uLightPos.xyz-vWorldPos*uLightPos.w);\n"
"    vec3 H=normalize(V+L);\n"
"    float NdotL=max(dot(N,L),0.0);\n"
"    float NdotH=max(dot(N,H),0.0);\n"
"    float NdotV=max(dot(N,V),0.0);\n"
"    float roughness=clamp(exp(-uShininess/18.0),0.005,1.0);\n"
"    float alpha=roughness*roughness;\n"
"    float a2=alpha*alpha;\n"
"    float denom=NdotH*NdotH*(a2-1.0)+1.0;\n"
"    float D=a2/(3.14159*denom*denom);\n"
"    float k=alpha/2.0;\n"
"    float G1=NdotL/(NdotL*(1.0-k)+k);\n"
"    float G2=NdotV/(NdotV*(1.0-k)+k);\n"
"    float G=G1*G2;\n"
"    float F0=0.04;\n"
"    float F=F0+(1.0-F0)*pow(1.0-max(dot(V,H),0.0),5.0);\n"
"    vec3 spec=(D*G*F)/(max(4.0*NdotL*NdotV,0.001))*uLightColor;\n"
"    vec3 diff=uColor*(1.0-F)/3.14159*NdotL*uLightColor;\n"
"    float att=1.0;\n"
"    if(uLightRadius>0.0 && uLightPos.w>0.0){\n"
"        float dist=length(uLightPos.xyz-vWorldPos);\n"
"        att=1.0/(1.0+2.0*dist/uLightRadius+(dist*dist)/(uLightRadius*uLightRadius));\n"
"    }\n"
"    vec3 lit=(diff+spec)*att;\n"
"    float seed=dot(uLightPos.xyz,vec3(17.0,59.0,113.0));\n"
"    float noise=fract(52.9829189*fract(dot(gl_FragCoord.xy+seed,vec2(0.06711056,0.00583715))))-0.5;\n"
"    vec3 encoded=clamp(linearToSrgb(max(lit,vec3(0.0)))+noise/255.0,0.0,1.0);\n"
"    gl_FragColor=vec4(srgbToLinear(encoded),1.0);\n"
"}\n";

static GLuint compile_shader(GLenum type, const char *src){
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	GLint ok;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if(!ok){
		char info[512];
		glGetShaderInfoLog(s, 512, NULL, info);
		fprintf(stderr, "shader compile error:\n%s\n", info);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

static vec3 srgb_to_linear(vec3 color){
	return v3(powf(color.x,2.2f),powf(color.y,2.2f),powf(color.z,2.2f));
}

void shader_init(void){
	GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
	if(!vs || !fs){
		if(vs) glDeleteShader(vs);
		if(fs) glDeleteShader(fs);
		fprintf(stderr, "shader_init: failed to compile shaders, continuing without PBR\n");
		prog = 0;
		return;
	}
	prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "aPos");
	glBindAttribLocation(prog, 1, "aNrm");
	glLinkProgram(prog);
	GLint ok;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if(!ok){
		char info[512];
		glGetProgramInfoLog(prog, 512, NULL, info);
		fprintf(stderr, "shader link error:\n%s\n", info);
		glDeleteProgram(prog);
		prog = 0;
	}
	glDeleteShader(vs);
	glDeleteShader(fs);

	vlocPos = 0;
	vlocNrm = 1;
	ulocViewProj    = glGetUniformLocation(prog, "uViewProj");
	ulocViewPos     = glGetUniformLocation(prog, "uViewPos");
	ulocLightPos    = glGetUniformLocation(prog, "uLightPos");
	ulocLightColor  = glGetUniformLocation(prog, "uLightColor");
	ulocLightRadius = glGetUniformLocation(prog, "uLightRadius");
	ulocColor       = glGetUniformLocation(prog, "uColor");
	ulocShininess   = glGetUniformLocation(prog, "uShininess");

	fprintf(stderr, "PBR shader initialized\n");
}

void shader_deinit(void){
	if(prog) glDeleteProgram(prog);
	prog = 0;
}

void shader_bind(void){
	glUseProgram(prog);
}

void shader_unbind(void){
	glUseProgram(0);
}

void shader_set_viewproj(mat4 m){
	glUniformMatrix4fv(ulocViewProj, 1, GL_FALSE, m.m);
}

void shader_set_camera_pos(vec3 pos){
	glUniform3f(ulocViewPos, pos.x, pos.y, pos.z);
}

void shader_set_light(Light *L){
	vec3 color=srgb_to_linear(L->color);
	if(L->isDirectional)
		glUniform4f(ulocLightPos, -L->dir.x, -L->dir.y, -L->dir.z, 0.0f);
	else
		glUniform4f(ulocLightPos, L->pos.x, L->pos.y, L->pos.z, 1.0f);
	glUniform3f(ulocLightColor, color.x*L->intensity, color.y*L->intensity, color.z*L->intensity);
	glUniform1f(ulocLightRadius, L->isDirectional ? 0.0f : L->radius);
}

void shader_set_material(vec3 color, float shininess){
	color=srgb_to_linear(color);
	glUniform3f(ulocColor, color.x, color.y, color.z);
	glUniform1f(ulocShininess, shininess);
}

void shader_draw_mesh(Mesh *m){
	glEnableVertexAttribArray(vlocPos);
	glEnableVertexAttribArray(vlocNrm);
	glVertexAttribPointer(vlocPos, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &m->verts[0].pos.x);
	glVertexAttribPointer(vlocNrm, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &m->verts[0].nrm.x);
	glDrawElements(GL_TRIANGLES, m->ntris * 3, GL_UNSIGNED_INT, m->tris);
	glDisableVertexAttribArray(vlocPos);
	glDisableVertexAttribArray(vlocNrm);
}
