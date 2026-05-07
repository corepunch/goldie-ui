// About dialog - auto-layout form with a banner image on the left and
// application info on the right.

#include "imageeditor.h"

#define ABOUT_WIN_W    270
#define ABOUT_WIN_H    120
#define ABOUT_BANNER_W  ABOUT_WIN_H
#define ABOUT_BANNER_H  ABOUT_WIN_H
#define ABOUT_ID_BANNER 1
#define ABOUT_ID_OK     2

// State

typedef struct {
  GLuint banner_tex;
} about_state_t;

static GLuint load_banner_texture(void) {
  const char *found = NULL;
#ifdef SHAREDIR
  char bundled[4096];
  int n = snprintf(bundled, sizeof(bundled), "%s/" SHAREDIR "/conan.png",
                   ui_get_exe_dir());
  if (n >= 0 && (size_t)n < sizeof(bundled)) {
    FILE *f = fopen(bundled, "rb");
    if (f) { fclose(f); found = bundled; }
  }
#endif
  if (!found) {
    static const char *kSourceTreeBanner = "examples/imageeditor/share/conan.png";
    FILE *f = fopen(kSourceTreeBanner, "rb");
    if (f) { fclose(f); found = kSourceTreeBanner; }
  }
  if (!found) return 0;

  int w = 0, h = 0;
  uint8_t *pixels = load_image(found, &w, &h);
  if (!pixels) return 0;

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  image_free(pixels);

  return tex;
}

static const form_ctrl_def_t kAboutInfoActions[] = {
  { .class_name = "space", .name = "flex", .h_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "button", .id = ABOUT_ID_OK, .size = {50, BUTTON_HEIGHT},
    .flags = BUTTON_DEFAULT, .text = "OK", .name = "ok", .h_align = LAYOUT_ALIGN_START },
};

static const form_ctrl_def_t kAboutInfoChildren[] = {
  { .class_name = "label", .text = "Orion Image Editor", .name = "title",
    .h_align = LAYOUT_ALIGN_STRETCH, .font = FONT_SYSTEM, .font_set = true },
  { .class_name = "label", .text = "Version 1.0", .name = "version",
    .h_align = LAYOUT_ALIGN_STRETCH, .color = brTextDisabled, .color_set = true },
  { .class_name = "label", .text = "A MacPaint-inspired", .name = "desc1",
    .h_align = LAYOUT_ALIGN_STRETCH, .color = brTextDisabled, .color_set = true },
  { .class_name = "label", .text = "pixel art editor.", .name = "desc2",
    .h_align = LAYOUT_ALIGN_STRETCH, .color = brTextDisabled, .color_set = true },
  { .class_name = "label", .text = "Built with the", .name = "desc3",
    .h_align = LAYOUT_ALIGN_STRETCH, .color = brTextDisabled, .color_set = true },
  { .class_name = "label", .text = "Orion UI framework.", .name = "desc4",
    .h_align = LAYOUT_ALIGN_STRETCH, .color = brTextDisabled, .color_set = true },
  { .class_name = "space", .name = "spacer", .h_align = LAYOUT_ALIGN_STRETCH },
  {
    .class_name = "stack",
    .name = "actions",
    .layout_kind = "stack",
    .layout_orientation = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
    .children = kAboutInfoActions,
    .child_count = ARRAY_LEN(kAboutInfoActions),
  },
};

static const form_ctrl_def_t kAboutChildren[] = {
  {
    .class_name = "image",
    .id = ABOUT_ID_BANNER,
    .size = {ABOUT_BANNER_W, ABOUT_BANNER_H},
    .name = "banner",
    .h_align = LAYOUT_ALIGN_START,
    .v_align = LAYOUT_ALIGN_STRETCH,
  },
  {
    .class_name = "stack",
    .name = "info",
    .flags = WINDOW_FLEXSPACE,
    .layout_kind = "stack",
    .layout_orientation = WINDOW_STACK_VERTICAL,
    .layout_spacing = 2,
    .padding = {0, 8, 4, 4},
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_STRETCH,
    .children = kAboutInfoChildren,
    .child_count = ARRAY_LEN(kAboutInfoChildren),
  },
};

static const form_def_t kAboutForm = {
  .name = "About Orion Image Editor",
  .width = ABOUT_WIN_W,
  .height = ABOUT_WIN_H,
  .auto_layout = true,
  .layout_kind = "stack",
  .layout_orientation = WINDOW_STACK_HORIZONTAL,
  .layout_spacing = 8,
  .children = kAboutChildren,
  .child_count = ARRAY_LEN(kAboutChildren),
};

// Dialog window procedure

static result_t about_proc(window_t *win, uint32_t msg,
                            uint32_t wparam, void *lparam) {
  about_state_t *st = (about_state_t *)win->userdata;

  switch (msg) {
    case evCreate: {
      about_state_t *s = allocate_window_data(win, sizeof(about_state_t));
      s->banner_tex = load_banner_texture();
      window_t *banner = get_window_item(win, ABOUT_ID_BANNER);
      if (banner)
        banner->userdata = (void *)(uintptr_t)s->banner_tex;
      return true;
    }

    case evCommand: {
      if (HIWORD(wparam) == btnClicked) {
        end_dialog(win, 1);
        return true;
      }
      return false;
    }

    case evDestroy: {
      if (st && st->banner_tex) {
        glDeleteTextures(1, &st->banner_tex);
        st->banner_tex = 0;
      }
      return false;
    }

    default:
      return false;
  }
}

// ──────────────────────────────────────────────────────────────────
// Public entry point
// ──────────────────────────────────────────────────────────────────

void show_about_dialog(window_t *parent) {
  show_dialog_from_form(&kAboutForm, "About Orion Image Editor",
                        parent, about_proc, NULL);
}
