// Editor context implementation
// Manages live view mapping between document elements and design-time windows.

#include "fe_editor_context.h"
#include <string.h>

// ============================================================
// Live View Management
// ============================================================

void fe_ctx_init(fe_editor_context_t *ctx) {
  if (!ctx)
    return;
  memset(ctx, 0, sizeof(*ctx));
}

void fe_ctx_register_live_view(fe_editor_context_t *ctx, uint32_t element_id, window_t *live_win) {
  if (!ctx || !live_win)
    return;

  // Check if already registered - update if so
  for (int i = 0; i < ctx->view_count; i++) {
    if (ctx->views[i].element_id == element_id) {
      ctx->views[i].live_win = live_win;
      return;
    }
  }

  // Add new mapping
  if (ctx->view_count < FE_MAX_LIVE_VIEWS) {
    ctx->views[ctx->view_count].element_id = element_id;
    ctx->views[ctx->view_count].live_win = live_win;
    ctx->view_count++;
  }
}

void fe_ctx_unregister_live_view(fe_editor_context_t *ctx, uint32_t element_id) {
  if (!ctx)
    return;

  // Find and remove mapping
  for (int i = 0; i < ctx->view_count; i++) {
    if (ctx->views[i].element_id == element_id) {
      // Shift remaining entries down
      for (int j = i; j < ctx->view_count - 1; j++) {
        ctx->views[j] = ctx->views[j + 1];
      }
      ctx->view_count--;
      return;
    }
  }
}

window_t *fe_ctx_find_live_view(fe_editor_context_t *ctx, uint32_t element_id) {
  if (!ctx)
    return NULL;

  for (int i = 0; i < ctx->view_count; i++) {
    if (ctx->views[i].element_id == element_id)
      return ctx->views[i].live_win;
  }
  return NULL;
}

form_element_t *fe_ctx_find_element_for_live_window(fe_editor_context_t *ctx, form_doc_t *doc, window_t *win) {
  if (!ctx || !doc || !win)
    return NULL;

  // Find element_id from window
  uint32_t element_id = 0;
  for (int i = 0; i < ctx->view_count; i++) {
    if (ctx->views[i].live_win == win) {
      element_id = ctx->views[i].element_id;
      break;
    }
  }

  if (element_id == 0)
    return NULL;

  // Find element in document
  return fe_doc_find_element(doc, element_id);
}

void fe_ctx_clear_all_live_views(fe_editor_context_t *ctx) {
  if (!ctx)
    return;
  ctx->view_count = 0;
}
