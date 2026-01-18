#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../ui.h"

#define SUPPORT_LUA

extern bool running;

#define ICON_FOLDER 5
#define ICON_FILE 6
#define ICON_UP 7
#define COLOR_FOLDER 0xffa0d000
#define COLOR_SCRIPT 0xff00a0d0

typedef struct {
  char path[512];
} filemanager_data_t;

static int get_file_color(struct dirent *ent, struct stat *st) {
  if (ent->d_name[0] == '.') {
    return COLOR_TEXT_DISABLED;
  } else if (S_ISDIR(st->st_mode)) {
    return COLOR_FOLDER;
#ifdef SUPPORT_LUA
  } else if (strstr(ent->d_name, ".lua")) {
    return COLOR_SCRIPT;
#endif
  } else {
    return COLOR_TEXT_NORMAL;
  }
}

static int get_file_icon(struct dirent *ent, struct stat *st) {
  if (S_ISDIR(st->st_mode)) {
    return ICON_FOLDER;
  } else {
    return ICON_FILE;
  }
}

static void add_entries(DIR *dir, window_t *win, filemanager_data_t *data, bool dirs_only) {
  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
    char fullpath[768];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", data->path, ent->d_name);
    struct stat st;
    if (stat(fullpath, &st) || S_ISDIR(st.st_mode) != dirs_only) continue;
    send_message(win, CVM_ADDITEM, 0, &(columnview_item_t) {
      .text = ent->d_name,
      .icon = get_file_icon(ent, &st),
      .color = get_file_color(ent, &st),
      .userdata = S_ISDIR(st.st_mode),
    });
  }
}

static void load_directory(window_t *win, filemanager_data_t *data) {
  DIR *dir = opendir(data->path);
  if (!dir) return;
  
  send_message(win, CVM_CLEAR, 0, NULL);
  // Add parent directory
  send_message(win, CVM_ADDITEM, 0, &(columnview_item_t) {"..", ICON_UP, COLOR_FOLDER, 0});

  // Add directories
  rewinddir(dir);
  add_entries(dir, win, data, true);

  // Add files
  rewinddir(dir);
  add_entries(dir, win, data, false);

  closedir(dir);
  send_message(win, kWindowMessageStatusBar, 0, data->path);
}

static void navigate_to(window_t *win, filemanager_data_t *data, columnview_item_t *item) {
  char newpath[512];
  snprintf(newpath, sizeof(newpath), "%s/%s", data->path, item->text);
  if (strcmp(item->text, "..") == 0) {
    char *last_slash = strrchr(data->path, '/');
    if (last_slash && last_slash != data->path) {
      *last_slash = '\0';
    } else if (strcmp(data->path, "/") != 0) {
      strcpy(data->path, "/");
    }
    load_directory(win, data);
  } else if (item->userdata) {
    strcpy(data->path, newpath);
    load_directory(win, data);
#ifdef SUPPORT_LUA
  } else if (item->color == COLOR_SCRIPT) {
    // Execute script
    printf("Executing script: %s\n", newpath);
    show_window(create_window("Terminal", 0, MAKERECT(16, 16, 240, 120), NULL, win_terminal, newpath), true);
#endif 
    // Here you would add your script execution logic
  }
}

result_t filemanager_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  filemanager_data_t *data = (filemanager_data_t *)win->userdata;
  
  switch (msg) {
    case kWindowMessageCreate:
      // First call columnview's kWindowMessageCreate
      win_columnview(win, msg, wparam, lparam);
      // Then set up filemanager data
      data = allocate_window_data(win, sizeof(filemanager_data_t));
      getcwd(data->path, sizeof(data->path));
      load_directory(win, data);
      return true;
    
    case kWindowMessageCommand:
      if (kHighWord(wparam) == CVN_DBLCLK) {
        // Can use CVM_GETITEMDATA to get item data, but lparam already has it
        // columnview_item_t tmp;
        // send_message(win, CVM_GETITEMDATA, kLowWord(wparam), &tmp);
        navigate_to(win, data, lparam);
      }
      return false;
    
    case kWindowMessageDestroy:
      if (data) free(data);
      win_columnview(win, msg, wparam, lparam);
      running = false;
      return true;
    
    default:
      return win_columnview(win, msg, wparam, lparam);
  }
}

int main(int argc, char* argv[]) {
  if (!ui_init_graphics(UI_INIT_DESKTOP|UI_INIT_TRAY, "File Manager", 480, 320)) {
    printf("Failed to initialize graphics!\n");
    return 1;
  }
  
  window_t *main_window = create_window(
    "File Manager",
    WINDOW_STATUSBAR,
    MAKERECT(20, 20, 320, 240),
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
