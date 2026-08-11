#include "scener.h"
#include <orion/kernel/renderer.h>
#include <orion/user/draw.h>
#include <orion/user/image.h>
#include "gmax_icons.h"

enum {
	CP_WIDTH = SIDE_PANEL_WIDTH,
	CP_BUTTON_HEIGHT = 20,
};

typedef void (*cp_action_fn)(void *context, uint16_t id);
typedef struct {
	const char *label;
	uint16_t id;
	int icon;
	cp_action_fn action;
	void *context;
} cp_command_t;
typedef struct { const char *label; const cp_command_t *commands; int count; } cp_section_t;
typedef struct { const char *label; uint16_t id; int icon; const cp_section_t *sections; int count; } cp_tab_t;
typedef struct { const cp_tab_t *tabs; int count; } cp_datasource_t;
typedef struct {
	uint32_t icons;
	bitmap_strip_t strip;
	window_t *tabview;
	const cp_datasource_t *datasource;
} cp_state_t;

#define COUNT_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

static void cp_menu_action(void *context, uint16_t id) {
	(void)context;
	handle_menu_command(id);
}

#define CP_MENU_COMMAND(label, id, icon) { label, id, icon, cp_menu_action, NULL }

static const cp_command_t kControlItems[] = {
	CP_MENU_COMMAND("Select", ID_TOOL_SELECT, GMAX_ICON_SELECT), CP_MENU_COMMAND("Move", ID_TOOL_MOVE, GMAX_ICON_MOVE),
	CP_MENU_COMMAND("Rotate", ID_TOOL_ROTATE, GMAX_ICON_ROTATE), CP_MENU_COMMAND("Scale", ID_TOOL_SCALE, GMAX_ICON_SCALE),
};

static const cp_command_t kShapeItems[] = {
	CP_MENU_COMMAND("Box", ID_CREATE_BOX, GMAX_ICON_BOX), CP_MENU_COMMAND("Sphere", ID_CREATE_SPHERE, GMAX_ICON_SPHERE),
	CP_MENU_COMMAND("Cylinder", ID_CREATE_CYLINDER, GMAX_ICON_CYLINDER), CP_MENU_COMMAND("Cone", ID_CREATE_CONE, GMAX_ICON_CONE),
	CP_MENU_COMMAND("Torus", ID_CREATE_TORUS, GMAX_ICON_TORUS), CP_MENU_COMMAND("Prism", ID_CREATE_PRISM, GMAX_ICON_PRISM),
	CP_MENU_COMMAND("Capsule", ID_CREATE_CAPSULE, GMAX_ICON_CAPSULE), CP_MENU_COMMAND("Arch", ID_CREATE_ARCH, GMAX_ICON_ARCH),
};

static const cp_command_t kSceneItems[] = {
	CP_MENU_COMMAND("Point Light", ID_CREATE_POINT_LIGHT, GMAX_ICON_POINT_LIGHT),
	CP_MENU_COMMAND("Directional", ID_CREATE_DIRECTIONAL_LIGHT, GMAX_ICON_DIR_LIGHT),
	CP_MENU_COMMAND("Camera", ID_CREATE_CAMERA, GMAX_ICON_CAMERA),
};

static const cp_command_t kModifierItems[] = {
	CP_MENU_COMMAND("Taper", ID_MODIFY_TAPER, GMAX_ICON_TAPER), CP_MENU_COMMAND("Twist", ID_MODIFY_TWIST, GMAX_ICON_TWIST),
	CP_MENU_COMMAND("Bend", ID_MODIFY_BEND, GMAX_ICON_BEND), CP_MENU_COMMAND("Stretch", ID_MODIFY_STRETCH, GMAX_ICON_STRETCH),
	CP_MENU_COMMAND("Skew", ID_MODIFY_SKEW, GMAX_ICON_SKEW), CP_MENU_COMMAND("Extrude", ID_MODIFY_EXTRUDE, GMAX_ICON_EXTRUDE),
	CP_MENU_COMMAND("Mirror", ID_MODIFY_MIRROR, GMAX_ICON_MIRROR), CP_MENU_COMMAND("Noise", ID_MODIFY_NOISE, GMAX_ICON_NOISE),
	CP_MENU_COMMAND("Shell", ID_MODIFY_SHELL, GMAX_ICON_SHELL), CP_MENU_COMMAND("Array", ID_MODIFY_ARRAY, GMAX_ICON_ARRAY),
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

static const cp_datasource_t kCommandPanelDataSource = {
	.tabs = kTabs, .count = COUNT_OF(kTabs),
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

static window_t *cp_create_page(window_t *tabview, const cp_tab_t *tab) {
	layout_view_config_t stack_cfg = { .spacing = 4 };
	window_t *page = create_window(tab->label, WINDOW_NOTITLE,
		MAKERECT(0, 0, 1, 1), tabview, "StackView", 0, &stack_cfg);
	if (!page) return NULL;
	for (int s = 0; s < tab->count; s++) {
		const cp_section_t *section = &tab->sections[s];
		window_t *section_win = create_window("", WINDOW_NOTITLE,
			MAKERECT(0, 0, 1, 1), page, "StackView", 0, &stack_cfg);
		if (!section_win) continue;
		create_window(section->label, WINDOW_NOTITLE,
			MAKERECT(0, 0, 1, CONTROL_HEIGHT), section_win, "Label", 0, NULL);
		layout_view_config_t grid_cfg = { .spacing = 4 };
		window_t *grid = create_window("", WINDOW_NOTITLE,
			MAKERECT(0, 0, 1, 1), section_win, "GridView", 0, &grid_cfg);
		if (!grid) continue;
		send_message(grid, evInitChildren, 0, NULL);
		window_t *columns[2] = { grid->children, grid->children ? grid->children->next : NULL };
		for (int i = 0; i < section->count; i++) {
			window_t *column = columns[i % 2];
			if (!column) continue;
			const cp_command_t *command = &section->commands[i];
			window_t *button = create_window(command->label,
				WINDOW_NOTITLE | WINDOW_FLEXSPACE, MAKERECT(0, 0, 1, CP_BUTTON_HEIGHT),
				column, "Button", 0, NULL);
			if (button) button->id = command->id;
		}
		send_message(grid, evResize, 0, NULL);
	}
	if (!tab->count)
		create_window("No controls available", WINDOW_NOTITLE,
			MAKERECT(0, 0, 1, CONTROL_HEIGHT), page, "Label", 0, NULL);
	send_message(page, evResize, 0, NULL);
	return page;
}

static const cp_command_t *cp_find_command(const cp_datasource_t *source, uint16_t id) {
	if (!source) return NULL;
	for (int t = 0; t < source->count; t++)
		for (int s = 0; s < source->tabs[t].count; s++)
			for (int i = 0; i < source->tabs[t].sections[s].count; i++) {
				const cp_command_t *command = &source->tabs[t].sections[s].commands[i];
				if (command->id == id) return command;
			}
	return NULL;
}

result_t win_command_panel(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
	(void)lparam;
	cp_state_t *st = (cp_state_t *)win->userdata;
	switch (msg) {
		case evCreate: {
			st = calloc(1, sizeof(*st));
			if (!st) return false;
			win->userdata = st;
			st->datasource = &kCommandPanelDataSource;
			st->icons = cp_load_icons();
			if (st->icons) st->strip = (bitmap_strip_t){
				.tex = st->icons, .icon_w = GMAX_ICON_SIZE, .icon_h = GMAX_ICON_SIZE,
				.cols = GMAX_ICON_COLS, .sheet_w = GMAX_ICON_SHEET_W, .sheet_h = GMAX_ICON_SHEET_H,
			};
			st->tabview = create_window("", WINDOW_NOTITLE | WINDOW_NORESIZE,
				MAKERECT(0, 0, 1, 1), win, "TabView", 0, NULL);
			if (!st->tabview) return false;
			for (int i = 0; i < st->datasource->count; i++) {
				window_t *page = cp_create_page(st->tabview, &st->datasource->tabs[i]);
				if (page) show_window(page, true);
			}
			send_message(st->tabview, tcSetStyle, TAB_STYLE_ICONS_ONLY, NULL);
			if (st->icons) {
				send_message(st->tabview, tcSetImageStrip, 0, &st->strip);
				for (int i = 0; i < st->datasource->count; i++)
					send_message(st->tabview, tcSetTabIcon, i, (void*)(intptr_t)st->datasource->tabs[i].icon);
			}
			irect16_t cr = get_client_rect(win);
			layout_arrange_t a = {R(0, 0, cr.w, cr.h)};
			send_message(st->tabview, evArrange, 0, &a);
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
			for (int i = 0; st && i < st->datasource->count; i++) if (st->datasource->tabs[i].id == id) {
				if (st && st->tabview) send_message(st->tabview, tcSetSelection, i, NULL);
				return true;
			}
			const cp_command_t *command = cp_find_command(st ? st->datasource : NULL, id);
			if (command && command->action) {
				command->action(command->context, command->id);
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
