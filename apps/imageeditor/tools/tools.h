// tools/tools.h — Tool handler interface for ImageEditor
// Replaces giant switch statements in win_canvas.c with table-driven dispatch

#ifndef __IMAGEEDITOR_TOOLS_H__
#define __IMAGEEDITOR_TOOLS_H__

#include "../imageeditor.h"

// Tool handler interface - implements behavior for one tool type.
// Each tool responds to mouse/keyboard events with begin/drag/end/cancel/key.
//
// Example: Pencil tool
//   begin() - Start stroke, push undo
//   drag()  - Draw line from last position to current
//   end()   - Finish stroke
//   cancel() - Discard stroke (ESC key)
//   key()   - Handle tool-specific shortcuts
typedef struct tool_handler_s {
  int         id;          // Tool ID (ID_TOOL_PENCIL, ID_TOOL_BRUSH, etc.)
  const char *name;        // Display name for debugging
  
  // Begin tool interaction - called on mouse down
  // doc_pt is in document (pixel) coordinates, already converted from viewport
  void (*begin)(canvas_doc_t *doc, canvas_win_state_t *view, ipoint16_t doc_pt);
  
  // Continue tool interaction - called on mouse move while button held
  void (*drag)(canvas_doc_t *doc, canvas_win_state_t *view, ipoint16_t doc_pt);
  
  // End tool interaction - called on mouse up
  // Commits changes (e.g., swaps undo snapshot for shape tools)
  void (*end)(canvas_doc_t *doc, canvas_win_state_t *view, ipoint16_t doc_pt);
  
  // Cancel tool interaction - called on ESC key or tool switch
  // Discards changes without committing (strict cancel semantics per decision 2-A)
  void (*cancel)(canvas_doc_t *doc, canvas_win_state_t *view);
  
  // Handle tool-specific keyboard input
  // Return true if handled, false to allow default behavior
  bool (*key)(canvas_doc_t *doc, canvas_win_state_t *view, uint32_t key, uint32_t mods);
  
} tool_handler_t;

// ── Tool registry ──────────────────────────────────────────────────────────

// Get the handler for a tool ID, or NULL if no handler registered.
// This is the main dispatch function - replaces switch(g_app->current_tool).
const tool_handler_t *get_tool_handler(int tool_id);

// Register all built-in tool handlers (called at startup)
void register_builtin_tools(void);

#endif // __IMAGEEDITOR_TOOLS_H__
