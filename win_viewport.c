#include "scener.h"
#include <orion/user/draw.h>
#include <orion/user/gl_compat.h>

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
	float cam_yaw, cam_pitch;
	int last_mouse_x, last_mouse_y, orbiting;
} viewport_state_t;

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
	float yaw = vp->cam_yaw * M_PIf / 180.0f;
	float pitch = vp->cam_pitch * M_PIf / 180.0f;
	vec3 dir = v3(cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw));
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
		scene->camPos, scene->camLook, g_app->debug_flags);
}

static void vp_init_camera(viewport_state_t *vp, const Scene *scene) {
	vec3 dir = vsub(scene->camLook, scene->camPos);
	if (vlen(dir) < 0.001f) dir = v3(0, 0, -1);
	dir = vnorm(dir);
	vp->cam_yaw = atan2f(dir.x, dir.z) * 180.0f / M_PIf;
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
			if (ready) draw_sprite_region((int)vp->target.color, R(0, 0, cr.w, cr.h),
				UV_RECT(0, 1, 1, 0), 0xffffffff, DRAW_SPRITE_NO_ALPHA);
			else fill_rect(get_sys_color(brWorkspaceBg), cr);
			return true;
		}
		case evLeftButtonDown:
			if (vp) {
				vp->last_mouse_x = (int16_t)LOWORD(wparam);
				vp->last_mouse_y = (int16_t)HIWORD(wparam);
				if (doc) {
					irect16_t cr = get_client_rect(win);
					float nx = (float)vp->last_mouse_x / cr.w * 2.0f - 1.0f;
					float ny = 1.0f - (float)vp->last_mouse_y / cr.h * 2.0f;
					float fov = doc->scene.camFov > 0 ? doc->scene.camFov : 60;
					float tan_h = tanf(fov * M_PIf / 360.0f);
					mat4 view = mat4_lookat(doc->scene.camPos, doc->scene.camLook, v3(0, 1, 0));
					vec3 right = v3(view.m[0], view.m[4], view.m[8]);
					vec3 up = v3(view.m[1], view.m[5], view.m[9]);
					vec3 fwd = vnorm(vsub(doc->scene.camLook, doc->scene.camPos));
					vec3 ray = vnorm(vadd(vadd(fwd, vscale(right, nx * tan_h * (float)cr.w / cr.h)), vscale(up, ny * tan_h)));
					doc->scene.selectedObj = scene_pick_object(&doc->scene, doc->scene.camPos, ray, NULL);
					invalidate_window(win);
				}
			}
			return true;
		case evRightButtonDown:
			if (vp) {
				vp->orbiting = 1;
				vp->last_mouse_x = (int16_t)LOWORD(wparam);
				vp->last_mouse_y = (int16_t)HIWORD(wparam);
				set_capture(win);
			}
			return true;
		case evRightButtonUp:
			if (vp) { vp->orbiting = 0; set_capture(NULL); }
			return true;
		case evMouseMove: {
			if (!vp) return false;
			int mx = (int16_t)LOWORD(wparam), my = (int16_t)HIWORD(wparam);
			int dx = mx - vp->last_mouse_x, dy = my - vp->last_mouse_y;
			vp->last_mouse_x = mx;
			vp->last_mouse_y = my;
			if (vp->orbiting && doc) {
				vp->cam_yaw += dx * 0.25f;
				vp->cam_pitch -= dy * 0.25f;
				if (vp->cam_pitch > 89) vp->cam_pitch = 89;
				if (vp->cam_pitch < -89) vp->cam_pitch = -89;
				invalidate_window(win);
			}
			return true;
		}
		case evKeyDown: {
			if (!doc) return false;
			float speed = 0.25f;
			vec3 look = vnorm(vsub(doc->scene.camLook, doc->scene.camPos));
			vec3 right = vnorm(vcross(look, v3(0, 1, 0)));
			switch ((int)wparam) {
				case AX_KEY_W: doc->scene.camPos = vadd(doc->scene.camPos, vscale(look, speed)); break;
				case AX_KEY_S: doc->scene.camPos = vadd(doc->scene.camPos, vscale(look, -speed)); break;
				case AX_KEY_D: doc->scene.camPos = vadd(doc->scene.camPos, vscale(right, speed)); break;
				case AX_KEY_A: doc->scene.camPos = vadd(doc->scene.camPos, vscale(right, -speed)); break;
				case AX_KEY_E: doc->scene.camPos.y += speed; break;
				case AX_KEY_Q: doc->scene.camPos.y -= speed; break;
				default: return false;
			}
			invalidate_window(win);
			return true;
		}
		case evDestroy:
			if (vp) {
				render_texture_free(&vp->target);
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
