// SVG icon strip loader — rasterizes iconoir SVGs into bitmap_strip_t at startup.
// nanosvg implementation is compiled here (single TU).

#define NANOSVG_IMPLEMENTATION
#include "tools/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "tools/nanosvgrast.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <platform/platform.h>
#include "svg_icon_loader.h"
#include "icons.h"
#include "sysicons.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Replace all occurrences of "currentColor" (12 bytes) in-place so iconoir SVGs
// rasterize white, ready to be tinted at draw time. Must be a same-length
// replacement. A named color padded with spaces fails: nanosvg's color parser
// (nsvg__parseColorName) strcmp()s against the raw value with no trailing-space
// trim, so "white       " falls through to the gray fallback (128,128,128).
// A hex literal works because nsvg__parseColorHex uses sscanf("#%2x%2x%2x"),
// which stops cleanly at the padding spaces.
static void patch_current_color(char *svg) {
    const char needle[]  = "currentColor";
    const char replace[] = "#ffffff     ";  // 12 chars each
    const size_t n = sizeof(needle) - 1;
    char *p = svg;
    while ((p = strstr(p, needle)) != NULL) {
        memcpy(p, replace, n);
        p += n;
    }
}

// Rasterize one SVG file into `out_rgba` (icon_size × icon_size × 4 bytes, RGBA).
// Returns true on success.  The tile is centered inside the square tile.
static bool rasterize_svg(const char *path, int size, uint8_t *out_rgba) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    if (file_size <= 0 || file_size > 512 * 1024) {
        fclose(f);
        return false;
    }

    char *buf = malloc((size_t)file_size + 1);
    if (!buf) { fclose(f); return false; }

    size_t read = fread(buf, 1, (size_t)file_size, f);
    fclose(f);
    buf[read] = '\0';

    patch_current_color(buf);

    // nsvgParse modifies buf in-place; we pass a copy so we can still free buf.
    NSVGimage *img = nsvgParse(buf, "px", 96.0f);
    free(buf);

    if (!img || img->width <= 0.0f || img->height <= 0.0f) {
        if (img) nsvgDelete(img);
        return false;
    }

    NSVGrasterizer *rast = nsvgCreateRasterizer();
    if (!rast) { nsvgDelete(img); return false; }

    float scale = fminf((float)size / img->width, (float)size / img->height);
    float tx    = ((float)size - img->width  * scale) * 0.5f;
    float ty    = ((float)size - img->height * scale) * 0.5f;

    memset(out_rgba, 0, (size_t)size * size * 4);
    nsvgRasterize(rast, img, tx, ty, scale, out_rgba, size, size, size * 4);

    nsvgDeleteRasterizer(rast);
    nsvgDelete(img);
    return true;
}

// ---------------------------------------------------------------------------
// Public: generic strip builder
// ---------------------------------------------------------------------------

bool svg_build_strip(const char *icons_dir,
                     const char **svg_names, int count,
                     int icon_size, int cols,
                     bitmap_strip_t *out,
                     FILE *missing) {
    if (!icons_dir || !svg_names || count <= 0 || icon_size <= 0 || cols <= 0 || !out)
        return false;

    int rows    = (count + cols - 1) / cols;
    int sheet_w = cols * icon_size;
    int sheet_h = rows * icon_size;

    uint8_t *sheet = calloc((size_t)sheet_w * sheet_h * 4, 1);
    if (!sheet) return false;

    uint8_t *tile = malloc((size_t)icon_size * icon_size * 4);
    if (!tile) { free(sheet); return false; }

    int ok_count = 0;
    for (int i = 0; i < count; i++) {
        const char *name  = svg_names[i];
        bool drawn = false;
        if (name && name[0]) {
            char path[4096];
            snprintf(path, sizeof(path), "%s/%s.svg", icons_dir, name);
            drawn = rasterize_svg(path, icon_size, tile);
            if (drawn) {
                ok_count++;
            } else if (missing) {
                fprintf(missing, "MISSING icon[%d] \"%s\"\n", i, name);
            }
        } else if (missing) {
            fprintf(missing, "UNMAPPED icon[%d]\n", i);
        }

        if (!drawn) memset(tile, 0, (size_t)icon_size * icon_size * 4);

        int col = i % cols;
        int row = i / cols;
        for (int y = 0; y < icon_size; y++) {
            memcpy(sheet + ((size_t)(row * icon_size + y) * sheet_w + col * icon_size) * 4,
                   tile  + (size_t)y * icon_size * 4,
                   (size_t)icon_size * 4);
        }
    }

    free(tile);

    // Refuse to upload a fully-blank sheet: if no SVGs were found the caller
    // should fall back to its PNG (or accept an empty strip).
    if (ok_count == 0) {
        free(sheet);
        return false;
    }

    uint32_t tex = R_CreateTextureRGBA(sheet_w, sheet_h, (uint8_t *)sheet,
                                       R_FILTER_NEAREST, R_WRAP_CLAMP);
    free(sheet);
    if (!tex) return false;

    out->tex     = tex;
    out->icon_w  = icon_size;
    out->icon_h  = icon_size;
    out->cols    = cols;
    out->sheet_w = sheet_w;
    out->sheet_h = sheet_h;
    return true;
}

// ---------------------------------------------------------------------------
// Sysicon mapping  (icons.h  sysicon_* enum, 16x16)
// ---------------------------------------------------------------------------

// Maps sysicon enum index (offset from SYSICON_BASE) → iconoir SVG base name.
// NULL means no iconoir equivalent; the tile is left blank and logged.
static const char *k_sysicon_names[] = {
    [sysicon_2d                         - SYSICON_BASE] = NULL,
    [sysicon_accept                     - SYSICON_BASE] = "check",
    [sysicon_add                        - SYSICON_BASE] = "plus",
    [sysicon_ambient_occlusion          - SYSICON_BASE] = NULL,
    [sysicon_ambient_occlusion_voxel    - SYSICON_BASE] = NULL,
    [sysicon_ammo                       - SYSICON_BASE] = NULL,
    [sysicon_angle                      - SYSICON_BASE] = NULL,
    [sysicon_anim_frame                 - SYSICON_BASE] = NULL,
    [sysicon_anim_frame_add             - SYSICON_BASE] = NULL,
    [sysicon_anim_frame_character       - SYSICON_BASE] = NULL,
    [sysicon_anim_frame_delete          - SYSICON_BASE] = NULL,
    [sysicon_anim_frame_duplicate       - SYSICON_BASE] = NULL,
    [sysicon_anim_frame_edit            - SYSICON_BASE] = NULL,
    [sysicon_anim_frame_move_left       - SYSICON_BASE] = NULL,
    [sysicon_anim_frame_move_right      - SYSICON_BASE] = NULL,
    [sysicon_anim_frame_speed           - SYSICON_BASE] = NULL,
    [sysicon_anvil_in                   - SYSICON_BASE] = NULL,
    [sysicon_anvil_out                  - SYSICON_BASE] = NULL,
    [sysicon_arguments                  - SYSICON_BASE] = NULL,
    [sysicon_arrow_blue_first           - SYSICON_BASE] = "fast-arrow-left",
    [sysicon_arrow_blue_last            - SYSICON_BASE] = "fast-arrow-right",
    [sysicon_arrow_blue_left            - SYSICON_BASE] = "arrow-left",
    [sysicon_arrow_blue_right           - SYSICON_BASE] = "arrow-right",
    [sysicon_arrow_branch               - SYSICON_BASE] = "git-fork",
    [sysicon_arrow_down                 - SYSICON_BASE] = "arrow-down",
    [sysicon_arrow_in                   - SYSICON_BASE] = "log-in",
    [sysicon_arrow_in_out               - SYSICON_BASE] = "data-transfer-both",
    [sysicon_arrow_left                 - SYSICON_BASE] = "arrow-left",
    [sysicon_arrow_left_right           - SYSICON_BASE] = NULL,
    [sysicon_arrow_lower                - SYSICON_BASE] = "arrow-down",
    [sysicon_arrow_lower_step           - SYSICON_BASE] = "arrow-separate-vertical",
    [sysicon_arrow_out                  - SYSICON_BASE] = "log-out",
    [sysicon_arrow_play                 - SYSICON_BASE] = "play",
    [sysicon_arrow_prompt               - SYSICON_BASE] = "forward",
    [sysicon_arrow_raise                - SYSICON_BASE] = "arrow-up",
    [sysicon_arrow_raise_step           - SYSICON_BASE] = NULL,
    [sysicon_arrow_refresh              - SYSICON_BASE] = "refresh",
    [sysicon_arrow_right                - SYSICON_BASE] = "arrow-right",
    [sysicon_arrow_rotate_anticlockwise - SYSICON_BASE] = "rotate-camera-left",
    [sysicon_arrow_rotate_clockwise     - SYSICON_BASE] = "rotate-camera-right",
    [sysicon_arrow_speed                - SYSICON_BASE] = NULL,
    [sysicon_arrow_switch               - SYSICON_BASE] = "switch-off",
    [sysicon_arrow_up                   - SYSICON_BASE] = "arrow-up",
    [sysicon_arrow_up_down              - SYSICON_BASE] = NULL,
    [sysicon_attach                     - SYSICON_BASE] = "attachment",
    [sysicon_autotile                   - SYSICON_BASE] = NULL,
    [sysicon_blip_orange                - SYSICON_BASE] = "circle",
    [sysicon_bomb                       - SYSICON_BASE] = NULL,
    [sysicon_book                       - SYSICON_BASE] = "book",
    [sysicon_boolean                    - SYSICON_BASE] = NULL,
    [sysicon_button                     - SYSICON_BASE] = "cursor-pointer",
    [sysicon_button_xbox_a              - SYSICON_BASE] = NULL,
    [sysicon_button_xbox_b              - SYSICON_BASE] = NULL,
    [sysicon_button_xbox_x              - SYSICON_BASE] = NULL,
    [sysicon_button_xbox_y              - SYSICON_BASE] = NULL,
    [sysicon_camera                     - SYSICON_BASE] = "camera",
    [sysicon_camera_add                 - SYSICON_BASE] = NULL,
    [sysicon_camera_edit                - SYSICON_BASE] = "camera",
    [sysicon_camera_first_person        - SYSICON_BASE] = NULL,
    [sysicon_camera_free                - SYSICON_BASE] = NULL,
    [sysicon_camera_isometric           - SYSICON_BASE] = NULL,
    [sysicon_camera_lock                - SYSICON_BASE] = NULL,
    [sysicon_camera_move                - SYSICON_BASE] = NULL,
    [sysicon_camera_reset               - SYSICON_BASE] = "rotate-camera-right",
    [sysicon_camera_rotate              - SYSICON_BASE] = "rotate-camera-left",
    [sysicon_character                  - SYSICON_BASE] = "user",
    [sysicon_character_add              - SYSICON_BASE] = "user-plus",
    [sysicon_character_dialogue         - SYSICON_BASE] = NULL,
    [sysicon_character_edit             - SYSICON_BASE] = "user-circle",
    [sysicon_character_heart            - SYSICON_BASE] = NULL,
    [sysicon_character_item_get         - SYSICON_BASE] = NULL,
    [sysicon_character_lock             - SYSICON_BASE] = "lock",
    [sysicon_character_move             - SYSICON_BASE] = NULL,
    [sysicon_character_npc              - SYSICON_BASE] = NULL,
    [sysicon_character_question         - SYSICON_BASE] = NULL,
    [sysicon_character_rotate           - SYSICON_BASE] = NULL,
    [sysicon_character_stop             - SYSICON_BASE] = NULL,
    [sysicon_checkbox                   - SYSICON_BASE] = "check-circle",
    [sysicon_checkbox_off               - SYSICON_BASE] = "circle",
    [sysicon_checkmark                  - SYSICON_BASE] = "check",
    [sysicon_chest                      - SYSICON_BASE] = NULL,
    [sysicon_chest_checkmark            - SYSICON_BASE] = NULL,
    [sysicon_chest_delete               - SYSICON_BASE] = NULL,
    [sysicon_chest_disable              - SYSICON_BASE] = NULL,
    [sysicon_chest_enable               - SYSICON_BASE] = NULL,
    [sysicon_chest_item                 - SYSICON_BASE] = NULL,
    [sysicon_clear                      - SYSICON_BASE] = "xmark",
    [sysicon_clipboard                  - SYSICON_BASE] = "paste-clipboard",
    [sysicon_clock                      - SYSICON_BASE] = "clock",
    [sysicon_clock_play                 - SYSICON_BASE] = NULL,
    [sysicon_clock_stop                 - SYSICON_BASE] = NULL,
    [sysicon_coins                      - SYSICON_BASE] = "coins",
    [sysicon_collapse                   - SYSICON_BASE] = "collapse",
    [sysicon_color_edit                 - SYSICON_BASE] = "color-filter",
    [sysicon_color_swatch               - SYSICON_BASE] = "palette",
    [sysicon_comment                    - SYSICON_BASE] = "chat-bubble",
    [sysicon_computer                   - SYSICON_BASE] = "computer",
    [sysicon_console                    - SYSICON_BASE] = "terminal",
    [sysicon_credits                    - SYSICON_BASE] = NULL,
    [sysicon_crosshair                  - SYSICON_BASE] = "precision-tool",
    [sysicon_cursor                     - SYSICON_BASE] = "cursor-pointer",
    [sysicon_delete                     - SYSICON_BASE] = "trash",
    [sysicon_dialogue                   - SYSICON_BASE] = "chat-lines",
    [sysicon_dialogue_add               - SYSICON_BASE] = NULL,
    [sysicon_dialogue_page              - SYSICON_BASE] = NULL,
    [sysicon_dialogue_start             - SYSICON_BASE] = NULL,
    [sysicon_dice                       - SYSICON_BASE] = NULL,
    [sysicon_discord_logo               - SYSICON_BASE] = "discord",
    [sysicon_disk                       - SYSICON_BASE] = "floppy-disk",
    [sysicon_disk_load                  - SYSICON_BASE] = "floppy-disk",
    [sysicon_disk_multiple              - SYSICON_BASE] = NULL,
    [sysicon_disk_save                  - SYSICON_BASE] = "floppy-disk",
    [sysicon_door                       - SYSICON_BASE] = NULL,
    [sysicon_enemy                      - SYSICON_BASE] = NULL,
    [sysicon_enemy_add                  - SYSICON_BASE] = NULL,
    [sysicon_entity                     - SYSICON_BASE] = NULL,
    [sysicon_entity_bounds              - SYSICON_BASE] = NULL,
    [sysicon_entity_delete              - SYSICON_BASE] = NULL,
    [sysicon_entity_move                - SYSICON_BASE] = NULL,
    [sysicon_exclamation                - SYSICON_BASE] = "warning-triangle",
    [sysicon_exit                       - SYSICON_BASE] = "log-out",
    [sysicon_expand                     - SYSICON_BASE] = "expand",
    [sysicon_expression                 - SYSICON_BASE] = NULL,
    [sysicon_eye                        - SYSICON_BASE] = "eye",
    [sysicon_eyedropper                 - SYSICON_BASE] = "color-picker",
    [sysicon_eye_height                 - SYSICON_BASE] = NULL,
    [sysicon_eye_hide                   - SYSICON_BASE] = "eye-closed",
    [sysicon_eye_lock                   - SYSICON_BASE] = NULL,
    [sysicon_eye_show                   - SYSICON_BASE] = "eye",
    [sysicon_fade_in                    - SYSICON_BASE] = NULL,
    [sysicon_fade_out                   - SYSICON_BASE] = NULL,
    [sysicon_flag_blue                  - SYSICON_BASE] = "triangle-flag",
    [sysicon_flip_horizontal            - SYSICON_BASE] = "mirror",
    [sysicon_folder                     - SYSICON_BASE] = "folder",
    [sysicon_folder_image               - SYSICON_BASE] = NULL,
    [sysicon_folder_page                - SYSICON_BASE] = NULL,
    [sysicon_folder_search              - SYSICON_BASE] = NULL,
    [sysicon_folder_settings            - SYSICON_BASE] = NULL,
    [sysicon_folder_up                  - SYSICON_BASE] = "folder-plus",
    [sysicon_folder_voxel               - SYSICON_BASE] = NULL,
    [sysicon_font                       - SYSICON_BASE] = "text",
    [sysicon_font_size                  - SYSICON_BASE] = NULL,
    [sysicon_fullscreen                 - SYSICON_BASE] = "expand",
    [sysicon_function                   - SYSICON_BASE] = "code-brackets",
    [sysicon_gear                       - SYSICON_BASE] = "settings",
    [sysicon_glow                       - SYSICON_BASE] = NULL,
    [sysicon_godot_logo                 - SYSICON_BASE] = NULL,
    [sysicon_gradient_red               - SYSICON_BASE] = NULL,
    [sysicon_grid                       - SYSICON_BASE] = "view-grid",
    [sysicon_grid_height                - SYSICON_BASE] = NULL,
    [sysicon_grid_tile                  - SYSICON_BASE] = "view-grid",
    [sysicon_grid_tile_place            - SYSICON_BASE] = NULL,
    [sysicon_ground                     - SYSICON_BASE] = NULL,
    [sysicon_group                      - SYSICON_BASE] = "group",
    [sysicon_group_add                  - SYSICON_BASE] = NULL,
    [sysicon_group_delete               - SYSICON_BASE] = NULL,
    [sysicon_group_edit                 - SYSICON_BASE] = NULL,
    [sysicon_group_mesh                 - SYSICON_BASE] = NULL,
    [sysicon_group_tiles                - SYSICON_BASE] = NULL,
    [sysicon_hand                       - SYSICON_BASE] = "drag-hand-gesture",
    [sysicon_hand_add                   - SYSICON_BASE] = NULL,
    [sysicon_hand_delete                - SYSICON_BASE] = NULL,
    [sysicon_hand_move                  - SYSICON_BASE] = "drag-hand-gesture",
    [sysicon_heart                      - SYSICON_BASE] = "heart",
    [sysicon_heart_add                  - SYSICON_BASE] = "heart",
    [sysicon_heart_delete               - SYSICON_BASE] = NULL,
    [sysicon_help                       - SYSICON_BASE] = "question-mark",
    [sysicon_hourglass                  - SYSICON_BASE] = "hourglass",
    [sysicon_image                      - SYSICON_BASE] = "media-image",
    [sysicon_image_add                  - SYSICON_BASE] = "media-image-plus",
    [sysicon_image_dimensions           - SYSICON_BASE] = NULL,
    [sysicon_image_hand                 - SYSICON_BASE] = NULL,
    [sysicon_import_export              - SYSICON_BASE] = "data-transfer-both",
    [sysicon_information                - SYSICON_BASE] = "info-circle",
    [sysicon_interact                   - SYSICON_BASE] = "cursor-pointer",
    [sysicon_inventory                  - SYSICON_BASE] = NULL,
    [sysicon_inventory_hide             - SYSICON_BASE] = NULL,
    [sysicon_inventory_show             - SYSICON_BASE] = NULL,
    [sysicon_itch_logo                  - SYSICON_BASE] = NULL,
    [sysicon_item                       - SYSICON_BASE] = NULL,
    [sysicon_item_add                   - SYSICON_BASE] = NULL,
    [sysicon_item_delete                - SYSICON_BASE] = NULL,
    [sysicon_item_drop                  - SYSICON_BASE] = NULL,
    [sysicon_item_stack                 - SYSICON_BASE] = NULL,
    [sysicon_joystick                   - SYSICON_BASE] = "gamepad",
    [sysicon_jump                       - SYSICON_BASE] = NULL,
    [sysicon_key_c                      - SYSICON_BASE] = NULL,
    [sysicon_key_t                      - SYSICON_BASE] = NULL,
    [sysicon_keyboard                   - SYSICON_BASE] = NULL,
    [sysicon_ladder                     - SYSICON_BASE] = NULL,
    [sysicon_ladder_character           - SYSICON_BASE] = NULL,
    [sysicon_ladder_down                - SYSICON_BASE] = NULL,
    [sysicon_ladder_lock                - SYSICON_BASE] = NULL,
    [sysicon_ladder_none                - SYSICON_BASE] = NULL,
    [sysicon_ladder_up                  - SYSICON_BASE] = NULL,
    [sysicon_ladder_up_down             - SYSICON_BASE] = NULL,
    [sysicon_laser                      - SYSICON_BASE] = NULL,
    [sysicon_library                    - SYSICON_BASE] = "book-stack",
    [sysicon_light                      - SYSICON_BASE] = "sun-light",
    [sysicon_light_add                  - SYSICON_BASE] = NULL,
    [sysicon_light_energy               - SYSICON_BASE] = NULL,
    [sysicon_light_fade                 - SYSICON_BASE] = NULL,
    [sysicon_light_negative             - SYSICON_BASE] = NULL,
    [sysicon_light_off                  - SYSICON_BASE] = "brightness",
    [sysicon_light_on_off               - SYSICON_BASE] = NULL,
    [sysicon_light_range                - SYSICON_BASE] = NULL,
    [sysicon_link                       - SYSICON_BASE] = "link",
    [sysicon_link_add                   - SYSICON_BASE] = NULL,
    [sysicon_link_box                   - SYSICON_BASE] = NULL,
    [sysicon_link_delete                - SYSICON_BASE] = "link-slash",
    [sysicon_link_eye                   - SYSICON_BASE] = NULL,
    [sysicon_linux_logo                 - SYSICON_BASE] = "linux",
    [sysicon_loading_screen             - SYSICON_BASE] = NULL,
    [sysicon_location                   - SYSICON_BASE] = "map-pin",
    [sysicon_location_character         - SYSICON_BASE] = NULL,
    [sysicon_location_enemy             - SYSICON_BASE] = NULL,
    [sysicon_location_hand              - SYSICON_BASE] = NULL,
    [sysicon_lock                       - SYSICON_BASE] = "lock",
    [sysicon_lock_vertical              - SYSICON_BASE] = NULL,
    [sysicon_mac_logo                   - SYSICON_BASE] = "apple-mac",
    [sysicon_magnifier                  - SYSICON_BASE] = "search",
    [sysicon_magnifier_zoom_in          - SYSICON_BASE] = "zoom-in",
    [sysicon_magnifier_zoom_out         - SYSICON_BASE] = "zoom-out",
    [sysicon_main_menu                  - SYSICON_BASE] = "menu",
    [sysicon_map                        - SYSICON_BASE] = "map",
    [sysicon_map_add                    - SYSICON_BASE] = NULL,
    [sysicon_map_fog                    - SYSICON_BASE] = NULL,
    [sysicon_map_link                   - SYSICON_BASE] = NULL,
    [sysicon_map_reset                  - SYSICON_BASE] = NULL,
    [sysicon_map_start                  - SYSICON_BASE] = NULL,
    [sysicon_margin_bottom              - SYSICON_BASE] = NULL,
    [sysicon_margin_left                - SYSICON_BASE] = NULL,
    [sysicon_margin_right               - SYSICON_BASE] = NULL,
    [sysicon_margin_top                 - SYSICON_BASE] = NULL,
    [sysicon_margin_top_left            - SYSICON_BASE] = NULL,
    [sysicon_mesh_default               - SYSICON_BASE] = NULL,
    [sysicon_mesh_marching              - SYSICON_BASE] = NULL,
    [sysicon_mesh_preview               - SYSICON_BASE] = NULL,
    [sysicon_message                    - SYSICON_BASE] = "chat-lines",
    [sysicon_missing                    - SYSICON_BASE] = "question-mark",
    [sysicon_model_edit                 - SYSICON_BASE] = NULL,
    [sysicon_mouse                      - SYSICON_BASE] = "mouse-button-left",
    [sysicon_mouse_eye                  - SYSICON_BASE] = NULL,
    [sysicon_mouse_free                 - SYSICON_BASE] = NULL,
    [sysicon_move_interact              - SYSICON_BASE] = NULL,
    [sysicon_music                      - SYSICON_BASE] = "music-double-note",
    [sysicon_music_play                 - SYSICON_BASE] = NULL,
    [sysicon_navigation                 - SYSICON_BASE] = "compass",
    [sysicon_navigation_adjacent        - SYSICON_BASE] = NULL,
    [sysicon_node                       - SYSICON_BASE] = "circle",
    [sysicon_node_add                   - SYSICON_BASE] = "plus-circle",
    [sysicon_node_copy                  - SYSICON_BASE] = NULL,
    [sysicon_node_library               - SYSICON_BASE] = NULL,
    [sysicon_node_link                  - SYSICON_BASE] = NULL,
    [sysicon_noise                      - SYSICON_BASE] = NULL,
    [sysicon_number                     - SYSICON_BASE] = NULL,
    [sysicon_object                     - SYSICON_BASE] = "cube",
    [sysicon_object_add                 - SYSICON_BASE] = NULL,
    [sysicon_object_hand                - SYSICON_BASE] = NULL,
    [sysicon_offset_horizontal          - SYSICON_BASE] = NULL,
    [sysicon_offset_vertical            - SYSICON_BASE] = NULL,
    [sysicon_package                    - SYSICON_BASE] = "package",
    [sysicon_page_add                   - SYSICON_BASE] = "page-plus",
    [sysicon_page_copy                  - SYSICON_BASE] = "copy",
    [sysicon_page_data                  - SYSICON_BASE] = "database-check",
    [sysicon_page_edit                  - SYSICON_BASE] = "page-edit",
    [sysicon_page_voxel                 - SYSICON_BASE] = NULL,
    [sysicon_paint_can                  - SYSICON_BASE] = "fill-color",
    [sysicon_palette                    - SYSICON_BASE] = "palette",
    [sysicon_party                      - SYSICON_BASE] = NULL,
    [sysicon_party_add                  - SYSICON_BASE] = NULL,
    [sysicon_party_delete               - SYSICON_BASE] = NULL,
    [sysicon_party_reset                - SYSICON_BASE] = NULL,
    [sysicon_pause                      - SYSICON_BASE] = "pause",
    [sysicon_pencil                     - SYSICON_BASE] = "edit-pencil",
    [sysicon_platform                   - SYSICON_BASE] = NULL,
    [sysicon_play                       - SYSICON_BASE] = "play",
    [sysicon_portrait                   - SYSICON_BASE] = "user",
    [sysicon_portrait_left              - SYSICON_BASE] = NULL,
    [sysicon_portrait_right             - SYSICON_BASE] = NULL,
    [sysicon_potion                     - SYSICON_BASE] = NULL,
    [sysicon_progress_bar               - SYSICON_BASE] = NULL,
    [sysicon_properties                 - SYSICON_BASE] = "notes",
    [sysicon_properties_edit            - SYSICON_BASE] = NULL,
    [sysicon_properties_insert          - SYSICON_BASE] = NULL,
    [sysicon_puzzle                     - SYSICON_BASE] = "puzzle",
    [sysicon_redo                       - SYSICON_BASE] = "redo",
    [sysicon_rpgiab_logo                - SYSICON_BASE] = NULL,
    [sysicon_scissors                   - SYSICON_BASE] = "scissor",
    [sysicon_script                     - SYSICON_BASE] = "code",
    [sysicon_script_add                 - SYSICON_BASE] = NULL,
    [sysicon_script_code                - SYSICON_BASE] = "code-brackets",
    [sysicon_script_delete              - SYSICON_BASE] = NULL,
    [sysicon_script_edit                - SYSICON_BASE] = NULL,
    [sysicon_script_lightning           - SYSICON_BASE] = NULL,
    [sysicon_script_lightning_add       - SYSICON_BASE] = NULL,
    [sysicon_script_lightning_edit      - SYSICON_BASE] = NULL,
    [sysicon_script_start               - SYSICON_BASE] = NULL,
    [sysicon_script_text                - SYSICON_BASE] = "notes",
    [sysicon_script_text_add            - SYSICON_BASE] = NULL,
    [sysicon_script_text_checkmark      - SYSICON_BASE] = NULL,
    [sysicon_script_text_delete         - SYSICON_BASE] = NULL,
    [sysicon_select_edit                - SYSICON_BASE] = NULL,
    [sysicon_settings_gear              - SYSICON_BASE] = "settings",
    [sysicon_settings_save              - SYSICON_BASE] = NULL,
    [sysicon_shadow                     - SYSICON_BASE] = NULL,
    [sysicon_shadow_darkness            - SYSICON_BASE] = NULL,
    [sysicon_shield                     - SYSICON_BASE] = "shield-check",
    [sysicon_ship                       - SYSICON_BASE] = NULL,
    [sysicon_skull                      - SYSICON_BASE] = NULL,
    [sysicon_skull_delete               - SYSICON_BASE] = NULL,
    [sysicon_sound                      - SYSICON_BASE] = "sound-high",
    [sysicon_star                       - SYSICON_BASE] = "star",
    [sysicon_steam_logo                 - SYSICON_BASE] = NULL,
    [sysicon_stop                       - SYSICON_BASE] = "square",
    [sysicon_stop_sign                  - SYSICON_BASE] = "warning-hexagon",
    [sysicon_string                     - SYSICON_BASE] = "input-field",
    [sysicon_surface_edges              - SYSICON_BASE] = NULL,
    [sysicon_sword                      - SYSICON_BASE] = NULL,
    [sysicon_sword_blood                - SYSICON_BASE] = NULL,
    [sysicon_sword_eye                  - SYSICON_BASE] = NULL,
    [sysicon_sword_hit                  - SYSICON_BASE] = NULL,
    [sysicon_sword_link                 - SYSICON_BASE] = NULL,
    [sysicon_sword_reset                - SYSICON_BASE] = NULL,
    [sysicon_sword_start                - SYSICON_BASE] = NULL,
    [sysicon_sword_stop                 - SYSICON_BASE] = NULL,
    [sysicon_swords                     - SYSICON_BASE] = NULL,
    [sysicon_tab                        - SYSICON_BASE] = "page",
    [sysicon_tag_blue                   - SYSICON_BASE] = NULL,
    [sysicon_tag_id                     - SYSICON_BASE] = NULL,
    [sysicon_terrain                    - SYSICON_BASE] = NULL,
    [sysicon_terrain_follow             - SYSICON_BASE] = NULL,
    [sysicon_terrain_precise            - SYSICON_BASE] = NULL,
    [sysicon_terrain_smooth_curve       - SYSICON_BASE] = NULL,
    [sysicon_terrain_smooth_linear      - SYSICON_BASE] = NULL,
    [sysicon_text                       - SYSICON_BASE] = "text",
    [sysicon_text_align_center          - SYSICON_BASE] = "align-center",
    [sysicon_text_bullets               - SYSICON_BASE] = "list",
    [sysicon_text_eye                   - SYSICON_BASE] = NULL,
    [sysicon_text_field                 - SYSICON_BASE] = "input-field",
    [sysicon_text_line_numbers          - SYSICON_BASE] = "numbered-list-left",
    [sysicon_text_line_spacing          - SYSICON_BASE] = NULL,
    [sysicon_tile                       - SYSICON_BASE] = NULL,
    [sysicon_tile_add                   - SYSICON_BASE] = NULL,
    [sysicon_tile_enter                 - SYSICON_BASE] = NULL,
    [sysicon_tile_exclamation           - SYSICON_BASE] = NULL,
    [sysicon_tile_exit                  - SYSICON_BASE] = NULL,
    [sysicon_tile_impassable            - SYSICON_BASE] = NULL,
    [sysicon_tile_passable              - SYSICON_BASE] = NULL,
    [sysicon_tile_stop                  - SYSICON_BASE] = NULL,
    [sysicon_tool_items                 - SYSICON_BASE] = NULL,
    [sysicon_tools                      - SYSICON_BASE] = "wrench",
    [sysicon_transparency               - SYSICON_BASE] = "square-dashed",
    [sysicon_tree_collapse              - SYSICON_BASE] = "nav-arrow-right",
    [sysicon_tree_expand                - SYSICON_BASE] = "nav-arrow-down",
    [sysicon_trello_logo                - SYSICON_BASE] = NULL,
    [sysicon_twitter_logo               - SYSICON_BASE] = "twitter",
    [sysicon_ui                         - SYSICON_BASE] = "square",
    [sysicon_ui_inventory               - SYSICON_BASE] = NULL,
    [sysicon_ui_toolbar                 - SYSICON_BASE] = NULL,
    [sysicon_undo                       - SYSICON_BASE] = "undo",
    [sysicon_unlock                     - SYSICON_BASE] = "lock-slash",
    [sysicon_variable                   - SYSICON_BASE] = NULL,
    [sysicon_variable_return            - SYSICON_BASE] = NULL,
    [sysicon_voxel                      - SYSICON_BASE] = NULL,
    [sysicon_voxel_add                  - SYSICON_BASE] = NULL,
    [sysicon_voxel_editor               - SYSICON_BASE] = NULL,
    [sysicon_voxel_outlines             - SYSICON_BASE] = NULL,
    [sysicon_voxel_paint                - SYSICON_BASE] = NULL,
    [sysicon_warning                    - SYSICON_BASE] = "warning-triangle",
    [sysicon_weapons                    - SYSICON_BASE] = NULL,
    [sysicon_weather_fog                - SYSICON_BASE] = NULL,
    [sysicon_weather_sun                - SYSICON_BASE] = "sun-light",
    [sysicon_widget                     - SYSICON_BASE] = "frame",
    [sysicon_widget_height              - SYSICON_BASE] = NULL,
    [sysicon_widget_hide                - SYSICON_BASE] = NULL,
    [sysicon_widget_lock                - SYSICON_BASE] = NULL,
    [sysicon_widget_save                - SYSICON_BASE] = NULL,
    [sysicon_widget_show                - SYSICON_BASE] = NULL,
    [sysicon_widget_width               - SYSICON_BASE] = NULL,
    [sysicon_windows_logo               - SYSICON_BASE] = "windows",
    [sysicon_window_compact_off         - SYSICON_BASE] = NULL,
    [sysicon_window_compact_on          - SYSICON_BASE] = NULL,
    [sysicon_window_dialogue            - SYSICON_BASE] = NULL,
    [sysicon_window_expand_down         - SYSICON_BASE] = NULL,
    [sysicon_window_expand_up           - SYSICON_BASE] = NULL,
    [sysicon_window_frame_show          - SYSICON_BASE] = NULL,
    [sysicon_window_free                - SYSICON_BASE] = NULL,
    [sysicon_window_title_show          - SYSICON_BASE] = NULL,
    [sysicon_window_width               - SYSICON_BASE] = NULL,
    [sysicon_wizard                     - SYSICON_BASE] = NULL,
    [sysicon_world                      - SYSICON_BASE] = "globe",
    [sysicon_world_disable              - SYSICON_BASE] = NULL,
    [sysicon_world_disk                 - SYSICON_BASE] = NULL,
    [sysicon_world_enable               - SYSICON_BASE] = NULL,
    [sysicon_world_hand                 - SYSICON_BASE] = NULL,
    [sysicon_world_page                 - SYSICON_BASE] = NULL,
    [sysicon_xp                         - SYSICON_BASE] = NULL,
    [sysicon_yield                      - SYSICON_BASE] = NULL,
    [sysicon_yield_add                  - SYSICON_BASE] = NULL,
};

#define SYSICON_COUNT ((int)(sysicon_yield_add - SYSICON_BASE + 1))

bool svg_load_sysicon_strip(const char *icons_dir, bitmap_strip_t *out, FILE *missing) {
    return svg_build_strip(icons_dir, k_sysicon_names, SYSICON_COUNT,
                           SYSICON_SIZE, 32, out, missing);
}

// ---------------------------------------------------------------------------
// File-picker mapping  (sysicons.h  icon_id_t enum, 16x16)
// ---------------------------------------------------------------------------

static const char *k_picker_names[ICON_COUNT] = {
    [ICON_ADD_ELEMENT          ] = "plus",
    [ICON_ARROW                ] = "arrow-right",
    [ICON_ARROW_DOWN           ] = "arrow-down",
    [ICON_ARROW_DOWN_END       ] = "nav-arrow-down",
    [ICON_ARROW_UP             ] = "arrow-up",
    [ICON_ARROW_UP_END         ] = "nav-arrow-up",
    [ICON_BACK_ARROW           ] = "arrow-left",
    [ICON_BACK_ARROW_END       ] = "fast-arrow-left",
    [ICON_BEGIN_END_SELECT     ] = NULL,
    [ICON_BOX_SELECT           ] = "select-window",
    [ICON_BRUSH                ] = "design-nib",
    [ICON_CASE_INSENSITIVE     ] = "text-size",
    [ICON_CHAIN_LINK           ] = "link",
    [ICON_CHESSBOARD           ] = "view-grid",
    [ICON_CLIPBOARD            ] = "paste-clipboard",
    [ICON_COLOUR_PICKER        ] = "color-picker",
    [ICON_COPY                 ] = "copy",
    [ICON_CROP                 ] = "crop",
    [ICON_CUT                  ] = "scissor",
    [ICON_CUT_2                ] = "scissor",
    [ICON_DELETE               ] = "trash",
    [ICON_DELETE_ALL_ELEMENTS  ] = NULL,
    [ICON_DELETE_ELEMENT       ] = NULL,
    [ICON_DRAW_OVAL            ] = "circle",
    [ICON_DRAW_RECT            ] = "square",
    [ICON_DRAW_TRIANGLE        ] = "triangle",
    [ICON_EDIT_ELEMENT         ] = "edit-pencil",
    [ICON_EDIT_ELEMENTS_CODE   ] = "code-brackets",
    [ICON_ERASER               ] = "erase",
    [ICON_ERROR                ] = "warning-triangle",
    [ICON_EXIT                 ] = "log-out",
    [ICON_FASTFORWARD          ] = "forward",
    [ICON_FIND                 ] = "search",
    [ICON_FIND_NEXT            ] = "search",
    [ICON_FIND_PREVIOUS        ] = "search",
    [ICON_FOLDER_CLOSE         ] = "folder",
    [ICON_FOLDER_NEW           ] = "folder-plus",
    [ICON_FOLDER_OPEN          ] = "folder",
    [ICON_FOLDER_OPEN_2        ] = "folder",
    [ICON_FORWARD_ARROW        ] = "arrow-right",
    [ICON_FORWARD_ARROW_END    ] = "fast-arrow-right",
    [ICON_FRAGMENT_SHADER      ] = "code",
    [ICON_GO_TO                ] = "arrow-up-right",
    [ICON_HELP_BUTTON          ] = "question-mark",
    [ICON_HIDE                 ] = "eye-closed",
    [ICON_IGNORE               ] = "xmark",
    [ICON_IGNORE_WHITESPACE    ] = NULL,
    [ICON_LINE                 ] = "minus",
    [ICON_LOCK_LOCKED          ] = "lock",
    [ICON_LOCK_UNLOCKED        ] = "lock-slash",
    [ICON_MOVE                 ] = "ruler-arrows",
    [ICON_NBT                  ] = "code-brackets",
    [ICON_PAINT_BUCKET         ] = "fill-color",
    [ICON_PAINTING_BLANK       ] = "media-image",
    [ICON_PAINTING_DECORATED   ] = "frame",
    [ICON_PAPER_BINARY         ] = "code",
    [ICON_PAPER_BLANK          ] = "page",
    [ICON_PAPER_CODE           ] = "code-brackets",
    [ICON_PAPER_EXPORT         ] = "send-diagonal",
    [ICON_PAPER_JSON           ] = "code",
    [ICON_PAPER_NEW            ] = "page-plus",
    [ICON_PAPER_SMALL          ] = "page",
    [ICON_PAPER_TEXT           ] = "notes",
    [ICON_PASTE                ] = "paste-clipboard",
    [ICON_PAUSE                ] = "pause",
    [ICON_PENCIL               ] = "edit-pencil",
    [ICON_REDO                 ] = "redo",
    [ICON_RELOAD_ARROW         ] = "refresh",
    [ICON_REPLACE              ] = "refresh",
    [ICON_REWIND               ] = "rewind",
    [ICON_RUN                  ] = "play",
    [ICON_RUN_IN_DEBUG         ] = "play",
    [ICON_SAVE                 ] = "floppy-disk",
    [ICON_SAVE_AS              ] = "floppy-disk",
    [ICON_SELECT_ALL           ] = "select-window",
    [ICON_SELECT_LINE          ] = NULL,
    [ICON_SELECT_WORD          ] = NULL,
    [ICON_SETTINGS             ] = "settings",
    [ICON_SHAPES               ] = "hexagon",
    [ICON_SHOW                 ] = "eye",
    [ICON_STRIP_SPECIAL_CHARACTERS] = NULL,
    [ICON_SWAP                 ] = "refresh",
    [ICON_TRANSLATE            ] = "language",
    [ICON_UNDO                 ] = "undo",
    [ICON_VERTEX_SHADER        ] = "code",
    [ICON_WRENCH               ] = "wrench",
    [ICON_ZOOM_IN              ] = "zoom-in",
    [ICON_ZOOM_OUT             ] = "zoom-out",
};

bool svg_load_picker_strip(const char *icons_dir, bitmap_strip_t *out, FILE *missing) {
    return svg_build_strip(icons_dir, k_picker_names, ICON_COUNT,
                           12, 12, out, missing);
}

// ---------------------------------------------------------------------------
// On-demand icon resolution (sysicon_resolve / svg_set_icons_dir)
// ---------------------------------------------------------------------------

extern bitmap_strip_t *ui_get_sysicon_strip(void);

#define MAX_ICON_DIRS 8
static char g_icon_dirs[MAX_ICON_DIRS][4096];
static int  g_icon_dir_count;

typedef struct {
    char     name[64];
    uint32_t tex;
    int      w, h;
} sysicon_cache_t;
static sysicon_cache_t g_sysicon_cache[64];
static int             g_sysicon_cache_n;

void svg_set_icons_dir(const char *dir) {
    g_icon_dir_count = 0;
    if (dir && dir[0]) {
        strncpy(g_icon_dirs[0], dir, sizeof(g_icon_dirs[0]) - 1);
        g_icon_dir_count = 1;
    }
}

void svg_add_icons_dir(const char *dir) {
    if (!dir || !dir[0] || g_icon_dir_count >= MAX_ICON_DIRS) return;
    strncpy(g_icon_dirs[g_icon_dir_count], dir, sizeof(g_icon_dirs[0]) - 1);
    g_icon_dir_count++;
}

bool sysicon_resolve(const char *name, sysicon_resolved_t *out) {
    if (!name || !name[0]) return false;

    bitmap_strip_t *strip = ui_get_sysicon_strip();
    if (strip && strip->tex) {
        for (int i = 0; i < SYSICON_COUNT; i++) {
            if (k_sysicon_names[i] && strcmp(k_sysicon_names[i], name) == 0) {
                int col = i % strip->cols;
                int row = i / strip->cols;
                out->tex = strip->tex;
                out->u0  = (float)(col * strip->icon_w) / (float)strip->sheet_w;
                out->v0  = (float)(row * strip->icon_h) / (float)strip->sheet_h;
                out->u1  = out->u0 + (float)strip->icon_w  / (float)strip->sheet_w;
                out->v1  = out->v0 + (float)strip->icon_h  / (float)strip->sheet_h;
                out->w   = strip->icon_w;
                out->h   = strip->icon_h;
                return true;
            }
        }
    }

    for (int i = 0; i < g_sysicon_cache_n; i++) {
        if (strcmp(g_sysicon_cache[i].name, name) == 0) {
            out->tex = g_sysicon_cache[i].tex;
            out->u0 = 0.0f; out->v0 = 0.0f; out->u1 = 1.0f; out->v1 = 1.0f;
            out->w  = g_sysicon_cache[i].w;
            out->h  = g_sysicon_cache[i].h;
            return true;
        }
    }

    if (!g_icon_dir_count || g_sysicon_cache_n >= 64) return false;
    uint8_t *pixels = (uint8_t *)malloc((size_t)SYSICON_SIZE * SYSICON_SIZE * 4);
    if (!pixels) return false;
    bool drawn = false;
    char path[5120];
    for (int di = 0; di < g_icon_dir_count && !drawn; di++) {
        snprintf(path, sizeof(path), "%s/%s.svg", g_icon_dirs[di], name);
        drawn = rasterize_svg(path, SYSICON_SIZE, pixels);
    }
    if (!drawn) { free(pixels); return false; }
    uint32_t tex = R_CreateTextureRGBA(SYSICON_SIZE, SYSICON_SIZE, pixels,
                                       R_FILTER_NEAREST, R_WRAP_CLAMP);
    free(pixels);
    if (!tex) return false;
    sysicon_cache_t *e = &g_sysicon_cache[g_sysicon_cache_n++];
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    e->tex = tex; e->w = SYSICON_SIZE; e->h = SYSICON_SIZE;
    out->tex = tex; out->u0 = 0.0f; out->v0 = 0.0f; out->u1 = 1.0f; out->v1 = 1.0f;
    out->w = SYSICON_SIZE; out->h = SYSICON_SIZE;
    return true;
}
