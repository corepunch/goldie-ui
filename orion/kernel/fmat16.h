#ifndef __UI_KERNEL_FMAT16_H__
#define __UI_KERNEL_FMAT16_H__

#include <string.h>

typedef struct {
  float v[16];
} fmat16_t;

static inline const float *fmat16_data(const fmat16_t *m) {
  return m->v;
}

static inline void fmat16_copy(const fmat16_t *src, fmat16_t *dst) {
  memcpy(dst->v, src->v, sizeof(dst->v));
}

static inline void fmat16_ortho(float l, float r, float b, float t,
                                float n, float f, fmat16_t *dst) {
  float rl = r - l;
  float tb = t - b;
  float fn = f - n;

  dst->v[0] = 2.0f / rl;
  dst->v[1] = 0.0f;
  dst->v[2] = 0.0f;
  dst->v[3] = 0.0f;

  dst->v[4] = 0.0f;
  dst->v[5] = 2.0f / tb;
  dst->v[6] = 0.0f;
  dst->v[7] = 0.0f;

  dst->v[8] = 0.0f;
  dst->v[9] = 0.0f;
  dst->v[10] = -2.0f / fn;
  dst->v[11] = 0.0f;

  dst->v[12] = -(r + l) / rl;
  dst->v[13] = -(t + b) / tb;
  dst->v[14] = -(f + n) / fn;
  dst->v[15] = 1.0f;
}

#endif /* __UI_KERNEL_FMAT16_H__ */
