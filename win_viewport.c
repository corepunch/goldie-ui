#include "scener.h"
#include <orion/user/draw.h>
#include <orion/user/gl_compat.h>

#ifndef SCENER_DEBUG
#define SCENER_DEBUG 0
#endif
#define VP_LOG(...) do { if (SCENER_DEBUG) fprintf(stderr, "scener viewport: " __VA_ARGS__); } while (0)

typedef struct {
	uint32_t fbo, color, depth;
	int width, height;
} render_texture_t;

typedef struct {
	GLint fbo, viewport[4], scissor[4], program, vao, array_buffer;
	GLint active_texture, texture, depth_func;
	GLboolean blend, depth_test, stencil_test, cull_face, scissor_test, framebuffer_srgb;
	GLboolean depth_mask, color_mask[4];
} viewport_gl_state_t;

typedef struct {
	render_texture_t target;
	uint32_t present_program;
	float cam_yaw, cam_pitch;
	int last_mouse_x, last_mouse_y, orbiting, left_down;
	uint32_t navigation_timer;
	longTime_t last_move_time;
} viewport_state_t;

static const char *vp_present_vs =
	"#version 150 core\n"
	"in vec2 position; in vec2 texcoord; in vec4 color;\n"
	"out vec2 tex; out vec4 col;\n"
	"uniform mat4 projection; uniform vec2 offset,scale,uv_offset,uv_scale;\n"
	"void main(){ col=color; tex=texcoord*uv_scale+uv_offset; gl_Position=projection*vec4(position*scale+offset,0,1); }\n";
static const char *vp_present_fs =
	"#version 150 core\n"
	"in vec2 tex; in vec4 col; out vec4 outColor; uniform sampler2D tex0; uniform vec4 tint; uniform float alpha;\n"
	"void main(){ vec4 s=texture(tex0,vec2(tex.x,1.0-tex.y))*col*tint; vec3 lo=12.92*s.rgb; vec3 hi=1.055*pow(s.rgb,vec3(1.0/2.4))-0.055; outColor=vec4(mix(lo,hi,step(vec3(0.0031308),s.rgb)),s.a*alpha); }\n";

static GLuint vp_compile_shader(GLenum type, const char *source) {
	GLuint shader=glCreateShader(type); glShaderSource(shader,1,&source,NULL); glCompileShader(shader);
	GLint ok=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
	if(!ok){ char log[1024]; glGetShaderInfoLog(shader,sizeof(log),NULL,log); fprintf(stderr,"viewport shader: %s\n",log); glDeleteShader(shader); return 0; }
	return shader;
}

static GLuint vp_create_present_program(void) {
	GLuint vs=vp_compile_shader(GL_VERTEX_SHADER,vp_present_vs), fs=vp_compile_shader(GL_FRAGMENT_SHADER,vp_present_fs);
	if(!vs||!fs){ if(vs) glDeleteShader(vs); if(fs) glDeleteShader(fs); return 0; }
	GLuint program=glCreateProgram(); glAttachShader(program,vs); glAttachShader(program,fs);
	glBindAttribLocation(program,0,"position"); glBindAttribLocation(program,1,"texcoord"); glBindAttribLocation(program,2,"color");
	glLinkProgram(program); glDeleteShader(vs); glDeleteShader(fs);
	GLint ok=0; glGetProgramiv(program,GL_LINK_STATUS,&ok);
	if(!ok){ char log[1024]; glGetProgramInfoLog(program,sizeof(log),NULL,log); fprintf(stderr,"viewport program: %s\n",log); glDeleteProgram(program); return 0; }
	return program;
}

static vec3 vp_camera_dir(const viewport_state_t *vp) {
	float yaw = vp->cam_yaw * M_PIf / 180.0f, pitch = vp->cam_pitch * M_PIf / 180.0f;
	return v3(cosf(pitch) * sinf(yaw), sinf(pitch), -cosf(pitch) * cosf(yaw));
}

static vec3 vp_mouse_ray(const viewport_state_t *vp, const Scene *scene, int x, int y, int w, int h) {
	vec3 fwd = vp_camera_dir(vp), right = vnorm(vcross(fwd, v3(0, 1, 0))), up = vnorm(vcross(right, fwd));
	float tan_h = tanf((scene->camFov > 0 ? scene->camFov : 60) * M_PIf / 360.0f);
	float nx = (float)x / w * 2.0f - 1.0f, ny = 1.0f - (float)y / h * 2.0f;
	return vnorm(vadd(vadd(fwd, vscale(right, nx * tan_h * (float)w / h)), vscale(up, ny * tan_h)));
}

static void vp_stop_navigation(window_t *win, viewport_state_t *vp, bool restore_focus) {
	if (!vp) return;
	vp->orbiting = 0;
	if (vp->navigation_timer) axCancelTimer(vp->navigation_timer);
	vp->navigation_timer = 0;
	if (g_app) g_app->viewport_navigating = false;
	set_capture(NULL);
	if (restore_focus && win->parent) set_focus(win->parent);
}

static bool vp_move_camera(viewport_state_t *vp, scene_doc_t *doc) {
	longTime_t now = axGetMilliseconds();
	float dt = (float)(now - vp->last_move_time) / 1000.0f;
	vp->last_move_time = now;
	if (dt <= 0 || dt > 0.1f) dt = 0.016f;
	vec3 look = vp_camera_dir(vp), right = vnorm(vcross(look, v3(0, 1, 0))), move = v3(0, 0, 0);
	if (ui_is_key_down(AX_KEY_W)) move = vadd(move, look);
	if (ui_is_key_down(AX_KEY_S)) move = vsub(move, look);
	if (ui_is_key_down(AX_KEY_D)) move = vadd(move, right);
	if (ui_is_key_down(AX_KEY_A)) move = vsub(move, right);
	if (ui_is_key_down(AX_KEY_E)) move = vadd(move, v3(0, 1, 0));
	if (ui_is_key_down(AX_KEY_Q)) move = vsub(move, v3(0, 1, 0));
	if (vlen(move) <= 1e-6f) return false;
	float speed = ui_is_key_down(AX_KEY_SHIFT) ? 7.8f : 3.25f;
	doc->scene.camPos = vadd(doc->scene.camPos, vscale(vnorm(move), speed * dt));
	doc->scene.camLook = vadd(doc->scene.camPos, look);
	return true;
}

static void gl_flag(GLenum flag, GLboolean enabled) {
	if (enabled) glEnable(flag); else glDisable(flag);
}

static void vp_save_gl(viewport_gl_state_t *state) {
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &state->fbo);
	glGetIntegerv(GL_VIEWPORT, state->viewport);
	glGetIntegerv(GL_SCISSOR_BOX, state->scissor);
	glGetIntegerv(GL_CURRENT_PROGRAM, &state->program);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state->vao);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state->array_buffer);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &state->active_texture);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &state->texture);
	glGetIntegerv(GL_DEPTH_FUNC, &state->depth_func);
	state->blend = glIsEnabled(GL_BLEND);
	state->depth_test = glIsEnabled(GL_DEPTH_TEST);
	state->stencil_test = glIsEnabled(GL_STENCIL_TEST);
	state->cull_face = glIsEnabled(GL_CULL_FACE);
	state->scissor_test = glIsEnabled(GL_SCISSOR_TEST);
	state->framebuffer_srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &state->depth_mask);
	glGetBooleanv(GL_COLOR_WRITEMASK, state->color_mask);
}

static void vp_restore_gl(const viewport_gl_state_t *state) {
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)state->fbo);
	glViewport(state->viewport[0], state->viewport[1], state->viewport[2], state->viewport[3]);
	glScissor(state->scissor[0], state->scissor[1], state->scissor[2], state->scissor[3]);
	glUseProgram((GLuint)state->program);
	glBindVertexArray((GLuint)state->vao);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint)state->array_buffer);
	glActiveTexture((GLenum)state->active_texture);
	glBindTexture(GL_TEXTURE_2D, (GLuint)state->texture);
	glDepthFunc((GLenum)state->depth_func);
	glDepthMask(state->depth_mask);
	glColorMask(state->color_mask[0], state->color_mask[1], state->color_mask[2], state->color_mask[3]);
	gl_flag(GL_BLEND, state->blend);
	gl_flag(GL_DEPTH_TEST, state->depth_test);
	gl_flag(GL_STENCIL_TEST, state->stencil_test);
	gl_flag(GL_CULL_FACE, state->cull_face);
	gl_flag(GL_SCISSOR_TEST, state->scissor_test);
	gl_flag(GL_FRAMEBUFFER_SRGB, state->framebuffer_srgb);
}

static void render_texture_free(render_texture_t *target) {
	if (target->fbo) glDeleteFramebuffers(1, &target->fbo);
	if (target->color) glDeleteTextures(1, &target->color);
	if (target->depth) glDeleteRenderbuffers(1, &target->depth);
	memset(target, 0, sizeof(*target));
}

static bool render_texture_resize(render_texture_t *target, int width, int height) {
	if (width <= 0 || height <= 0) return false;
	if (target->fbo && target->width == width && target->height == height) return true;
	render_texture_free(target);
	glGenFramebuffers(1, &target->fbo);
	glGenTextures(1, &target->color);
	glGenRenderbuffers(1, &target->depth);
	glBindTexture(GL_TEXTURE_2D, target->color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindRenderbuffer(GL_RENDERBUFFER, target->depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, target->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->color, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, target->depth);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		render_texture_free(target);
		return false;
	}
	target->width = width;
	target->height = height;
	return true;
}

static void vp_render(viewport_state_t *vp, scene_doc_t *doc) {
	render_texture_t *target = &vp->target;
	Scene *scene = &doc->scene;
	vec3 dir = vp_camera_dir(vp);
	scene->camLook = vadd(scene->camPos, dir);
	mat4 proj = mat4_perspective(scene->camFov > 0 ? scene->camFov : 60,
		(float)target->width / target->height, 0.1f, 1000.0f);
	mat4 view = mat4_lookat(scene->camPos, scene->camLook, v3(0, 1, 0));
	glBindFramebuffer(GL_FRAMEBUFFER, target->fbo);
	glViewport(0, 0, target->width, target->height);
	glScissor(0, 0, target->width, target->height);
	glEnable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	render_frame(scene, target->width, target->height, proj, view,
		scene->camPos, dir, g_app->debug_flags);
}

static void vp_init_camera(viewport_state_t *vp, const Scene *scene) {
	vec3 dir = vsub(scene->camLook, scene->camPos);
	if (vlen(dir) < 0.001f) dir = v3(0, 0, -1);
	dir = vnorm(dir);
	vp->cam_yaw = atan2f(dir.x, -dir.z) * 180.0f / M_PIf;
	vp->cam_pitch = asinf(dir.y) * 180.0f / M_PIf;
}

result_t win_viewport(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
	viewport_state_t *vp = (viewport_state_t *)win->userdata;
	scene_doc_t *doc = (scene_doc_t *)win->userdata2;
	switch (msg) {
		case evCreate:
			vp = calloc(1, sizeof(*vp));
			if (!vp) return false;
			win->userdata = vp;
			win->userdata2 = lparam;
			vp->present_program = vp_create_present_program();
			doc = (scene_doc_t *)lparam;
			if (doc) vp_init_camera(vp, &doc->scene);
			return true;
		case evPaint: {
			if (!vp || !doc) return false;
			irect16_t cr = get_client_rect(win);
			if (cr.w <= 0 || cr.h <= 0) return true;
			viewport_gl_state_t state;
			vp_save_gl(&state);
			bool ready = render_texture_resize(&vp->target, cr.w, cr.h);
			if (ready) vp_render(vp, doc);
			vp_restore_gl(&state);
			if (ready && vp->present_program)
				draw_rect_program((int)vp->target.color,0,0,cr.w,cr.h,vp->present_program,0);
			else fill_rect(get_sys_color(brWorkspaceBg), cr);
			return true;
		}
		case evLeftButtonDown:
			if (vp) {
				vp->last_mouse_x = (int16_t)LOWORD(wparam);
				vp->last_mouse_y = (int16_t)HIWORD(wparam);
				if (doc) {
					irect16_t cr = get_client_rect(win);
					if (cr.w <= 0 || cr.h <= 0) return true;
					vec3 ray = vp_mouse_ray(vp, &doc->scene, vp->last_mouse_x, vp->last_mouse_y, cr.w, cr.h);
					vp->left_down = 1;
					if (doc->scene.hoveredHandle != GIZMO_NONE)
						gizmo_begin_drag(&doc->scene, doc->scene.hoveredHandle, vp->last_mouse_x, vp->last_mouse_y);
					else {
						doc->scene.draggingHandle = GIZMO_NONE;
						float distance = 0;
						doc->scene.selectedObj = scene_pick_object(&doc->scene, doc->scene.camPos, ray, &distance);
						VP_LOG("pick mouse=(%d,%d) size=(%d,%d) cam=(%.3f,%.3f,%.3f) ray=(%.3f,%.3f,%.3f) hit=%d distance=%.3f objects=%d\n",
							vp->last_mouse_x, vp->last_mouse_y, cr.w, cr.h,
							doc->scene.camPos.x, doc->scene.camPos.y, doc->scene.camPos.z,
							ray.x, ray.y, ray.z, doc->scene.selectedObj, distance, doc->scene.nobjs);
					}
					set_capture(win);
					invalidate_window(win);
				}
			}
			return true;
		case evLeftButtonUp:
			if (vp) vp->left_down = 0;
			if (doc) doc->scene.draggingHandle = GIZMO_NONE;
			set_capture(NULL);
			return true;
		case evRightButtonDown:
			if (vp) {
				vp->orbiting = 1;
				vp->last_mouse_x = (int16_t)LOWORD(wparam);
				vp->last_mouse_y = (int16_t)HIWORD(wparam);
				g_app->viewport_navigating = true;
				set_focus(win);
				set_capture(win);
				vp->last_move_time = axGetMilliseconds();
				if (!vp->navigation_timer) vp->navigation_timer = axSetTimer(win, 16, NULL, true);
			}
			return true;
		case evRightButtonUp:
			vp_stop_navigation(win, vp, true);
			return true;
		case evKillFocus:
			if (vp && vp->orbiting) vp_stop_navigation(win, vp, false);
			return false;
		case evMouseMove: {
			if (!vp) return false;
			int mx = (int16_t)LOWORD(wparam), my = (int16_t)HIWORD(wparam);
			int dx = lparam ? (int16_t)LOWORD((uintptr_t)lparam) : mx - vp->last_mouse_x;
			int dy = lparam ? (int16_t)HIWORD((uintptr_t)lparam) : my - vp->last_mouse_y;
			vp->last_mouse_x = mx;
			vp->last_mouse_y = my;
			if (vp->orbiting && doc) {
				vp->cam_yaw += dx * 0.08f;
				vp->cam_pitch -= dy * 0.08f;
				if (vp->cam_pitch > 89) vp->cam_pitch = 89;
				if (vp->cam_pitch < -89) vp->cam_pitch = -89;
				invalidate_window(win);
			} else if (doc) {
				irect16_t cr = get_client_rect(win);
				if (cr.w <= 0 || cr.h <= 0) return true;
				vec3 fwd = vp_camera_dir(vp), right = vnorm(vcross(fwd, v3(0, 1, 0))), up = vnorm(vcross(right, fwd));
				vec3 ray = vp_mouse_ray(vp, &doc->scene, mx, my, cr.w, cr.h);
				if (vp->left_down && doc->scene.draggingHandle != GIZMO_NONE)
					gizmo_apply_drag(&doc->scene, mx, my, cr.w, cr.h, doc->scene.camPos, right, up, fwd, doc->scene.camFov);
				else {
					int hovered = gizmo_pick_handle(&doc->scene, doc->scene.camPos, ray, fwd, doc->scene.camFov);
					if (hovered == doc->scene.hoveredHandle) return true;
					doc->scene.hoveredHandle = hovered;
				}
				invalidate_window(win);
			}
			return true;
		}
		case evKeyDown:
		case evKeyUp:
			if (!vp || !vp->orbiting) return false;
			return wparam == AX_KEY_W || wparam == AX_KEY_S || wparam == AX_KEY_A ||
				wparam == AX_KEY_D || wparam == AX_KEY_E || wparam == AX_KEY_Q || wparam == AX_KEY_SHIFT;
		case evTimer:
			if (!vp || !doc || wparam != vp->navigation_timer) return false;
			if (vp_move_camera(vp, doc)) invalidate_window(win);
			return true;
		case evDestroy:
			if (vp) {
				if (vp->orbiting) vp_stop_navigation(win, vp, false);
				render_texture_free(&vp->target);
				if(vp->present_program) glDeleteProgram(vp->present_program);
				free(vp);
				win->userdata = NULL;
			}
			return false;
		default:
			return false;
	}
}

window_t *create_viewport_window(window_t *parent, scene_doc_t *doc) {
	irect16_t cr = get_client_rect(parent);
	window_t *win = create_window("Viewport", WINDOW_NOTITLE | WINDOW_NOFILL,
		MAKERECT(0, 0, cr.w, cr.h), parent, win_viewport, 0, doc);
	if (win) show_window(win, true);
	return win;
}
