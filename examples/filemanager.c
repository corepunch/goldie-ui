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
      strcpy(data->entries[data->count].name, ent->d_name);
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
      strcpy(data->entries[data->count].name, ent->d_name);
      data->entries[data->count].is_dir = false;
      data->count++;
    }
  }
  
  closedir(dir);
  data->selected = -1;
}

static void navigate_to(filemanager_data_t *data, const char *name) {
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
}

result_t filemanager_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  filemanager_data_t *data = (filemanager_data_t *)win->userdata;
  
  switch (msg) {
    case WM_CREATE: {
      data = malloc(sizeof(filemanager_data_t));
      win->userdata = data;
      getcwd(data->path, sizeof(data->path));
      data->last_click_time = 0;
      data->last_click_index = -1;
      load_directory(data);
      return true;
    }
    
    case WM_PAINT: {
      int col1_y = 0;
      int col2_y = 0;
      
      for (int i = 0; i < data->count; i++) {
        int col = (i < (data->count + 1) / 2) ? 0 : 1;
        int x = col * COLUMN_WIDTH + 5;
        int y = (col == 0 ? col1_y : col2_y) * ENTRY_HEIGHT + 5;
        
        if (i == data->selected) {
          fill_rect(COLOR_TEXT_NORMAL, x - 2, y - 1, COLUMN_WIDTH - 6, ENTRY_HEIGHT);
          char display[280];
          if (data->entries[i].is_dir) {
            snprintf(display, sizeof(display), "%s/", data->entries[i].name);
          } else {
            snprintf(display, sizeof(display), "%s", data->entries[i].name);
          }
          draw_text_small(display, x, y, COLOR_PANEL_BG);
        } else {
          char display[280];
          if (data->entries[i].is_dir) {
            snprintf(display, sizeof(display), "%s/", data->entries[i].name);
          } else {
            snprintf(display, sizeof(display), "%s", data->entries[i].name);
          }
          draw_text_small(display, x, y, COLOR_TEXT_NORMAL);
        }
        
        if (col == 0) col1_y++; else col2_y++;
      }
      
      draw_text_small(data->path, 5, win->frame.h - 15, COLOR_TEXT_DISABLED);
      return false;
    }
    
    case WM_LBUTTONDOWN: {
      int mx = LOWORD(wparam);
      int my = HIWORD(wparam);
      int col = mx < COLUMN_WIDTH ? 0 : 1;
      int row = (my - 5) / ENTRY_HEIGHT;
      int index = row + (col == 0 ? 0 : (data->count + 1) / 2);
      
      if (index >= 0 && index < data->count) {
        uint32_t now = SDL_GetTicks();
        if (data->last_click_index == index && (now - data->last_click_time) < 500) {
          if (data->entries[index].is_dir) {
            navigate_to(data, data->entries[index].name);
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
    0,
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
