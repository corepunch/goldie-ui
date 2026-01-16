#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../ui.h"

extern bool running;

#define MAX_ENTRIES 256
#define ENTRY_HEIGHT 13
#define COLUMN_WIDTH 160
#define ICON_OFFSET 12
#define ICON_DODGE 1

#define ICON_FOLDER 5
#define ICON_FILE 6
#define ICON_UP 7

#define COLOR_FOLDER 0xffa0d000

#define WIN_PADDING 4

typedef struct {
  char name[256];
  bool is_dir;
} entry_t;

typedef struct {
  char path[512];
  entry_t entries[MAX_ENTRIES];
  int count;
  int selected;
  uint32_t last_click_time;
  int last_click_index;
} filemanager_data_t;

static void load_directory(filemanager_data_t *data) {
  DIR *dir = opendir(data->path);
  if (!dir) return;
  
  data->count = 0;
  strcpy(data->entries[data->count].name, "..");
  data->entries[data->count].is_dir = true;
  data->count++;
  
  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL && data->count < MAX_ENTRIES) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
    
    char fullpath[768];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", data->path, ent->d_name);
    struct stat st;
    bool is_dir = false;
    if (stat(fullpath, &st) == 0) {
      is_dir = S_ISDIR(st.st_mode);
    }
    
    if (is_dir) {
      strncpy(data->entries[data->count].name, ent->d_name, sizeof(data->entries[data->count].name) - 1);
      data->entries[data->count].name[sizeof(data->entries[data->count].name) - 1] = '\0';
      data->entries[data->count].is_dir = true;
      data->count++;
    }
  }
  
  rewinddir(dir);
  while ((ent = readdir(dir)) != NULL && data->count < MAX_ENTRIES) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
    
    char fullpath[768];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", data->path, ent->d_name);
    struct stat st;
    bool is_dir = false;
    if (stat(fullpath, &st) == 0) {
      is_dir = S_ISDIR(st.st_mode);
    }
    
    if (!is_dir) {
      strncpy(data->entries[data->count].name, ent->d_name, sizeof(data->entries[data->count].name) - 1);
      data->entries[data->count].name[sizeof(data->entries[data->count].name) - 1] = '\0';
      data->entries[data->count].is_dir = false;
      data->count++;
    }
  }
  
  closedir(dir);
  data->selected = -1;
}

static void navigate_to(window_t *win, filemanager_data_t *data, const char *name) {
  if (strcmp(name, "..") == 0) {
    char *last_slash = strrchr(data->path, '/');
    if (last_slash && last_slash != data->path) {
      *last_slash = '\0';
    } else if (strcmp(data->path, "/") != 0) {
      strcpy(data->path, "/");
    }
  } else {
    char newpath[512];
    if (strcmp(data->path, "/") == 0) {
      snprintf(newpath, sizeof(newpath), "/%s", name);
    } else {
      snprintf(newpath, sizeof(newpath), "%s/%s", data->path, name);
    }
    strncpy(data->path, newpath, sizeof(data->path) - 1);
    data->path[sizeof(data->path) - 1] = '\0';
  }
  load_directory(data);
  send_message(win, WM_STATUSBAR, 0, data->path);
}

result_t filemanager_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  filemanager_data_t *data = (filemanager_data_t *)win->userdata;
  const int ncol = win->frame.w / COLUMN_WIDTH;
  switch (msg) {
    case WM_CREATE: {
      data = malloc(sizeof(filemanager_data_t));
      win->userdata = data;
      getcwd(data->path, sizeof(data->path));
      data->last_click_time = 0;
      data->last_click_index = -1;
      load_directory(data);
      send_message(win, WM_STATUSBAR, 0, data->path);
      return true;
    }
    
    case WM_PAINT: {
      for (int i = 0; i < data->count; i++) {
        int col = i % ncol; // (i < (data->count + 1) / 2) ? 0 : 1;
        int x = col * COLUMN_WIDTH + WIN_PADDING;
        int y = (i / ncol) * ENTRY_HEIGHT + WIN_PADDING;
        int icon = data->entries[i].is_dir ? ICON_FOLDER : ICON_FILE;
        int color = data->entries[i].is_dir ? COLOR_FOLDER : COLOR_TEXT_NORMAL;
        if (!strcmp(data->entries[i].name, "..")) icon = ICON_UP;
        else if (!strncmp(data->entries[i].name, ".", 1)) color = COLOR_TEXT_DISABLED;
        set_clip_rect(win, &(rect_t){x - 2, y - 2, COLUMN_WIDTH - 6, ENTRY_HEIGHT-2});
        if (i == data->selected) {
          fill_rect(COLOR_TEXT_NORMAL, x - 2, y - 2, COLUMN_WIDTH - 6, ENTRY_HEIGHT-2);
          draw_icon8(icon, x, y-ICON_DODGE, COLOR_PANEL_BG);
          draw_text_small(data->entries[i].name, x+ICON_OFFSET, y, COLOR_PANEL_BG);
        } else {
          draw_icon8(icon, x, y-ICON_DODGE, color);
          draw_text_small(data->entries[i].name, x+ICON_OFFSET, y, color);
        }
      }
      
      return false;
    }
    
    case WM_LBUTTONDOWN: {
      int mx = LOWORD(wparam);
      int my = HIWORD(wparam);
      int col = mx / COLUMN_WIDTH;
      int row = (my - WIN_PADDING) / ENTRY_HEIGHT;
      int index = row * ncol + col;
      
      if (index >= 0 && index < data->count) {
        uint32_t now = SDL_GetTicks();
        if (data->last_click_index == index && (now - data->last_click_time) < 500) {
          if (data->entries[index].is_dir) {
            navigate_to(win, data, data->entries[index].name);
            invalidate_window(win);
          }
          data->last_click_time = 0;
          data->last_click_index = -1;
        } else {
          data->selected = index;
          data->last_click_time = now;
          data->last_click_index = index;
          invalidate_window(win);
        }
      }
      return true;
    }
    
    case WM_DESTROY:
      if (data) free(data);
      running = false;
      return true;
      
    default:
      return false;
  }
}

int main(int argc, char* argv[]) {
  if (!ui_init_graphics(UI_INIT_DESKTOP|UI_INIT_TRAY, "File Manager", 640, 480)) {
    printf("Failed to initialize graphics!\n");
    return 1;
  }
  
  window_t *main_window = create_window(
    "File Manager",
    WINDOW_STATUSBAR,
    MAKERECT(20, 20, 340, 400),
    NULL,
    filemanager_window_proc,
    NULL
  );
  
  if (!main_window) {
    printf("Failed to create window!\n");
    ui_shutdown_graphics();
    return 1;
  }
  
  show_window(main_window, true);
  
  ui_event_t e;
  while (running) {
    while (get_message(&e)) {
      dispatch_message(&e);
    }
    repost_messages();
  }
  
  destroy_window(main_window);
  ui_shutdown_graphics();
  return 0;
}
