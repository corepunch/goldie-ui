#ifndef __UI_APPCHROME_H__
#define __UI_APPCHROME_H__

#include "menubar.h"

// Fixed, screen-wide application chrome with independent menu and toolbar
// bands.  The returned root owns both children.
window_t *create_app_chrome(const char *title, winproc_t menubar_proc,
                            const menu_def_t *menus, int menu_count,
                            winproc_t toolbar_proc, hinstance_t hinstance);
window_t *app_chrome_menubar(window_t *chrome);
window_t *app_chrome_toolbar(window_t *chrome);

#endif
