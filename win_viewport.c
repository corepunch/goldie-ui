#include "scener.h"
#include <orion/user/gl_compat.h>

enum {
  VP_TIMER_ID = 1,
};

typedef struct {
  uint32_t fbo;
  uint32_t color_tex;
  uint32_t depth_rbo;
  int      tex_w;
  int      tex_h;
  bool     gl_initialized;
  float    cam_yaw;
  float    cam_pitch;
  int      last_mouse_x;
  int      last_mouse_y;
  int      orbiting;
} viewport_state_t;

static void vp_create_fbo(viewport_state_t *vp, int w, int h) {
  if (w <= 0 || h <= 0) return;
  if (vp->tex_w == w && vp->tex_h == h && vp->fbo) return;

  if (vp->fbo)         glDeleteFramebuffers(1, &vp->fbo);
  if (vp->color_tex)   glDeleteTextures(1, &vp->color_tex);
  if (vp->depth_rbo)   glDeleteRenderbuffers(1, &vp->depth_rbo);

  glGenFramebuffers(1, &vp->fbo);
  glGenTextures(1, &vp->color_tex);
  glGenRenderbuffers(1, &vp->depth_rbo);

  glBindTexture(GL_TEXTURE_2D, vp->color_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindRenderbuffer(GL_RENDERBUFFER, vp->depth_rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

  glBindFramebuffer(GL_FRAMEBUFFER, vp->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vp->color_tex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, vp->depth_rbo);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  vp->tex_w = w;
  vp->tex_h = h;
}

static void vp_render_scene(viewport_state_t *vp, scene_doc_t *doc) {
  if (!doc || !vp->fbo || vp->tex_w <= 0 || vp->tex_h <= 0) return;

  Scene *s = &doc->scene;
  float aspect = (float)vp->tex_w / (float)vp->tex_h;

  vec3 look = vsub(s->camLook, s->camPos);
  float dist = vlen(look);
  if (dist < 0.001f) look = v3(0, 0, -1);
  else look = vnorm(look);

  float yaw_rad = vp->cam_yaw * M_PIf / 180.0f;
  float pitch_rad = vp->cam_pitch * M_PIf / 180.0f;
  vec3 dir;
  dir.x = cosf(pitch_rad) * sinf(yaw_rad);
  dir.y = sinf(pitch_rad);
  dir.z = cosf(pitch_rad) * cosf(yaw_rad);

  s->camLook = vadd(s->camPos, dir);

  mat4 proj = mat4_perspective(s->camFov > 0 ? s->camFov : 60, aspect, 0.1f, 1000.0f);
  mat4 view = mat4_lookat(s->camPos, s->camLook, v3(0, 1, 0));

  GLint prev_fbo;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, vp->fbo);
  glViewport(0, 0, vp->tex_w, vp->tex_h);

  render_frame(s, vp->tex_w, vp->tex_h, proj, view, s->camPos, s->camLook, g_app->debug_flags);

  glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
}

result_t win_viewport(window_t *win, uint32_t msg,
                      uint32_t wparam, void *lparam) {
  viewport_state_t *vp = (viewport_state_t *)win->userdata;
  scene_doc_t *doc = (scene_doc_t *)win->userdata2;

  switch (msg) {
    case evCreate:
      vp = calloc(1, sizeof(viewport_state_t));
      win->userdata = vp;
      win->userdata2 = lparam;
      doc = (scene_doc_t *)lparam;
      if (doc) {
        vp->cam_yaw = 0;
        vp->cam_pitch = 0;
      }
      return true;

    case evPaint: {
      if (!vp || !doc) return false;
      irect16_t cr = get_client_rect(win);
      int w = cr.w, h = cr.h;
      if (w <= 0 || h <= 0) return true;

      vp_create_fbo(vp, w, h);
      vp_render_scene(vp, doc);

      draw_rect((int)vp->color_tex, R(0, 0, w, h));
      return true;
    }

    case evLeftButtonDown:
      if (vp) {
        vp->last_mouse_x = (int16_t)LOWORD(wparam);
        vp->last_mouse_y = (int16_t)HIWORD(wparam);
        int mx = vp->last_mouse_x;
        int my = vp->last_mouse_y;

        if (doc) {
          irect16_t cr = get_client_rect(win);
          float nx = (float)mx / (float)cr.w * 2.0f - 1.0f;
          float ny = 1.0f - (float)my / (float)cr.h * 2.0f;
          float aspect = (float)cr.w / (float)cr.h;
          float fov = doc->scene.camFov > 0 ? doc->scene.camFov : 60;
          float tan_h = tanf(fov * M_PIf / 360.0f);

          mat4 view = mat4_lookat(doc->scene.camPos, doc->scene.camLook, v3(0, 1, 0));
          vec3 right = v3(view.m[0], view.m[4], view.m[8]);
          vec3 up = v3(view.m[1], view.m[5], view.m[9]);
          vec3 fwd = vnorm(vsub(doc->scene.camLook, doc->scene.camPos));

          vec3 ray_dir = vnorm(vadd(vadd(fwd, vscale(right, nx * tan_h * aspect)),
                                     vscale(up, ny * tan_h)));

          int picked = scene_pick_object(&doc->scene, doc->scene.camPos, ray_dir, NULL);
          doc->scene.selectedObj = picked;
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
      if (vp) {
        vp->orbiting = 0;
        set_capture(NULL);
      }
      return true;

    case evMouseMove: {
      if (!vp) return false;
      int mx = (int16_t)LOWORD(wparam);
      int my = (int16_t)HIWORD(wparam);
      int dx = mx - vp->last_mouse_x;
      int dy = my - vp->last_mouse_y;
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
        if (vp->fbo)       glDeleteFramebuffers(1, &vp->fbo);
        if (vp->color_tex) glDeleteTextures(1, &vp->color_tex);
        if (vp->depth_rbo) glDeleteRenderbuffers(1, &vp->depth_rbo);
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
  window_t *win = create_window(
      "Viewport",
      WINDOW_NOTITLE | WINDOW_NOFILL,
      MAKERECT(0, 0, cr.w, cr.h),
      parent, win_viewport, 0, doc);
  if (win) show_window(win, true);
  return win;
}
