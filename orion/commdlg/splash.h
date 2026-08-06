#ifndef __UI_SPLASH_H__
#define __UI_SPLASH_H__

#include <orion/user/user.h>

// Splash screen dialog API.
// Displays an image in a borderless, always-on-top window that closes on click.
window_t *show_splash_screen(const char *image_path, hinstance_t hinstance);

#endif
