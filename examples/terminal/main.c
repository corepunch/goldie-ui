// Terminal emulator example
// A simple terminal emulator using the Orion framework's VGA font rendering.

#include "vgat.h"
#include "../../gem_magic.h"

// File types this GEM handles (NULL-terminated).
static const char *terminal_file_types[] = { ".lua", NULL };

// Shared window-creation helper so both gem_init() and standalone main()
// use consistent dimensions, styles, and launch-data ownership.
static window_t *create_terminal_window(hinstance_t hinstance,
                                         const char *script_path,
                                         int x, int y) {
  terminal_launch_t launch = { .script_path = script_path };
  int sw = MIN(640, ui_get_system_metrics(kSystemMetricScreenWidth));
  int sh = MIN(480, ui_get_system_metrics(kSystemMetricScreenHeight));
  return create_window(
    VGAT_WINDOW_TITLE,
    WINDOW_VSCROLL,
    MAKERECT(x, y, sw, sh),
    NULL,
    terminal_proc,
    hinstance,
    script_path ? &launch : NULL
  );
}

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
  const char *script = argc > 1 ? argv[1] : NULL;
  for (int i = 0; i < 2; i++) {
    int off = i * 50;
    window_t *win = create_terminal_window(hinstance, script, 20 + off, 20 + off);
    if (!win) return false;
    show_window(win, true);
  }
  return true;
}

GEM_DEFINE("Terminal", "1.0", gem_init, NULL, terminal_file_types)

#ifndef BUILD_AS_GEM
int main(int argc, char *argv[]) {
  if (!ui_init_graphics(UI_INIT_DESKTOP, VGAT_WINDOW_TITLE, 1024, 768)) {
    printf("Failed to initialize graphics!\n");
    return 1;
  }

  register_commctl_classes();

  const char *script = argc > 1 ? argv[1] : NULL;
  for (int i = 0; i < 2; i++) {
    int off = i * 50;
    window_t *win = create_terminal_window(0, script, 20 + off, 20 + off);
    if (!win) {
      ui_shutdown_graphics();
      return 1;
    }
    show_window(win, true);
  }

  ui_event_t e;
  while (ui_is_running()) {
    while (get_message(&e)) dispatch_message(&e);
    repost_messages();
  }

  ui_shutdown_graphics();
  return 0;
}
#endif // BUILD_AS_GEM
