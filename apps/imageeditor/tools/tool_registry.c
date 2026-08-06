// tools/tool_registry.c — Tool handler registration and lookup

#include "tools.h"

// ── Forward declarations for tool handlers ────────────────────────────────

// Implemented in tool_*.c files (Phase 3.2)
extern const tool_handler_t tool_pencil_handler;
extern const tool_handler_t tool_brush_handler;
extern const tool_handler_t tool_eraser_handler;
extern const tool_handler_t tool_select_handler;
extern const tool_handler_t tool_crop_handler;
extern const tool_handler_t tool_move_handler;
extern const tool_handler_t tool_line_handler;
extern const tool_handler_t tool_rect_handler;
extern const tool_handler_t tool_ellipse_handler;
extern const tool_handler_t tool_rounded_rect_handler;
extern const tool_handler_t tool_polygon_handler;
extern const tool_handler_t tool_fill_handler;
extern const tool_handler_t tool_spray_handler;
extern const tool_handler_t tool_hand_handler;
extern const tool_handler_t tool_zoom_handler;
extern const tool_handler_t tool_eyedropper_handler;
extern const tool_handler_t tool_magnifier_handler;
extern const tool_handler_t tool_text_handler;
extern const tool_handler_t tool_magic_wand_handler;

// ── Tool registry table ────────────────────────────────────────────────────

// Static table of all registered tools. Indexed by ID_TOOL_* constants.
// NULL entries mean no handler is registered for that tool.
static const tool_handler_t *g_tool_registry[NUM_TOOLS] = {NULL};

void register_builtin_tools(void) {
  // Drawing tools
  g_tool_registry[ID_TOOL_PENCIL - ID_TOOL_PENCIL]       = &tool_pencil_handler;
  g_tool_registry[ID_TOOL_BRUSH - ID_TOOL_PENCIL]        = &tool_brush_handler;
  g_tool_registry[ID_TOOL_ERASER - ID_TOOL_PENCIL]       = &tool_eraser_handler;
  
  // Selection tools
  g_tool_registry[ID_TOOL_SELECT - ID_TOOL_PENCIL]       = &tool_select_handler;
  g_tool_registry[ID_TOOL_CROP - ID_TOOL_PENCIL]         = &tool_crop_handler;
  g_tool_registry[ID_TOOL_MOVE - ID_TOOL_PENCIL]         = &tool_move_handler;
  g_tool_registry[ID_TOOL_MAGIC_WAND - ID_TOOL_PENCIL]   = &tool_magic_wand_handler;
  
  // Shape tools
  g_tool_registry[ID_TOOL_LINE - ID_TOOL_PENCIL]         = &tool_line_handler;
  g_tool_registry[ID_TOOL_RECT - ID_TOOL_PENCIL]         = &tool_rect_handler;
  g_tool_registry[ID_TOOL_ELLIPSE - ID_TOOL_PENCIL]      = &tool_ellipse_handler;
  g_tool_registry[ID_TOOL_ROUNDED_RECT - ID_TOOL_PENCIL] = &tool_rounded_rect_handler;
  g_tool_registry[ID_TOOL_POLYGON - ID_TOOL_PENCIL]      = &tool_polygon_handler;
  
  // Fill tools
  g_tool_registry[ID_TOOL_FILL - ID_TOOL_PENCIL]         = &tool_fill_handler;
  g_tool_registry[ID_TOOL_SPRAY - ID_TOOL_PENCIL]        = &tool_spray_handler;
  
  // View tools
  g_tool_registry[ID_TOOL_HAND - ID_TOOL_PENCIL]         = &tool_hand_handler;
  g_tool_registry[ID_TOOL_ZOOM - ID_TOOL_PENCIL]         = &tool_zoom_handler;
  g_tool_registry[ID_TOOL_EYEDROPPER - ID_TOOL_PENCIL]   = &tool_eyedropper_handler;
  g_tool_registry[ID_TOOL_MAGNIFIER - ID_TOOL_PENCIL]    = &tool_magnifier_handler;
  
  // Special tools
  g_tool_registry[ID_TOOL_TEXT - ID_TOOL_PENCIL]         = &tool_text_handler;
}

const tool_handler_t *get_tool_handler(int tool_id) {
  // Validate tool_id range
  if (tool_id < ID_TOOL_PENCIL || tool_id >= ID_TOOL_PENCIL + NUM_TOOLS) {
    return NULL;
  }
  
  int index = tool_id - ID_TOOL_PENCIL;
  return g_tool_registry[index];
}
