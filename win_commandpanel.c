#include "scener.h"
#include <orion/kernel/renderer.h>
#include <orion/user/draw.h>
#include <orion/user/image.h>
#include "gmax_icons.h"

enum {
	CP_WIDTH = SIDE_PANEL_WIDTH,
	CP_HEADER_HEIGHT = 24,
	CP_BUTTON_HEIGHT = 20,
};

typedef struct { const char *label; uint16_t id; int icon; } cp_item_t;
typedef struct { const char *label; const cp_item_t *items; int count; } cp_section_t;
typedef struct { const char *label; uint16_t id; int icon; const cp_section_t *sections; int count; } cp_tab_t;
typedef struct { const cp_tab_t *tab; } cp_page_state_t;
typedef struct {
	uint32_t icons;
	bitmap_strip_t strip;
	window_t *tabview;
} cp_state_t;

#define COUNT_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const cp_item_t kControlItems[] = {
	{ "Select", ID_TOOL_SELECT, GMAX_ICON_SELECT }, { "Move", ID_TOOL_MOVE, GMAX_ICON_MOVE },
	{ "Rotate", ID_TOOL_ROTATE, GMAX_ICON_ROTATE }, { "Scale", ID_TOOL_SCALE, GMAX_ICON_SCALE },
};

static const cp_item_t kShapeItems[] = {
	{ "Box", ID_CREATE_BOX, GMAX_ICON_BOX }, { "Sphere", ID_CREATE_SPHERE, GMAX_ICON_SPHERE },
	{ "Cylinder", ID_CREATE_CYLINDER, GMAX_ICON_CYLINDER }, { "Cone", ID_CREATE_CONE, GMAX_ICON_CONE },
	{ "Torus", ID_CREATE_TORUS, GMAX_ICON_TORUS }, { "Prism", ID_CREATE_PRISM, GMAX_ICON_PRISM },
	{ "Capsule", ID_CREATE_CAPSULE, GMAX_ICON_CAPSULE }, { "Arch", ID_CREATE_ARCH, GMAX_ICON_ARCH },
};

static const cp_item_t kSceneItems[] = {
	{ "Point Light", ID_CREATE_POINT_LIGHT, GMAX_ICON_POINT_LIGHT },
	{ "Directional", ID_CREATE_DIRECTIONAL_LIGHT, GMAX_ICON_DIR_LIGHT },
	{ "Camera", ID_CREATE_CAMERA, GMAX_ICON_CAMERA },
};

static const cp_item_t kModifierItems[] = {
	{ "Taper", ID_MODIFY_TAPER, GMAX_ICON_TAPER }, { "Twist", ID_MODIFY_TWIST, GMAX_ICON_TWIST },
	{ "Bend", ID_MODIFY_BEND, GMAX_ICON_BEND }, { "Stretch", ID_MODIFY_STRETCH, GMAX_ICON_STRETCH },
	{ "Skew", ID_MODIFY_SKEW, GMAX_ICON_SKEW }, { "Extrude", ID_MODIFY_EXTRUDE, GMAX_ICON_EXTRUDE },
	{ "Mirror", ID_MODIFY_MIRROR, GMAX_ICON_MIRROR }, { "Noise", ID_MODIFY_NOISE, GMAX_ICON_NOISE },
	{ "Shell", ID_MODIFY_SHELL, GMAX_ICON_SHELL }, { "Array", ID_MODIFY_ARRAY, GMAX_ICON_ARRAY },
};

static const cp_section_t kCreateSections[] = {
	{ "Controls", kControlItems, COUNT_OF(kControlItems) },
	{ "Shapes", kShapeItems, COUNT_OF(kShapeItems) },
	{ "Lights and Cameras", kSceneItems, COUNT_OF(kSceneItems) },
};

static const cp_section_t kModifySections[] = {
	{ "Modifiers", kModifierItems, COUNT_OF(kModifierItems) },
};

static const cp_tab_t kTabs[] = {
	{ "Create", ID_CP_TAB_CREATE, GMAX_ICON_TAB_CREATE, kCreateSections, COUNT_OF(kCreateSections) },
	{ "Modify", ID_CP_TAB_MODIFY, GMAX_ICON_TAB_MODIFY, kModifySections, COUNT_OF(kModifySections) },
	{ "Hierarchy", ID_CP_TAB_HIERARCHY, GMAX_ICON_TAB_HIERARCHY, NULL, 0 },
	{ "Motion", ID_CP_TAB_MOTION, GMAX_ICON_TAB_MOTION, NULL, 0 },
	{ "Display", ID_CP_TAB_DISPLAY, GMAX_ICON_TAB_DISPLAY, NULL, 0 },
	{ "Utilities", ID_CP_TAB_UTILITIES, GMAX_ICON_TAB_UTILITIES, NULL, 0 },
};

static uint32_t cp_load_icons(void) {
	const char *found = NULL;
	char bundled[1024];
#ifdef SHAREDIR
	int n = snprintf(bundled, sizeof(bundled), "%s/" SHAREDIR "/gmax-icons-24.png", ui_get_exe_dir());
	if (n > 0 && (size_t)n < sizeof(bundled)) {
		FILE *f = fopen(bundled, "rb");
		if (f) { fclose(f); found = bundled; }
	}
#endif
	if (!found) {
		static const char *source = "apps/scener/share/gmax-icons-24.png";
		FILE *f = fopen(source, "rb");
		if (f) { fclose(f); found = source; }
	}
	if (!found) return 0;
	int w = 0, h = 0;
	uint8_t *pixels = load_image(found, &w, &h);
	if (!pixels || w != GMAX_ICON_SHEET_W || h != GMAX_ICON_SHEET_H) {
		if (pixels) image_free(pixels);
		return 0;
	}
	uint32_t texture = R_CreateTextureRGBA(w, h, pixels, R_FILTER_NEAREST, R_WRAP_CLAMP);
	image_free(pixels);
	return texture;
}

static void cp_draw_header(const char *label, int *y, int width) {
	draw_text_small(label, 6, *y + 5, get_sys_color(brTextNormal));
	fill_rect(get_sys_color(brDarkEdge), R(4, *y + CP_HEADER_HEIGHT - 2, width - 8, 1));
	*y += CP_HEADER_HEIGHT;
}

static void cp_draw_item(const cp_item_t *item, irect16_t rect) {
	bool active = item->id >= ID_TOOL_SELECT && item->id <= ID_TOOL_SCALE && scener_active_tool() == item->id;
	draw_button(rect, 0, 0, active);
	draw_text(FONT_SMALL, item->label, rect.x + 7, rect.y + (rect.h - text_char_height(FONT_SMALL)) / 2, get_sys_color(brTextNormal));
}

static const cp_item_t *cp_hit_item(const cp_tab_t *tab, int mx, int my, int width) {
	int y = CP_HEADER_HEIGHT + 4;
	int button_w = (width - 10) / 2;
	for (int s = 0; s < tab->count; s++) {
		const cp_section_t *section = &tab->sections[s];
		y += 4 + CP_HEADER_HEIGHT;
		for (int i = 0; i < section->count; i++) {
			int col = i % 2, x = 4 + col * (button_w + 2);
			if (mx >= x && mx < x + button_w && my >= y && my < y + CP_BUTTON_HEIGHT)
				return &section->items[i];
			if (col || i == section->count - 1) y += CP_BUTTON_HEIGHT + 2;
		}
	}
	return NULL;
}

static result_t win_cp_page(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
	(void)wparam;
	cp_page_state_t *ps = (cp_page_state_t *)win->userdata;
	switch (msg) {
		case evCreate:
			ps = allocate_window_data(win, sizeof(*ps));
			if (!ps) return false;
			ps->tab = (const cp_tab_t *)lparam;
			return true;
		case evPaint: {
			if (!ps || !ps->tab) return false;
			irect16_t cr = get_client_rect(win);
			fill_rect(get_sys_color(brControlBg), cr);
			const cp_tab_t *tab = ps->tab;
			int y = 4;
			cp_draw_header(tab->label, &y, cr.w);
			if (!tab->count) {
				draw_text_small("No controls available", 8, y + 8, get_sys_color(brTextDisabled));
				return true;
			}
			int button_w = (cr.w - 10) / 2;
			for (int s = 0; s < tab->count; s++) {
				const cp_section_t *section = &tab->sections[s];
				y += 4;
				cp_draw_header(section->label, &y, cr.w);
				for (int i = 0; i < section->count; i++) {
					int col = i % 2;
					irect16_t rect = { (int16_t)(4 + col * (button_w + 2)), (int16_t)y, (int16_t)button_w, CP_BUTTON_HEIGHT };
					cp_draw_item(&section->items[i], rect);
					if (col || i == section->count - 1) y += CP_BUTTON_HEIGHT + 2;
				}
			}
			return true;
		}
		case evLeftButtonDown: {
			if (!ps || !ps->tab) return false;
			int mx = (int16_t)LOWORD(wparam), my = (int16_t)HIWORD(wparam);
			irect16_t cr = get_client_rect(win);
			const cp_item_t *item = cp_hit_item(ps->tab, mx, my, cr.w);
			if (item) handle_menu_command(item->id);
			return true;
		}
		default:
			return false;
	}
}

result_t win_command_panel(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
	(void)lparam;
	cp_state_t *st = (cp_state_t *)win->userdata;
	switch (msg) {
		case evCreate: {
			st = calloc(1, sizeof(*st));
			if (!st) return false;
			win->userdata = st;
			st->icons = cp_load_icons();
			if (st->icons) st->strip = (bitmap_strip_t){
				.tex = st->icons, .icon_w = GMAX_ICON_SIZE, .icon_h = GMAX_ICON_SIZE,
				.cols = GMAX_ICON_COLS, .sheet_w = GMAX_ICON_SHEET_W, .sheet_h = GMAX_ICON_SHEET_H,
			};
			st->tabview = create_window("", WINDOW_NOTITLE | WINDOW_NORESIZE,
				MAKERECT(0, 0, 1, 1), win, "TabView", 0, NULL);
			if (!st->tabview) return false;
			if (st->icons) {
				send_message(st->tabview, tcSetImageStrip, 0, &st->strip);
				for (int i = 0; i < COUNT_OF(kTabs); i++)
					send_message(st->tabview, tcSetTabIcon, i, (void*)(intptr_t)kTabs[i].icon);
			}
			for (int i = 0; i < COUNT_OF(kTabs); i++) {
				window_t *page = create_window(kTabs[i].label, WINDOW_NOTITLE,
					MAKERECT(0, 0, 1, 1), st->tabview, win_cp_page, 0, (void *)&kTabs[i]);
				if (page) show_window(page, true);
			}
			show_window(st->tabview, true);
			return true;
		}
		case evPaint: {
			if (!st) return false;
			irect16_t cr = get_client_rect(win);
			fill_rect(get_sys_color(brControlBg), cr);
			for (window_t *c = win->children; c; c = c->next)
				send_message(c, evPaint, 0, NULL);
			return true;
		}
		case evResize: {
			if (!st) return false;
			irect16_t cr = get_client_rect(win);
			for (window_t *c = win->children; c; c = c->next) {
				layout_arrange_t a = {R(0, 0, cr.w, cr.h)};
				send_message(c, evArrange, 0, &a);
			}
			return true;
		}
		case evCommand: {
			if (HIWORD(wparam) == tcnSelChange) return true;
			uint16_t id = LOWORD(wparam);
			for (int i = 0; i < COUNT_OF(kTabs); i++) if (kTabs[i].id == id) {
				if (st && st->tabview) send_message(st->tabview, tcSetSelection, i, NULL);
				return true;
			}
			return false;
		}
		case evDestroy:
			if (st) {
				R_DeleteTexture(st->icons);
				free(st);
				win->userdata = NULL;
			}
			if (g_app) g_app->command_panel_win = NULL;
			return false;
		default:
			return false;
	}
}

window_t *create_command_panel_window(void) {
	if (!g_app) return NULL;
	int sw = ui_get_system_metrics(kSystemMetricScreenWidth);
	int sh = ui_get_system_metrics(kSystemMetricScreenHeight);
	int win_h = (int)((sh - MENUBAR_HEIGHT - TOOLBAR_BAND_HEIGHT - 40) * 0.60f);
	window_t *win = create_window("Command Panel",
		WINDOW_ALWAYSONTOP | WINDOW_NOTRAYBUTTON | WINDOW_NORESIZE,
		MAKERECT(sw - CP_WIDTH, MENUBAR_HEIGHT + TOOLBAR_BAND_HEIGHT, CP_WIDTH, win_h),
		NULL, win_command_panel, g_app->hinstance, NULL);
	if (win) show_window(win, true);
	return win;
}
