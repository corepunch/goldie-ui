#include "scener.h"
#include <orion/kernel/renderer.h>
#include <orion/user/draw.h>
#include <orion/user/image.h>
#include "gmax_icons.h"

enum {
	CP_WIDTH = 280,
	CP_TAB_HEIGHT = 28,
	CP_HEADER_HEIGHT = 24,
	CP_BUTTON_HEIGHT = 28,
};

typedef struct { const char *label; uint16_t id; int icon; } cp_item_t;
typedef struct { const char *label; const cp_item_t *items; int count; } cp_section_t;
typedef struct { const char *label; uint16_t id; int icon; const cp_section_t *sections; int count; } cp_tab_t;
typedef struct {
	int active_tab;
	uint32_t icons;
	bitmap_strip_t strip;
	window_t *tab_buttons[6];
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

static void cp_draw_icon(uint32_t texture, int index, irect16_t rect) {
	if (!texture || index < 0) return;
	int col = index % GMAX_ICON_COLS, row = index / GMAX_ICON_COLS;
	float u0 = (float)(col * GMAX_ICON_SIZE) / GMAX_ICON_SHEET_W;
	float v0 = (float)(row * GMAX_ICON_SIZE) / GMAX_ICON_SHEET_H;
	float u1 = (float)((col + 1) * GMAX_ICON_SIZE) / GMAX_ICON_SHEET_W;
	float v1 = (float)((row + 1) * GMAX_ICON_SIZE) / GMAX_ICON_SHEET_H;
	draw_sprite_region((int)texture, rect, UV_RECT(u0, v0, u1, v1), 0xffffffff, 0);
}

static void cp_layout_tabs(window_t *win, cp_state_t *st) {
	irect16_t cr = get_client_rect(win);
	int tab_w = cr.w / COUNT_OF(kTabs);
	for (int i = 0; i < COUNT_OF(kTabs); i++) if (st->tab_buttons[i]) {
		window_t *button = st->tab_buttons[i];
		button->frame.x = i * tab_w;
		button->frame.y = 0;
		button->frame.w = i == COUNT_OF(kTabs) - 1 ? cr.w - i * tab_w : tab_w;
		button->frame.h = CP_TAB_HEIGHT;
	}
}

static void cp_create_tabs(window_t *win, cp_state_t *st) {
	st->strip = (bitmap_strip_t){
		.tex = st->icons, .icon_w = GMAX_ICON_SIZE, .icon_h = GMAX_ICON_SIZE,
		.cols = GMAX_ICON_COLS, .sheet_w = GMAX_ICON_SHEET_W, .sheet_h = GMAX_ICON_SHEET_H,
	};
	for (int i = 0; i < COUNT_OF(kTabs); i++) {
		st->tab_buttons[i] = create_window(kTabs[i].label,
			WINDOW_NOTITLE | WINDOW_NORESIZE | BUTTON_PUSHLIKE | BUTTON_AUTORADIO,
			MAKERECT(0, 0, 1, CP_TAB_HEIGHT), win, win_toolbar_button, 0, NULL);
		if (!st->tab_buttons[i]) continue;
		st->tab_buttons[i]->id = kTabs[i].id;
		if (st->icons) send_message(st->tab_buttons[i], btnSetImage, kTabs[i].icon, &st->strip);
		show_window(st->tab_buttons[i], true);
	}
	cp_layout_tabs(win, st);
	if (st->tab_buttons[0]) send_message(st->tab_buttons[0], btnSetCheck, btnStateChecked, NULL);
}

static void cp_draw_header(const char *label, int *y, int width) {
	draw_text_small(label, 6, *y + 5, get_sys_color(brTextNormal));
	fill_rect(get_sys_color(brDarkEdge), R(4, *y + CP_HEADER_HEIGHT - 2, width - 8, 1));
	*y += CP_HEADER_HEIGHT;
}

static void cp_draw_item(cp_state_t *st, const cp_item_t *item, irect16_t rect) {
	bool active = g_app && item->id >= ID_TOOL_SELECT && item->id <= ID_TOOL_SCALE && g_app->current_tool == item->id;
	draw_button(rect, 0, 0, active);
	int text_x = rect.x + 7;
	if (item->icon >= 0 && st->icons) {
		cp_draw_icon(st->icons, item->icon, R(rect.x + 3, rect.y + 2, GMAX_ICON_SIZE, GMAX_ICON_SIZE));
		text_x += GMAX_ICON_SIZE;
	}
	draw_text_small(item->label, text_x, rect.y + (rect.h - CHAR_HEIGHT) / 2, get_sys_color(brTextNormal));
}

static void cp_draw_content(cp_state_t *st, int width) {
	const cp_tab_t *tab = &kTabs[st->active_tab];
	int y = CP_TAB_HEIGHT + 4;
	cp_draw_header(tab->label, &y, width);
	if (!tab->count) {
		draw_text_small("No controls available", 8, y + 8, get_sys_color(brTextDisabled));
		return;
	}
	int button_w = (width - 10) / 2;
	for (int s = 0; s < tab->count; s++) {
		const cp_section_t *section = &tab->sections[s];
		y += 4;
		cp_draw_header(section->label, &y, width);
		for (int i = 0; i < section->count; i++) {
			int col = i % 2;
			irect16_t rect = { (int16_t)(4 + col * (button_w + 2)), (int16_t)y, (int16_t)button_w, CP_BUTTON_HEIGHT };
			cp_draw_item(st, &section->items[i], rect);
			if (col || i == section->count - 1) y += CP_BUTTON_HEIGHT + 2;
		}
	}
}

static const cp_item_t *cp_hit_item(cp_state_t *st, int mx, int my, int width) {
	const cp_tab_t *tab = &kTabs[st->active_tab];
	int y = CP_TAB_HEIGHT + 4 + CP_HEADER_HEIGHT;
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

result_t win_command_panel(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
	(void)lparam;
	cp_state_t *st = (cp_state_t *)win->userdata;
	switch (msg) {
		case evCreate:
			st = calloc(1, sizeof(*st));
			if (!st) return false;
			win->userdata = st;
			st->icons = cp_load_icons();
			cp_create_tabs(win, st);
			return true;
		case evPaint: {
			if (!st) return false;
			irect16_t cr = get_client_rect(win);
			fill_rect(get_sys_color(brWindowBg), cr);
			cp_draw_content(st, cr.w);
			return false;
		}
		case evLeftButtonDown: {
			if (!st) return false;
			int mx = (int16_t)LOWORD(wparam), my = (int16_t)HIWORD(wparam);
			irect16_t cr = get_client_rect(win);
			const cp_item_t *item = cp_hit_item(st, mx, my, cr.w);
			if (item) handle_menu_command(item->id);
			return true;
		}
		case evResize:
			if (st) cp_layout_tabs(win, st);
			return true;
		case tbButtonClick: {
			uint16_t id = (uint16_t)wparam;
			for (int i = 0; i < COUNT_OF(kTabs); i++) if (kTabs[i].id == id) {
				st->active_tab = i;
				invalidate_window(win);
				return true;
			}
			return false;
		}
		case evCommand: {
			uint16_t id = LOWORD(wparam);
			for (int i = 0; i < COUNT_OF(kTabs); i++) if (kTabs[i].id == id) {
				st->active_tab = i;
				if (st->tab_buttons[i]) send_message(st->tab_buttons[i], btnSetCheck, btnStateChecked, NULL);
				invalidate_window(win);
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
	int win_h = sh - MENUBAR_HEIGHT - TOOLBAR_BAND_HEIGHT - 40;
	window_t *win = create_window("Command Panel",
		WINDOW_ALWAYSONTOP | WINDOW_NOTRAYBUTTON | WINDOW_NORESIZE,
		MAKERECT(sw - CP_WIDTH, MENUBAR_HEIGHT + TOOLBAR_BAND_HEIGHT, CP_WIDTH, win_h),
		NULL, win_command_panel, g_app->hinstance, NULL);
	if (win) show_window(win, true);
	return win;
}
