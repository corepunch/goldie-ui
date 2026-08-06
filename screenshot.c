#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL.h>
#include <OpenGL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "simplegl.h"

static int write_ppm(const char *path, unsigned char *pixels, int w, int h){
	FILE *f = fopen(path, "wb");
	if(!f){ fprintf(stderr, "cannot write %s\n", path); return 0; }
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for(int y = h-1; y >= 0; y--)
		fwrite(pixels + y*w*3, 3, (size_t)w, f);
	fclose(f);
	return 1;
}

static void strip_ext(char *dst, const char *path, size_t dstsz){
	const char *base = strrchr(path, '/');
	base = base ? base + 1 : path;
	const char *dot = strrchr(base, '.');
	size_t len = dot ? (size_t)(dot - base) : strlen(base);
	if(len >= dstsz) len = dstsz - 1;
	memcpy(dst, base, len);
	dst[len] = 0;
}

static void mkdir_p(const char *path){
	char tmp[512];
	snprintf(tmp, sizeof(tmp), "%s", path);
	for(char *p = tmp + 1; *p; p++){
		if(*p == '/'){ *p = 0; mkdir(tmp, 0755); *p = '/'; }
	}
	mkdir(tmp, 0755);
}

int main(int argc, char **argv){
	const char *scenePath = "scenes/sample_room.xml";
	const char *outPath = NULL;
	const char *camName = NULL;
	int allCameras = 0;
	int W = 1280, H = 800;
	int debugMode = DBG_HIDE_LIGHTS;

	for(int i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-o") && i+1 < argc) outPath = argv[++i];
		else if(!strcmp(argv[i], "-cam") && i+1 < argc) camName = argv[++i];
		else if(!strcmp(argv[i], "-all")) allCameras = 1;
		else if(!strcmp(argv[i], "-w") && i+1 < argc) W = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-h") && i+1 < argc) H = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-d") && i+1 < argc) debugMode = atoi(argv[++i]);
		else if(!strcmp(argv[i], "-no-shadows")) debugMode = DBG_NO_SHADOWS;
		else if(!strcmp(argv[i], "-wireframe")) debugMode |= DBG_NO_SHADOWS;
		else scenePath = argv[i];
	}

	Scene scene;
	if(!load_scene(scenePath, &scene)) return 1;
	scene_build_all_shadow_volumes(&scene);
	fprintf(stderr, "loaded %d objects, %d lights, %d materials, %d cameras\n",
			scene.nobjs, scene.nlights, scene.nmats, scene.ncameras);

	if(SDL_Init(SDL_INIT_VIDEO) != 0){
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1;
	}
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

	SDL_Window *win = SDL_CreateWindow("screenshot",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
	if(!win){ fprintf(stderr, "CreateWindow: %s\n", SDL_GetError()); return 1; }
	SDL_GLContext ctx = SDL_GL_CreateContext(win);
	if(!ctx){ fprintf(stderr, "GL_CreateContext: %s\n", SDL_GetError()); return 1; }

	unsigned char *pixels = malloc((size_t)(W * H * 3));
	char outDir[256];

	if(allCameras){
		if(outPath) snprintf(outDir, sizeof(outDir), "%s", outPath);
		else strip_ext(outDir, scenePath, sizeof(outDir));
		mkdir_p(outDir);
	}

	int ncam = allCameras ? scene.ncameras : 1;
	for(int ci = 0; ci < ncam; ci++){
		if(allCameras){
			Camera *c = &scene.cameras[ci];
			scene.camPos = c->pos;
			scene.camLook = c->look;
			scene.camFov = c->fov;
		} else if(camName){
			scene_select_camera(&scene, camName);
		}

		mat4 proj = mat4_perspective(scene.camFov, (float)W/(float)H, 0.1f, 100.0f);
		vec3 fwd = vnorm(vsub(scene.camLook, scene.camPos));
		mat4 view = mat4_lookat(scene.camPos, vadd(scene.camPos, fwd), v3(0,1,0));

		render_frame(&scene, W, H, proj, view, debugMode);
		glFinish();

		glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, pixels);

		char filePath[512];
		if(allCameras)
			snprintf(filePath, sizeof(filePath), "%s/%s.ppm", outDir, scene.cameras[ci].name);
		else if(outPath)
			snprintf(filePath, sizeof(filePath), "%s", outPath);
		else
			snprintf(filePath, sizeof(filePath), "screenshots/render.ppm");

		write_ppm(filePath, pixels, W, H);
		fprintf(stderr, "wrote %s (%dx%d)\n", filePath, W, H);
	}

	free(pixels);
	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();
	scene_free(&scene);
	return 0;
}
