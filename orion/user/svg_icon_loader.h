#ifndef __SVG_ICON_LOADER_H__
#define __SVG_ICON_LOADER_H__

#include <stdio.h>
#include <stdbool.h>
#include "draw.h"

// Build a bitmap_strip_t by rasterizing SVG files from a directory.
//
// svg_names : array of `count` iconoir base names (no .svg extension);
//             NULL entries produce blank tiles.
// icon_size : output tile size in pixels (square).
// cols      : sheet columns; rows are computed automatically.
// missing   : optional FILE* to receive one diagnostic line per blank tile.
//
// On success, fills *out and returns true.  The texture is uploaded to GPU and
// must be released with R_DeleteTexture(out->tex) when done.
bool svg_build_strip(const char *icons_dir,
                     const char **svg_names, int count,
                     int icon_size, int cols,
                     bitmap_strip_t *out,
                     FILE *missing);

// Build the system-icon strip (sysicon_* enum) from <icons_dir>.
bool svg_load_sysicon_strip(const char *icons_dir, bitmap_strip_t *out, FILE *missing);

// Build the file-picker icon strip (icon_id_t enum) from <icons_dir>.
bool svg_load_picker_strip(const char *icons_dir, bitmap_strip_t *out, FILE *missing);


#endif
