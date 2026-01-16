#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../ui.h"

extern bool running;

#define ICON_FOLDER 5
#define ICON_FILE 6
#define ICON_UP 7
#define COLOR_FOLDER 0xffa0d000

typedef struct {
  char path[512];
} filemanager_data_t;

static void load_directory(window_t *win, filemanager_data_t *data) {
  DIR *dir = opendir(data->path);
  if (!dir) return;
  
  send_message(win, CVM_CLEAR, 0, NULL);
  
  // Add parent directory
  columnview_item_t item = {"..", ICON_UP, COLOR_FOLDER, NULL};
  send_message(win, CVM_ADDITEM, 0, &item);
  
  struct dirent *ent;
  // First pass: directories
  while ((ent = readdir(dir)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
    char fullpath[768];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", data->path, ent->d_name);
    struct stat st;
    if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
      strcpy(item.text, ent->d_name);
      item.icon = ICON_FOLDER;
      item.color = (ent->d_name[0] == '.') ? COLOR_TEXT_DISABLED : COLOR_FOLDER;
      item.userdata = (void*)1; // Mark as directory
      send_message(win, CVM_ADDITEM, 0, &item);
    }
  }
  
  // Second pass: files
  rewinddir(dir);
  while ((ent = readdir(dir)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
    char fullpath[768];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", data->path, ent->d_name);
    struct stat st;
    if (stat(fullpath, &st) == 0 && !S_ISDIR(st.st_mode)) {
      strcpy(item.text, ent->d_name);
      item.icon = ICON_FILE;
      item.color = (ent->d_name[0] == '.') ? COLOR_TEXT_DISABLED : COLOR_TEXT_NORMAL;
      item.userdata = NULL; // Mark as file
      send_message(win, CVM_ADDITEM, 0, &item);
    }
  }
  
  closedir(dir);
  send_message(win, WM_STATUSBAR, 0, data->path);
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
    snprintf(newpath, sizeof(newpath), "%s%s%s", data->path, strcmp(data->path, "/") == 0 ? "" : "/", name);
    strcpy(data->path, newpath);
  }
  load_directory(win, data);
}

result_t filemanager_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  filemanager_data_t *data = (filemanager_data_t *)win->userdata2;
  
  switch (msg) {
    case WM_CREATE:
      // First call columnview's WM_CREATE
      win_columnview(win, msg, wparam, lparam);
      // Then set up filemanager data
      data = malloc(sizeof(filemanager_data_t));
      win->userdata2 = data;
      getcwd(data->path, sizeof(data->path));
      load_directory(win, data);
      return true;
    
    case WM_COMMAND:
      if (HIWORD(wparam) == CVN_DBLCLK) {
        int index = (int)(intptr_t)lparam;
        columnview_item_t item;
        if (send_message(win, CVM_GETITEMDATA, index, &item)) {
          if (item.userdata || index == 0) { // Directory or parent dir
            navigate_to(win, data, item.text);
          }
        }
      }
      return false;
    
    case WM_DESTROY:
      if (data) free(data);
      win_columnview(win, msg, wparam, lparam);
      running = false;
      return true;
    
    default:
      return win_columnview(win, msg, wparam, lparam);
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
