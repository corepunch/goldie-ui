#ifndef __FE_DOCUMENT_H__
#define __FE_DOCUMENT_H__

#include "../../ui.h"

// ============================================================
// Document Model API
// ============================================================
// Pure document model layer - no UI references, no live_win pointers.
// Document lifecycle, element mutation, ID generation/resolution.

struct form_doc_t;
typedef struct form_doc_t form_doc_t;

struct form_element_t;
typedef struct form_element_t form_element_t;

// ============================================================
// Document Lifecycle
// ============================================================

// Create a new empty document with given form dimensions.
// Returns NULL on allocation failure or invalid dimensions.
form_doc_t *fe_doc_create(const char *form_id, int w, int h);

// Destroy document and free all resources.
// Does NOT destroy UI windows - caller must handle that.
void fe_doc_destroy(form_doc_t *doc);

// Mark document as modified, update title, and broadcast notifications.
void fe_doc_mark_modified(form_doc_t *doc);

// Update document window title with modified marker.
void fe_doc_update_title(form_doc_t *doc);

// ============================================================
// Element Mutation
// ============================================================

// Add a new element to the document.
// Returns element index, or -1 on failure.
int fe_doc_add_element(form_doc_t *doc, int type, irect16_t frame, uint32_t parent_id);

// Delete element at given index.
// Returns true on success, false if index invalid.
bool fe_doc_delete_element(form_doc_t *doc, int idx);

// Set element properties.
// Returns true on success, false if index invalid.
bool fe_doc_set_element_text(form_doc_t *doc, int idx, const char *text);
bool fe_doc_set_element_frame(form_doc_t *doc, int idx, irect16_t frame);
bool fe_doc_set_element_name(form_doc_t *doc, int idx, const char *name);
bool fe_doc_set_element_align(form_doc_t *doc, int idx, uint8_t h_align, uint8_t v_align);
bool fe_doc_set_element_font(form_doc_t *doc, int idx, uint8_t font);
bool fe_doc_set_element_color(form_doc_t *doc, int idx, uint8_t color);

// ============================================================
// Element Query
// ============================================================

// Find element by ID (returns NULL if not found).
form_element_t *fe_doc_find_element(form_doc_t *doc, uint32_t id);

// Find element index by ID (returns -1 if not found).
int fe_doc_find_element_index(form_doc_t *doc, uint32_t id);

// Get element at index (returns NULL if index invalid).
form_element_t *fe_doc_get_element(form_doc_t *doc, int idx);

// Get element count.
int fe_doc_element_count(const form_doc_t *doc);

// ============================================================
// ID Generation
// ============================================================

// Resolve control ID expression to numeric ID.
// If expr is a number, returns that; otherwise allocates new ID.
int fe_doc_resolve_control_id(form_doc_t *doc, const char *expr);

// Generate control ID expression string (e.g., "ID_MYFORM_BUTTON1").
void fe_doc_make_control_id_expr(char *out, size_t out_sz,
                                  const char *form_id,
                                  const char *name,
                                  const char *class_name,
                                  int ordinal);

#endif // __FE_DOCUMENT_H__
