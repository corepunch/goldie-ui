---
layout: default
title: GEM Plugin System
nav_order: 11
---

# GEM Plugin System

A GEM is an Orion application compiled as a loadable shared module. Standalone
and GEM builds use the same app lifecycle and window procedures.

| Mode | Output | Event loop owner |
|---|---|---|
| Standalone | `build/bin/myapp` | The application |
| GEM | `build/gem/myapp.gem` | Orion Shell |

Use `<orion/gem.h>` for both modes.

## Application Contract

```c
#include <orion/ui.h>
#include <orion/gem.h>

static window_t *g_main_win;

static result_t main_proc(window_t *win, uint32_t msg,
                          uint32_t wparam, void *lparam) {
  switch (msg) {
    case evPaint:
      draw_text_small("Hello from a GEM", 12, 12,
                      get_sys_color(brTextNormal));
      return true;
    case evDestroy:
#ifndef BUILD_AS_GEM
      ui_request_quit();
#endif
      return true;
    default:
      return false;
  }
}

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
  (void)argc;
  (void)argv;
  register_commctl_classes();
  g_main_win = create_window(
    "My App", 0, MAKERECT(32, 32, 320, 200),
    NULL, main_proc, hinstance, NULL);
  if (!g_main_win) return false;
  show_window(g_main_win, true);
  return true;
}

void gem_shutdown(void) {
  g_main_win = NULL;
}

GEM_DEFINE("My App", "1.0", gem_init, gem_shutdown, NULL)
GEM_STANDALONE_MAIN("My App", UI_INIT_DESKTOP, 640, 480, NULL, NULL)
```

`gem_init` creates application state and windows. `gem_shutdown` releases
app-owned allocations, textures, accelerators, databases, and plugins. Orion
owns window allocation, but apps should destroy live top-level windows during
explicit teardown when their lifecycle requires it.

## Exported Interface

`GEM_DEFINE` exports `gem_get_interface()`, which returns:

- display name and version,
- optional handled file extensions,
- `init(argc, argv, hinstance)`,
- optional `shutdown()`,
- optional contributed menus and command handler.

Shell loads the module through Platform's dynamic-library API, calls `init`, and
then drives all GEM windows through its shared event loop. A GEM must not run a
second loop or shut down the Shell.

## Build And Run

```bash
make build/bin/myapp       # standalone application
make build/gem/myapp.gem   # one GEM
make gems                  # all GEMs
make apps                  # all standalone applications
make all                   # framework, apps, GEMs, and tools
```

Load one or more modules at Shell startup:

```bash
build/bin/shell build/gem/imageeditor.gem build/gem/filemanager.gem
```

The build compiles GEMs with `BUILD_AS_GEM`, links them against Orion's shared
runtime libraries, and verifies the exported interface.

## Menus And Commands

Use `set_app_menu()` in `gem_init` so one definition works in both modes:

```c
g_app->menubar_win = set_app_menu(
  app_menubar_proc, kMenus, kNumMenus,
  handle_menu_command, hinstance);
```

In standalone mode it creates an application menu bar. In GEM mode it exposes
those menu definitions and the handler to Shell, which merges them into the
shared menu bar. Menu items, context menus, toolbars, and accelerators should
reuse the same command IDs.

## File Associations

Pass a null-terminated extension list to `GEM_DEFINE`:

```c
static const char *image_types[] = { ".png", ".jpg", ".bmp", NULL };
GEM_DEFINE("Image Editor", "1.0", gem_init, gem_shutdown, image_types)
```

Shell can then route opened files to the appropriate GEM. The app receives
payload paths in `argv`; `argv[0]` identifies the module or executable.

## Instance Ownership

Shell assigns each GEM an `hinstance_t`. Pass that instance when creating
application-owned top-level windows. The window manager uses it to group
windows, route shared menus, and unload a GEM when its application windows are
gone.

Do not store state in framework globals. Keep one app context owned by the GEM,
associate per-window state with `win->userdata`, and clean up app-owned
resources in `gem_shutdown`.

## Standard Standalone Options

`GEM_STANDALONE_MAIN` consumes framework options before calling `gem_init`.
Currently:

```text
--screenshot PATH  Capture the first fully painted frame as JPEG and exit
```

App-specific arguments remain in `argv`, so deterministic captures can combine
content and screenshot arguments. See
[Presenting An Application](app-presentation).

## Related Guides

- [Getting Started](getting-started) builds the same lifecycle step by step.
- [MDI Application Architecture](mdi) covers multi-document state and commands.
- [Architecture](architecture) explains layer ownership and message flow.
- [Applications](examples) shows complete standalone/GEM implementations.
