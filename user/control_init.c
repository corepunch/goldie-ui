// Orion UI Component Initialization
// 
// Initializes the Orion UI component registry and registers built-in controls.
// This layer is separate from kernel graphics initialization to maintain clean
// separation of concerns: kernel handles graphics/events, user handles UI components.
// 
// Applications should call ui_init_ui() after ui_init_graphics().

#include "user.h"
#include "../commctl/commctl.h"

// Initialize the Orion UI component registry and register all built-in controls.
// Must be called after ui_init_graphics() but before creating any UI windows.
// Safe to call multiple times; subsequent calls are no-ops if already initialized.
void ui_init_ui(void) {
  static bool initialized = false;
  if (initialized) return;
  
  register_commctl_classes();
  
  initialized = true;
}
