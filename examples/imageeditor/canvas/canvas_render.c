// Canvas rendering: compositing and GL texture management

#include "imageeditor.h"

// ============================================================
// Compositing
// ============================================================

void canvas_composite(const canvas_doc_t *doc, uint8_t *dst) {
  size_t n = (size_t)doc->canvas_w * doc->canvas_h;
  memset(dst, 0x00, n * 4);

#if IMAGEEDITOR_INDEXED
  // Indexed mode: one layer, map each pixel index through the palette.
  // The transparent index produces fully transparent pixels.
  if (doc->layer.count > 0 && doc->layer.stack[0]->pixels) {
    const uint8_t *idx_buf = doc->layer.stack[0]->pixels;
    for (size_t i = 0; i < n; i++) {
      uint8_t pidx = idx_buf[i];
      uint8_t *d = dst + i * 4;
      if (pidx == (uint8_t)doc->ipal.transparent) {
        // Transparent: leave as zero (already cleared).
        continue;
      }
      uint32_t c = doc->ipal.entries[pidx];
      d[0] = COLOR_R(c);
      d[1] = COLOR_G(c);
      d[2] = COLOR_B(c);
      d[3] = 255;
    }
  }
#else
  for (int li = 0; li < doc->layer.count; li++) {
    const layer_t *lay = doc->layer.stack[li];
    if (!lay->visible) continue;

    for (size_t i = 0; i < n; i++) {
      const uint8_t *src = lay->pixels + i * 4;
      uint8_t       *d   = dst + i * 4;

      uint32_t sa = (src[3] * lay->opacity + 127) / 255;
      if (sa == 0) continue;

      uint32_t da = d[3];
      uint32_t inv = 255 - sa;
      uint32_t out_a = sa + (da * inv + 127) / 255;
      if (out_a == 0) continue;

      uint8_t br = src[0], bg = src[1], bb = src[2];
      switch (lay->blend_mode) {
        case LAYER_BLEND_MULTIPLY:
          br = (uint8_t)((uint32_t)src[0] * d[0] / 255);
          bg = (uint8_t)((uint32_t)src[1] * d[1] / 255);
          bb = (uint8_t)((uint32_t)src[2] * d[2] / 255);
          break;
        case LAYER_BLEND_SCREEN:
          br = (uint8_t)(255 - ((uint32_t)(255 - src[0]) * (255 - d[0]) / 255));
          bg = (uint8_t)(255 - ((uint32_t)(255 - src[1]) * (255 - d[1]) / 255));
          bb = (uint8_t)(255 - ((uint32_t)(255 - src[2]) * (255 - d[2]) / 255));
          break;
        case LAYER_BLEND_ADD:
          br = (uint8_t)MIN(255, (int)src[0] + (int)d[0]);
          bg = (uint8_t)MIN(255, (int)src[1] + (int)d[1]);
          bb = (uint8_t)MIN(255, (int)src[2] + (int)d[2]);
          break;
        case LAYER_BLEND_NORMAL:
        default:
          break;
      }

      uint64_t out_r = (uint64_t)br * sa * 255 +
                       (uint64_t)d[0] * da * inv;
      uint64_t out_g = (uint64_t)bg * sa * 255 +
                       (uint64_t)d[1] * da * inv;
      uint64_t out_b = (uint64_t)bb * sa * 255 +
                       (uint64_t)d[2] * da * inv;
      uint64_t denom = (uint64_t)out_a * 255;

      d[0] = (uint8_t)((out_r + denom / 2) / denom);
      d[1] = (uint8_t)((out_g + denom / 2) / denom);
      d[2] = (uint8_t)((out_b + denom / 2) / denom);
      d[3] = (uint8_t)out_a;
    }
  }
#endif
}

void canvas_composite_over_bg(const canvas_doc_t *doc, uint8_t *rgba) {
  if (!doc || !rgba) return;
  if (!doc->background.show) return;

  uint8_t bg_r = COLOR_R(doc->background.color);
  uint8_t bg_g = COLOR_G(doc->background.color);
  uint8_t bg_b = COLOR_B(doc->background.color);
  size_t n = (size_t)doc->canvas_w * doc->canvas_h;

  for (size_t i = 0; i < n; i++) {
    uint8_t *p = rgba + i * 4;
    uint32_t sa = p[3];
    if (sa == 0) {
      p[0] = bg_r;
      p[1] = bg_g;
      p[2] = bg_b;
      p[3] = 255;
      continue;
    }
    if (sa == 255) {
      continue;
    }

    uint32_t inv = 255 - sa;
    p[0] = (uint8_t)((p[0] * sa + bg_r * inv + 127) / 255);
    p[1] = (uint8_t)((p[1] * sa + bg_g * inv + 127) / 255);
    p[2] = (uint8_t)((p[2] * sa + bg_b * inv + 127) / 255);
    p[3] = 255;
  }
}

// ============================================================
// GL texture management
// ============================================================

static void layer_upload_texture(canvas_doc_t *doc, layer_t *lay) {
  if (!doc || !lay || !lay->pixels) return;
#if IMAGEEDITOR_INDEXED
  // Indexed mode: expand the palette-index buffer to RGBA for the GPU.
  // Use the composite scratch buffer as a temporary (it is always canvas_w * canvas_h * 4).
  uint8_t *rgba = doc->layer.composite_buf;
  if (!rgba) return;
  canvas_composite(doc, rgba);
  if (!lay->tex) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, doc->canvas_w, doc->canvas_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    lay->tex = tex;
  } else {
    glBindTexture(GL_TEXTURE_2D, lay->tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, doc->canvas_w, doc->canvas_h,
                    GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  }
#else
  if (!lay->tex) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, doc->canvas_w, doc->canvas_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, lay->pixels);
    lay->tex = tex;
  } else {
    glBindTexture(GL_TEXTURE_2D, lay->tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, doc->canvas_w, doc->canvas_h,
                    GL_RGBA, GL_UNSIGNED_BYTE, lay->pixels);
  }
#endif
}

void canvas_upload(canvas_doc_t *doc) {
  if (!doc) return;
  bool need_upload = doc->canvas_dirty;
  for (int i = 0; i < doc->layer.count && !need_upload; i++) {
    if (!doc->layer.stack[i]->tex)
      need_upload = true;
  }

  if (need_upload) {
    for (int i = 0; i < doc->layer.count; i++)
      layer_upload_texture(doc, doc->layer.stack[i]);
    doc->canvas_dirty = false;
  }
}
