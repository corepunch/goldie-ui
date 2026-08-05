#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "math.h"
#include "scene.h"
#include "shadow.h"
#include "render.h"

int main(int argc,char**argv){
	const char *scenePath = NULL;
	const char *camName = NULL;
	for(int i=1;i<argc;i++){
		if(!strcmp(argv[i],"-cam") && i+1<argc){ camName=argv[++i]; }
		else if(!scenePath) scenePath=argv[i];
	}
	if(!scenePath) scenePath="scene.xml";

	Scene scene;
	if(!load_scene(scenePath,&scene)) return 1;
	if(camName) scene_select_camera(&scene,camName);
	scene_build_all_shadow_volumes(&scene);
	fprintf(stderr,"loaded %d objects, %d lights, %d materials, %d cameras\n",
	        scene.nobjs, scene.nlights, scene.nmats, scene.ncameras);

    if(SDL_Init(SDL_INIT_VIDEO)!=0){ fprintf(stderr,"SDL_Init: %s\n",SDL_GetError()); return 1; }
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);

    int W=1280,H=800;
    SDL_Window *win=SDL_CreateWindow("simplegl",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        W,H, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!win){ fprintf(stderr,"CreateWindow: %s\n",SDL_GetError()); return 1; }
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    if(!ctx){ fprintf(stderr,"GL_CreateContext: %s\n",SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    vec3 fwd=vnorm(vsub(scene.camLook,scene.camPos));
    float yaw = atan2f(fwd.x,-fwd.z) * 180.0f/M_PIf;
    float pitch = asinf(fwd.y) * 180.0f/M_PIf;
    vec3 pos=scene.camPos;

    int running=1;
    int debugMode=DBG_NONE;
    Uint32 lastTicks=SDL_GetTicks();
    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT) running=0;
            else if(ev.type==SDL_KEYDOWN){
                if(ev.key.keysym.sym==SDLK_ESCAPE) running=0;
                else if(ev.key.keysym.sym==SDLK_1){ debugMode=DBG_NONE; fprintf(stderr,"debug: normal\n"); }
                else if(ev.key.keysym.sym==SDLK_2){ debugMode=DBG_WIRE_SHADOWVOL; fprintf(stderr,"debug: wireframe shadow vols (red)\n"); }
                else if(ev.key.keysym.sym==SDLK_3){ debugMode=DBG_SHOW_STENCIL; fprintf(stderr,"debug: stencil != 0 (red overlay)\n"); }
            }
            else if(ev.type==SDL_MOUSEMOTION){
                yaw   += ev.motion.xrel * 0.15f;
                pitch -= ev.motion.yrel * 0.15f;
                if(pitch>89) pitch=89;
                if(pitch<-89) pitch=-89;
            } else if(ev.type==SDL_WINDOWEVENT && ev.window.event==SDL_WINDOWEVENT_RESIZED){
                W=ev.window.data1; H=ev.window.data2;
            }
        }
        Uint32 now=SDL_GetTicks(); float dt=(now-lastTicks)/1000.0f; lastTicks=now;

        float yawR=yaw*M_PIf/180.0f, pitchR=pitch*M_PIf/180.0f;
        vec3 look = v3(sinf(yawR)*cosf(pitchR), sinf(pitchR), -cosf(yawR)*cosf(pitchR));
        vec3 right = vnorm(vcross(look, v3(0,1,0)));

        const Uint8 *ks=SDL_GetKeyboardState(NULL);
        float speed = (ks[SDL_SCANCODE_LSHIFT]||ks[SDL_SCANCODE_RSHIFT]) ? 6.0f : 2.5f;
        vec3 move={0,0,0};
        if(ks[SDL_SCANCODE_W]) move=vadd(move, look);
        if(ks[SDL_SCANCODE_S]) move=vsub(move, look);
        if(ks[SDL_SCANCODE_D]) move=vadd(move, right);
        if(ks[SDL_SCANCODE_A]) move=vsub(move, right);
        if(ks[SDL_SCANCODE_E]) move=vadd(move, v3(0,1,0));
        if(ks[SDL_SCANCODE_Q]) move=vsub(move, v3(0,1,0));
        if(vlen(move)>1e-6f) pos = vadd(pos, vscale(vnorm(move), speed*dt));

        mat4 proj = mat4_perspective(scene.camFov, (float)W/(float)H, 0.05f, 1000.0f);
        mat4 view = mat4_lookat(pos, vadd(pos,look), v3(0,1,0));
        render_frame(&scene, W,H, proj, view, debugMode);

        SDL_GL_SwapWindow(win);
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    scene_free(&scene);
    return 0;
}
