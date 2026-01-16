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

#define ICON_FOLDER 5
#define ICON_FILE 6
#define ICON_UP 7

#define COLOR_FOLDER 0xffa0d000

typedef struct {
  char name[256];
  bool is_dir;
} entry_t;

typedef struct {
  char path[512];
  entry_t entries[MAX_ENTRIES];
  int count;
  window_t *columnview;
} filemanager_data_t;

static void load_directory(filemanager_data_t *data) {
  DIR *dir = opendir(data->path);
  if (!dir) return;
  
  // Clear the columnview
  send_message(data->columnview, CVM_CLEAR, 0, NULL);
  
  data->count = 0;
  strcpy(data->entries[data->count].name, "..");
  data->entries[data->count].is_dir = true;
  data->count++;
  
  // Add parent directory entry
  columnview_item_t item;
  strncpy(item.text, "..", sizeof(item.text) - 1);
  item.text[sizeof(item.text) - 1] = '\0';
  item.icon = ICON_UP;
  item.color = COLOR_FOLDER;
  item.userdata = &data->entries[0];
  send_message(data->columnview, CVM_ADDITEM, 0, &item);
  
  struct dirent *ent;
  // First pass: add directories
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
      
      // Add to columnview
      strncpy(item.text, data->entries[data->count].name, sizeof(item.text) - 1);
      item.text[sizeof(item.text) - 1] = '\0';
      item.icon = ICON_FOLDER;
      item.color = (data->entries[data->count].name[0] == '.') ? COLOR_TEXT_DISABLED : COLOR_FOLDER;
      item.userdata = &data->entries[data->count];
      send_message(data->columnview, CVM_ADDITEM, 0, &item);
      
      data->count++;
    }
  }
  
  // Second pass: add files
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
      
      // Add to columnview
      strncpy(item.text, data->entries[data->count].name, sizeof(item.text) - 1);
      item.text[sizeof(item.text) - 1] = '\0';
      item.icon = ICON_FILE;
      item.color = (data->entries[data->count].name[0] == '.') ? COLOR_TEXT_DISABLED : COLOR_TEXT_NORMAL;
      item.userdata = &data->entries[data->count];
      send_message(data->columnview, CVM_ADDITEM, 0, &item);
      
      data->count++;
    }
  }
  
  closedir(dir);
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
  invalidate_window(data->columnview);
}

result_t filemanager_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  filemanager_data_t *data = (filemanager_data_t *)win->userdata;
  
  switch (msg) {
    case WM_CREATE: {
      data = malloc(sizeof(filemanager_data_t));
      win->userdata = data;
      getcwd(data->path, sizeof(data->path));
      data->count = 0;
      
      // Create columnview control
      rect_t cv_rect = {0, 0, win->frame.w, win->frame.h};
      data->columnview = create_window("", WINDOW_NOTITLE | WINDOW_TRANSPARENT, &cv_rect, win, win_columnview, NULL);
      show_window(data->columnview, true);
      
      // Load initial directory
      load_directory(data);
      send_message(win, WM_STATUSBAR, 0, data->path);
      return true;
    }
    
    case WM_COMMAND: {
      uint16_t id = LOWORD(wparam);
      uint16_t code = HIWORD(wparam);
      
      if (id == data->columnview->id) {
        if (code == CVN_DBLCLK) {
          // Get the index from lparam
          int index = (int)(intptr_t)lparam;
          if (index >= 0 && index < data->count) {
            if (data->entries[index].is_dir) {
              navigate_to(win, data, data->entries[index].name);
            }
          }
        }
      }
      return true;
    }
    
    case WM_RESIZE: {
      // Resize columnview to match window
      if (data && data->columnview) {
        resize_window(data->columnview, win->frame.w, win->frame.h);
      }
      return false;
    }
    
    case WM_DESTROY:
      if (data) {
        if (data->columnview) {
          destroy_window(data->columnview);
        }
        free(data);
      }
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
