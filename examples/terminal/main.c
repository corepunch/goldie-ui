// Terminal emulator example
// A simple terminal emulator using the Orion framework's VGA font rendering.

#include "vgat.h"
#include "../../gem_magic.h"

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
  (void)argc; (void)argv;
  int sw = MIN(640, ui_get_system_metrics(kSystemMetricScreenWidth));
  int sh = MIN(480, ui_get_system_metrics(kSystemMetricScreenHeight));
  window_t *win = create_window(
    VGAT_WINDOW_TITLE,
    WINDOW_VSCROLL,
    MAKERECT(20, 20, sw - 40, sh - 40),
    NULL,
     terminal_proc,
    hinstance,
    NULL
  );
  if (!win) return false;
  show_window(win, true);
  return true;
}

GEM_DEFINE("Terminal", "1.0", gem_init, NULL, NULL)

#ifndef BUILD_AS_GEM
int main(int argc, char *argv[]) {
  (void)argc; (void)argv;

  if (!ui_init_graphics(UI_INIT_DESKTOP, VGAT_WINDOW_TITLE, 660, 428)) {
    printf("Failed to initialize graphics!\n");
    return 1;
  }

  register_commctl_classes();

  int sw = MIN(660, ui_get_system_metrics(kSystemMetricScreenWidth));
  int sh = MIN(428, ui_get_system_metrics(kSystemMetricScreenHeight));
  window_t *win = create_window(
    VGAT_WINDOW_TITLE,
    WINDOW_VSCROLL,
    MAKERECT(20, 20, sw - 40, sh - 40),
    NULL,
    terminal_proc,
    0,
    NULL
  );
  if (!win) {
    ui_shutdown_graphics();
    return 1;
  }

  show_window(win, true);

  ui_event_t e;
  while (ui_is_running()) {
    while (get_message(&e)) dispatch_message(&e);
    repost_messages();
  }

  ui_shutdown_graphics();
  return 0;
}
#endif // BUILD_AS_GEM
