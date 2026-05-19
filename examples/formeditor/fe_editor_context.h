#ifndef __FE_EDITOR_CONTEXT_H__
#define __FE_EDITOR_CONTEXT_H__

#include "../../ui.h"
#include "fe_document.h"

// ============================================================
// Editor Context API
// ============================================================
// Manages the separation between document model and live design-time views.
// The editor context owns the mapping between model elements (by ID) and
// live window instances used for WYSIWYG preview on the canvas.

// Live view reference - maps element ID to design-time window instance.
typedef struct {
  uint32_t element_id;   // document element ID
  window_t *live_win;    // design-time window instance
} fe_live_view_ref_t;

// Maximum number of live views (same as MAX_ELEMENTS).
#define FE_MAX_LIVE_VIEWS MAX_ELEMENTS

// Editor context storage (embedded in canvas_state_t for now).
typedef struct {
  fe_live_view_ref_t views[FE_MAX_LIVE_VIEWS];
  int view_count;
} fe_editor_context_t;

// ============================================================
// Live View Management
// ============================================================

// Initialize editor context (clears all mappings).
void fe_ctx_init(fe_editor_context_t *ctx);

// Register a live view for an element.
// Creates mapping from element_id → live_win.
void fe_ctx_register_live_view(fe_editor_context_t *ctx, uint32_t element_id, window_t *live_win);

// Unregister a live view for an element.
// Removes mapping for element_id.
void fe_ctx_unregister_live_view(fe_editor_context_t *ctx, uint32_t element_id);

// Find live window for an element ID.
// Returns NULL if not found.
window_t *fe_ctx_find_live_view(fe_editor_context_t *ctx, uint32_t element_id);

// Find element for a live window (reverse lookup).
// Returns NULL if not found.
form_element_t *fe_ctx_find_element_for_live_window(fe_editor_context_t *ctx, form_doc_t *doc, window_t *win);

// Clear all live view mappings (typically before rebuild).
void fe_ctx_clear_all_live_views(fe_editor_context_t *ctx);

#endif // __FE_EDITOR_CONTEXT_H__
