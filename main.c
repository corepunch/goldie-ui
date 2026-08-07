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

static int camera_index(Scene *scene,const char *name){
	if(!name) return 0;
	for(int i=0;i<scene->ncameras;i++) if(!strcmp(scene->cameras[i].name,name)) return i;
	return 0;
}

static void snap_camera(Scene *scene,int index,vec3 *pos,float *yaw,float *pitch){
	Camera *camera=&scene->cameras[index];
	vec3 fwd=vnorm(vsub(camera->look,camera->pos));
	scene->camPos=camera->pos;
	scene->camLook=camera->look;
	scene->camFov=camera->fov;
	*pos=camera->pos;
	*yaw=atan2f(fwd.x,-fwd.z)*180.0f/M_PIf;
	*pitch=asinf(fwd.y)*180.0f/M_PIf;
}

static void set_camera_title(SDL_Window *win,Scene *scene,int index){
	char title[128];
	snprintf(title,sizeof(title),"simplegl - camera %d/%d: %s",index+1,scene->ncameras,scene->cameras[index].name);
	SDL_SetWindowTitle(win,title);
}

int main(int argc,char**argv){
	const char *scenePath = NULL;
	const char *camName = NULL;
	int listCameras = 0;
	int runSanity = 0;
	int renderFlags = 0;
	for(int i=1;i<argc;i++){
		if(!strcmp(argv[i],"-cam") && i+1<argc){ camName=argv[++i]; }
		else if(!strcmp(argv[i],"-list-cameras")){ listCameras = 1; }
		else if(!strcmp(argv[i],"-test")){ runSanity = 1; }
		else if(!strcmp(argv[i],"-no-shadows")){ renderFlags|=DBG_NO_SHADOWS; }
		else if(!strcmp(argv[i],"-show-stencil")){ renderFlags|=DBG_SHOW_STENCIL; }
		else if(!scenePath) scenePath=argv[i];
	}
	if(!scenePath) scenePath="scenes/sample_room.blks";

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
	if(runSanity){
		int ok=scene_sanity_check(&scene);
		scene_free(&scene);
		return ok ? 0 : 1;
	}

	int currentCamera=camera_index(&scene,camName);
	scene_select_camera(&scene,scene.cameras[currentCamera].name);
	scene_build_all_shadow_volumes(&scene);
	fprintf(stderr,"loaded %d objects, %d lights, %d materials, %d cameras\n",
	        scene.nobjs, scene.nlights, scene.nmats, scene.ncameras);

    if(SDL_Init(SDL_INIT_VIDEO)!=0){ fprintf(stderr,"SDL_Init: %s\n",SDL_GetError()); return 1; }
    SDL_StopTextInput();
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);

    int W=1024,H=768;
    SDL_Window *win=SDL_CreateWindow("simplegl",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        W,H, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!win){ fprintf(stderr,"CreateWindow: %s\n",SDL_GetError()); return 1; }
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    if(!ctx){ fprintf(stderr,"GL_CreateContext: %s\n",SDL_GetError()); return 1; }
    set_camera_title(win,&scene,currentCamera);
    shader_init();
    SDL_GL_SetSwapInterval(1);
    scene_rebuild_camera_gizmos(&scene,(float)W/(float)H);

    float yaw,pitch;
    vec3 pos;
    snap_camera(&scene,currentCamera,&pos,&yaw,&pitch);

    int running=1;
    int rightMouseDown=0;
	int tabDown=0;
    int debugFlags=renderFlags;
    scene.selectedObj=-1;
    scene.editMode=EDIT_W_MOVE;
    scene.hoveredHandle=GIZMO_NONE;
    scene.draggingHandle=GIZMO_NONE;
    int leftMouseDown=0;
    int mouseX=W/2,mouseY=H/2;
    Uint32 lastTicks=SDL_GetTicks();
    while(running){
        Uint32 now=SDL_GetTicks(); float dt=(now-lastTicks)/1000.0f; lastTicks=now;
        float yawR=yaw*M_PIf/180.0f, pitchR=pitch*M_PIf/180.0f;
        vec3 look = v3(sinf(yawR)*cosf(pitchR), sinf(pitchR), -cosf(yawR)*cosf(pitchR));
        vec3 right = vnorm(vcross(look, v3(0,1,0)));

        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT) running=0;
            else if(ev.type==SDL_KEYDOWN){
                if(ev.key.keysym.sym==SDLK_ESCAPE) running=0;
				else if(ev.key.keysym.sym==SDLK_TAB && !tabDown){
					tabDown=1;
					if(ev.key.keysym.mod&KMOD_SHIFT) currentCamera=(currentCamera+scene.ncameras-1)%scene.ncameras;
					else currentCamera=(currentCamera+1)%scene.ncameras;
					snap_camera(&scene,currentCamera,&pos,&yaw,&pitch);
					set_camera_title(win,&scene,currentCamera);
					fprintf(stderr,"camera %d/%d: %s\n",currentCamera+1,scene.ncameras,scene.cameras[currentCamera].name);
				}
                else if(ev.key.keysym.sym==SDLK_1){ debugFlags^=DBG_NO_SHADOWS; fprintf(stderr,"shadows: %s\n",(debugFlags&DBG_NO_SHADOWS)?"off":"on"); }
                else if(ev.key.keysym.sym==SDLK_2){ debugFlags^=DBG_WIRE_SHADOWVOL; fprintf(stderr,"shadow volume wire: %s\n",(debugFlags&DBG_WIRE_SHADOWVOL)?"on":"off"); }
                else if(ev.key.keysym.sym==SDLK_3){ debugFlags^=DBG_SHOW_STENCIL; fprintf(stderr,"stencil overlay: %s\n",(debugFlags&DBG_SHOW_STENCIL)?"on":"off"); }
                else if(ev.key.keysym.sym==SDLK_4){ debugFlags^=DBG_HIDE_LIGHTS; fprintf(stderr,"cam/lamp dummies: %s\n",(debugFlags&DBG_HIDE_LIGHTS)?"hidden":"visible"); }
                else if(ev.key.keysym.sym==SDLK_5){ debugFlags^=DBG_HIDE_CHARS; fprintf(stderr,"character dummies: %s\n",(debugFlags&DBG_HIDE_CHARS)?"hidden":"visible"); }
                else if(!rightMouseDown && ev.key.keysym.sym==SDLK_q){ scene.editMode=EDIT_Q_SELECT; fprintf(stderr,"edit mode: select\n"); }
                else if(!rightMouseDown && ev.key.keysym.sym==SDLK_w){ scene.editMode=EDIT_W_MOVE; fprintf(stderr,"edit mode: move\n"); }
                else if(!rightMouseDown && ev.key.keysym.sym==SDLK_e){ scene.editMode=EDIT_E_ROTATE; fprintf(stderr,"edit mode: rotate\n"); }
                else if(!rightMouseDown && ev.key.keysym.sym==SDLK_r){ scene.editMode=EDIT_R_SCALE; fprintf(stderr,"edit mode: scale\n"); }
            }
			else if(ev.type==SDL_KEYUP && ev.key.keysym.sym==SDLK_TAB) tabDown=0;
            else if(ev.type==SDL_MOUSEBUTTONDOWN && ev.button.button==SDL_BUTTON_RIGHT){
                rightMouseDown=1;
            } else if(ev.type==SDL_MOUSEBUTTONUP && ev.button.button==SDL_BUTTON_RIGHT){
                rightMouseDown=0;
            } else if(ev.type==SDL_MOUSEBUTTONDOWN && ev.button.button==SDL_BUTTON_LEFT){
                leftMouseDown=1;
                float fovRad=scene.camFov*M_PIf/180.0f;
                float hh=tanf(fovRad*0.5f);
                float hw=hh*(float)W/(float)H;
                float ndcX=(2.0f*mouseX)/W-1.0f;
                float ndcY=1.0f-(2.0f*mouseY)/H;
                vec3 cameraUp=vnorm(vcross(right,look));
                vec3 rayDir=vnorm(vadd(vadd(vscale(right,ndcX*hw),vscale(cameraUp,ndcY*hh)),look));
                /* Start gizmo drag if hovering a handle */
                if(scene.hoveredHandle!=GIZMO_NONE){
                    scene.draggingHandle=scene.hoveredHandle;
                    scene.dragStartMouseX=mouseX;
                    scene.dragStartMouseY=mouseY;
                    scene.dragPrevAnchor=v3(1e30f,1e30f,1e30f);
                    vec3 bmin,bmax;
                    scene_get_obj_bounds(&scene,scene.selectedObj,&bmin,&bmax);
                    scene.dragStartCenter=vscale(vadd(bmin,bmax),0.5f);
                } else {
                    scene.draggingHandle=GIZMO_NONE;
                    scene.selectedObj=scene_pick_object(&scene,pos,rayDir,NULL);
                    fprintf(stderr,"selected object %d\n",scene.selectedObj);
                }
            } else if(ev.type==SDL_MOUSEBUTTONUP && ev.button.button==SDL_BUTTON_LEFT){
                leftMouseDown=0;
                scene.draggingHandle=GIZMO_NONE;
            } else if(ev.type==SDL_MOUSEMOTION){
                mouseX=ev.motion.x; mouseY=ev.motion.y;
                if(rightMouseDown){
                    yaw   += ev.motion.xrel * MOUSE_SENSITIVITY;
                    pitch -= ev.motion.yrel * MOUSE_SENSITIVITY;
                    if(pitch>89) pitch=89;
                    if(pitch<-89) pitch=-89;
                } else if(leftMouseDown && scene.draggingHandle!=GIZMO_NONE){
                    vec3 cameraUp=vnorm(vcross(right,look));
                    gizmo_apply_drag(&scene, mouseX, mouseY, W, H,
                                     pos, right, cameraUp, look, scene.camFov);
                }
            } else if(ev.type==SDL_WINDOWEVENT && ev.window.event==SDL_WINDOWEVENT_RESIZED){
                W=ev.window.data1; H=ev.window.data2;
                scene_rebuild_camera_gizmos(&scene,(float)W/(float)H);
            }
        }
        const Uint8 *ks=SDL_GetKeyboardState(NULL);
        float speed = (ks[SDL_SCANCODE_LSHIFT]||ks[SDL_SCANCODE_RSHIFT]) ? SPRINT_SPEED : MOVE_SPEED;
        vec3 move={0,0,0};
        if(rightMouseDown){
            if(ks[SDL_SCANCODE_W]) move=vadd(move, look);
            if(ks[SDL_SCANCODE_S]) move=vsub(move, look);
            if(ks[SDL_SCANCODE_D]) move=vadd(move, right);
            if(ks[SDL_SCANCODE_A]) move=vsub(move, right);
            if(ks[SDL_SCANCODE_E]) move=vadd(move, v3(0,1,0));
            if(ks[SDL_SCANCODE_Q]) move=vsub(move, v3(0,1,0));
        }
        if(vlen(move)>1e-6f) pos = vadd(pos, vscale(vnorm(move), speed*dt));

        /* Per-frame gizmo hover detection (only when not dragging) */
        if(scene.draggingHandle==GIZMO_NONE){
            float fovRad=scene.camFov*M_PIf/180.0f;
            float hh=tanf(fovRad*0.5f);
            float hw=hh*(float)W/(float)H;
            float ndcX=(2.0f*mouseX)/(float)W-1.0f;
            float ndcY=1.0f-(2.0f*mouseY)/(float)H;
            vec3 cameraUp=vnorm(vcross(right,look));
            vec3 rayDir=vnorm(vadd(vadd(vscale(right,ndcX*hw),vscale(cameraUp,ndcY*hh)),look));
            scene.hoveredHandle=gizmo_pick_handle(&scene,pos,rayDir);
        }

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
