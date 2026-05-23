#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "browser.h"

#define BROWSER_SETTINGS_FILE "browser.ini"
#define BROWSER_DEFAULT_HOME  "https://example.com"

#define ID_DLG_HOME_EDIT   3101
#define ID_DLG_SAVE        3102
#define ID_DLG_CANCEL      3103

#define ID_ABOUT_OK        3201

typedef struct {
  browser_state_t *st;
} browser_settings_dialog_state_t;

static const form_ctrl_def_t kSettingsUrlRow[] = {
  { .class_name = "Label",   .text = "Home URL:",       .name = "lbl_home",
    .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
  { .class_name = "TextBox", .id = ID_DLG_HOME_EDIT,   .name = "edit_home",
    .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
};

static const form_ctrl_def_t kSettingsBtnRow[] = {
  { .class_name = "Button", .id = ID_DLG_SAVE,   .flags = BUTTON_DEFAULT, .text = "Save",
    .name = "btn_save",   .h_align = LAYOUT_ALIGN_START },
  { .class_name = "Button", .id = ID_DLG_CANCEL, .text = "Cancel",
    .name = "btn_cancel", .h_align = LAYOUT_ALIGN_START },
};

static const form_ctrl_def_t kSettingsChildren[] = {
  {
    .class_name         = "stack",
    .name               = "url_row",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing     = 6,
    .h_align            = LAYOUT_ALIGN_STRETCH,
    .v_align            = LAYOUT_ALIGN_START,
    .children           = kSettingsUrlRow,
    .child_count        = (int)(sizeof(kSettingsUrlRow)/sizeof(kSettingsUrlRow[0])),
  },
  {
    .class_name         = "stack",
    .name               = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing     = 6,
    .h_align            = LAYOUT_ALIGN_END,
    .v_align            = LAYOUT_ALIGN_START,
    .children           = kSettingsBtnRow,
    .child_count        = (int)(sizeof(kSettingsBtnRow)/sizeof(kSettingsBtnRow[0])),
  },
};

static const form_def_t kSettingsForm = {
  .name           = "Browser Settings",
  .flags          = WINDOW_AUTO_LAYOUT,
  .width          = 344,
  .height         = 62,
  .layout_spacing = 6,
  .padding        = {8, 8, 8, 8},
  .children       = kSettingsChildren,
  .child_count    = (int)(sizeof(kSettingsChildren) / sizeof(kSettingsChildren[0])),
};

static const form_ctrl_def_t kAboutChildren[] = {
  { .class_name = "Label",  .text = "Orion Browser",
    .name = "lbl_title",   .h_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "Label",  .text = "Version 0.2",
    .name = "lbl_version", .h_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "Label",  .text = "Minimal HTML browser with local file support.",
    .name = "lbl_desc",    .h_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "Button", .id = ID_ABOUT_OK, .flags = BUTTON_DEFAULT, .text = "OK",
    .name = "btn_ok",      .h_align = LAYOUT_ALIGN_CENTER },
};

static const form_def_t kAboutForm = {
  .name           = "About Browser",
  .flags          = WINDOW_AUTO_LAYOUT,
  .width          = 236,
  .height         = 98,
  .layout_spacing = 6,
  .padding        = {8, 8, 8, 8},
  .children       = kAboutChildren,
  .child_count    = (int)(sizeof(kAboutChildren) / sizeof(kAboutChildren[0])),
  .ok_id          = ID_ABOUT_OK,
};


static void trim_copy_url(char *dst, size_t dst_sz, const char *src) {
  if (!dst || dst_sz == 0) return;
  dst[0] = '\0';
  if (!src) return;

  while (*src && isspace((unsigned char)*src)) src++;
  size_t n = strlen(src);
  while (n > 0 && isspace((unsigned char)src[n - 1])) n--;
  if (n == 0) return;

  snprintf(dst, dst_sz, "%.*s", (int)n, src);
}

void browser_settings_init(browser_state_t *st) {
  if (!st) return;
  snprintf(st->home_url, sizeof(st->home_url), "%s", BROWSER_DEFAULT_HOME);
}

bool browser_settings_load(browser_state_t *st) {
  if (!st) return false;

  char buf[2048];
  size_t n = 0;
  if (!axSettingsLoad(BROWSER_SETTINGS_FILE, buf, sizeof(buf) - 1, &n)) return false;
  buf[n] = '\0';

  bool in_browser_section = false;
  char *line = buf;
  while (*line) {
    char *next = strchr(line, '\n');
    if (next) {
      *next = '\0';
      next++;
    }

    while (*line && isspace((unsigned char)*line)) line++;
    char *end = line + strlen(line);
    while (end > line && isspace((unsigned char)end[-1])) *--end = '\0';

    if (*line == '\0' || *line == ';' || *line == '#') {
      line = next ? next : end;
      continue;
    }

    if (*line == '[') {
      in_browser_section = (strcmp(line, "[browser]") == 0);
      line = next ? next : end;
      continue;
    }

    if (in_browser_section) {
      char *eq = strchr(line, '=');
      if (eq) {
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;

        while (*key && isspace((unsigned char)*key)) key++;
        char *kend = key + strlen(key);
        while (kend > key && isspace((unsigned char)kend[-1])) *--kend = '\0';

        while (*val && isspace((unsigned char)*val)) val++;

        if (strcmp(key, "home_url") == 0) {
          char home[sizeof(st->home_url)];
          trim_copy_url(home, sizeof(home), val);
          if (home[0])
            snprintf(st->home_url, sizeof(st->home_url), "%s", home);
        }
      }
    }

    line = next ? next : end;
  }

  return true;
}

bool browser_settings_save(const browser_state_t *st) {
  if (!st) return false;

  char text[1400];
  int n = snprintf(
    text,
    sizeof(text),
    "; Orion browser settings\n"
    "[browser]\n"
    "home_url=%s\n",
    st->home_url[0] ? st->home_url : BROWSER_DEFAULT_HOME
  );
  if (n <= 0 || (size_t)n >= sizeof(text)) return false;

  return axSettingsSave(BROWSER_SETTINGS_FILE, text, (size_t)n) ? true : false;
}

static lresult_t browser_settings_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  browser_settings_dialog_state_t *ds = (browser_settings_dialog_state_t *)win->userdata;

  switch (msg) {
    case evCreate:
      win->userdata = lparam;
      ds = (browser_settings_dialog_state_t *)win->userdata;
      if (ds && ds->st) {
        ds->st->settings_win = win;
        set_window_item_text(win, ID_DLG_HOME_EDIT, "%s", ds->st->home_url);
      }
      return true;

    case evClose:
      destroy_window(win);
      return true;

    case evCommand:
      if (HIWORD(wparam) != btnClicked) return false;
      if (LOWORD(wparam) == ID_DLG_CANCEL) {
        destroy_window(win);
        return true;
      }
      if (LOWORD(wparam) == ID_DLG_SAVE) {
        window_t *home = get_window_item(win, ID_DLG_HOME_EDIT);
        if (home && ds && ds->st) {
          char trimmed[sizeof(ds->st->home_url)];
          trim_copy_url(trimmed, sizeof(trimmed), home->title);
          if (trimmed[0])
            snprintf(ds->st->home_url, sizeof(ds->st->home_url), "%s", trimmed);
          browser_settings_save(ds->st);
        }
        destroy_window(win);
        return true;
      }
      return false;

    case evDestroy:
      if (ds && ds->st && ds->st->settings_win == win)
        ds->st->settings_win = NULL;
      free(ds);
      win->userdata = NULL;
      return true;

    default:
      return default_winproc(win, msg, wparam, lparam);
  }
}

bool browser_show_settings_window(window_t *parent, browser_state_t *st) {
  if (!st) return false;
  if (st->settings_win && is_window(st->settings_win)) {
    move_to_top(st->settings_win);
    set_focus(st->settings_win);
    return true;
  }

  browser_settings_dialog_state_t *ds = malloc(sizeof(*ds));
  if (!ds) return false;
  ds->st = st;

  // Use create_window_from_form (not show_dialog_from_form) because the settings
  // window is modeless — it stays open while the user continues browsing.
  // Manual centering is therefore necessary here.
  form_def_t settings_def = kSettingsForm;
  settings_def.flags |= WINDOW_DIALOG | WINDOW_NOTRAYBUTTON | WINDOW_NORESIZE;

  irect16_t wr = {0, 0, settings_def.width, settings_def.height};
  adjust_window_rect(&wr, settings_def.flags);
  wr = center_window_rect(wr, parent);

  window_t *win = create_window_from_form(&settings_def, wr.x, wr.y,
                                          NULL, browser_settings_proc,
                                          parent ? get_root_window(parent)->hinstance : 0,
                                          ds);
  if (!win) {
    free(ds);
    return false;
  }

  show_window(win, true);
  move_to_top(win);
  set_focus(win);
  return true;
}

bool browser_pick_open_path(window_t *parent, char *out_path, size_t out_sz) {
  openfilename_t ofn = {0};

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = parent;
  ofn.lpstrFile = out_path;
  ofn.nMaxFile = (uint32_t)out_sz;
  ofn.lpstrFilter = "HTML Files\0*.html;*.htm\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_FILEMUSTEXIST;

  return get_open_filename(&ofn);
}

bool browser_pick_save_path(window_t *parent, char *out_path, size_t out_sz) {
  openfilename_t ofn = {0};

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = parent;
  ofn.lpstrFile = out_path;
  ofn.nMaxFile = (uint32_t)out_sz;
  ofn.lpstrFilter = "HTML Files\0*.html;*.htm\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_OVERWRITEPROMPT;

  return get_save_filename(&ofn);
}

void browser_show_about_dialog(window_t *parent) {
  show_ddx_dialog(&kAboutForm, "About Browser", parent, NULL);
}
