#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "simplegl.h"
#include "shader.h"

#define MOUSE_SENSITIVITY 0.25f
#define MOVE_SPEED 3.25f
#define SPRINT_SPEED 7.8f

int main(int argc,char**argv){
	const char *scenePath = NULL;
	const char *camName = NULL;
	int listCameras = 0;
	int renderFlags = 0;
	for(int i=1;i<argc;i++){
		if(!strcmp(argv[i],"-cam") && i+1<argc){ camName=argv[++i]; }
		else if(!strcmp(argv[i],"-list-cameras")){ listCameras = 1; }
		else if(!strcmp(argv[i],"-no-shadows")){ renderFlags|=DBG_NO_SHADOWS; }
		else if(!strcmp(argv[i],"-show-stencil")){ renderFlags|=DBG_SHOW_STENCIL; }
		else if(!scenePath) scenePath=argv[i];
	}
	if(!scenePath) scenePath="scenes/sample_room.xml";

	Scene scene;
	if(!load_scene(scenePath,&scene)) return 1;

	if(listCameras){
		for(int i=0;i<scene.ncameras;i++){
			Camera *c = &scene.cameras[i];
			fprintf(stderr,"%-16s %s%s%s\n", c->name,
			        c->comment[0] ? "\"" : "", c->comment,
			        c->comment[0] ? "\"" : "");
		}
		scene_free(&scene);
		return 0;
	}

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
    shader_init();
    SDL_GL_SetSwapInterval(1);

    vec3 fwd=vnorm(vsub(scene.camLook,scene.camPos));
    float yaw = atan2f(fwd.x,-fwd.z) * 180.0f/M_PIf;
    float pitch = asinf(fwd.y) * 180.0f/M_PIf;
    vec3 pos=scene.camPos;

    int running=1;
    int rightMouseDown=0;
    int debugFlags=renderFlags;
    Uint32 lastTicks=SDL_GetTicks();
    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT) running=0;
            else if(ev.type==SDL_KEYDOWN){
                if(ev.key.keysym.sym==SDLK_ESCAPE) running=0;
                else if(ev.key.keysym.sym==SDLK_1){ debugFlags^=DBG_NO_SHADOWS; fprintf(stderr,"shadows: %s\n",(debugFlags&DBG_NO_SHADOWS)?"off":"on"); }
                else if(ev.key.keysym.sym==SDLK_2){ debugFlags^=DBG_WIRE_SHADOWVOL; fprintf(stderr,"shadow volume wire: %s\n",(debugFlags&DBG_WIRE_SHADOWVOL)?"on":"off"); }
                else if(ev.key.keysym.sym==SDLK_3){ debugFlags^=DBG_SHOW_STENCIL; fprintf(stderr,"stencil overlay: %s\n",(debugFlags&DBG_SHOW_STENCIL)?"on":"off"); }
                else if(ev.key.keysym.sym==SDLK_4){ debugFlags^=DBG_HIDE_LIGHTS; fprintf(stderr,"cam/lamp dummies: %s\n",(debugFlags&DBG_HIDE_LIGHTS)?"hidden":"visible"); }
                else if(ev.key.keysym.sym==SDLK_5){ debugFlags^=DBG_HIDE_CHARS; fprintf(stderr,"character dummies: %s\n",(debugFlags&DBG_HIDE_CHARS)?"hidden":"visible"); }
            }
            else if(ev.type==SDL_MOUSEBUTTONDOWN && ev.button.button==SDL_BUTTON_RIGHT){
                rightMouseDown=1;
            } else if(ev.type==SDL_MOUSEBUTTONUP && ev.button.button==SDL_BUTTON_RIGHT){
                rightMouseDown=0;
            } else if(ev.type==SDL_MOUSEMOTION && rightMouseDown){
                yaw   += ev.motion.xrel * MOUSE_SENSITIVITY;
                pitch -= ev.motion.yrel * MOUSE_SENSITIVITY;
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
        float speed = (ks[SDL_SCANCODE_LSHIFT]||ks[SDL_SCANCODE_RSHIFT]) ? SPRINT_SPEED : MOVE_SPEED;
        vec3 move={0,0,0};
        if(ks[SDL_SCANCODE_W]) move=vadd(move, look);
        if(ks[SDL_SCANCODE_S]) move=vsub(move, look);
        if(ks[SDL_SCANCODE_D]) move=vadd(move, right);
        if(ks[SDL_SCANCODE_A]) move=vsub(move, right);
        if(ks[SDL_SCANCODE_E]) move=vadd(move, v3(0,1,0));
        if(ks[SDL_SCANCODE_Q]) move=vsub(move, v3(0,1,0));
        if(vlen(move)>1e-6f) pos = vadd(pos, vscale(vnorm(move), speed*dt));

        mat4 proj = mat4_perspective(scene.camFov, (float)W/(float)H, 0.1f, 100.0f);
        mat4 view = mat4_lookat(pos, vadd(pos,look), v3(0,1,0));
        render_frame(&scene, W,H, proj, view, pos, debugFlags);

        SDL_GL_SwapWindow(win);
    }

    shader_deinit();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    scene_free(&scene);
    return 0;
}
