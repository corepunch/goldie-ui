#ifndef __FE_DOCUMENT_H__
#define __FE_DOCUMENT_H__

#include "../../ui.h"

// Forward declare to avoid circular dependencies
#define MAX_ELEMENTS  256
#define CTRL_ID_BASE  1001
#define FE_MAX_TABLE_COLUMNS 16
// Note: FE_MAX_COMPONENTS is defined in user/user.h as 128

// ============================================================
// Document Model Types
// ============================================================

typedef struct {
  int      type;        // registered component ID
  int      id;          // numeric control ID (e.g. 1001)
  uint32_t parent;      // parent control ID; 0 = form root
  char     id_expr[32]; // original ID expression from project XML, if any
  irect16_t frame;      // position and size in form coordinates
  uint32_t flags;        // reserved for future style flags
  char     flags_expr[128]; // original flags expression from project XML, if any
  char     text[64];     // control caption / label text
  char     name[32];     // identifier name (e.g. "IDC_BUTTON1")
  uint8_t  h_align;     // horizontal alignment; 0 = stretch
  uint8_t  v_align;     // vertical alignment; 0 = stretch
  irect16_t padding;    // inner padding for nested layout containers
  irect16_t margin;     // outer margin when auto-layout reflows this element
  uint8_t  layout_spacing; // spacing between direct children for layout containers
  uint8_t  font;        // label font; FONT_SMALL by default
  bool     font_set;    // font attribute explicitly set in the project
  uint8_t  color;       // label color palette index; 0 = transparent
  bool     color_set;   // color attribute explicitly set in the project
  
  // Database binding support (analogous to NeXTSTEP DBKit)
  char     db_field[64];      // Field path: "posts.title" or "author.name"
  char     db_source[64];     // Combobox source table: "authors"
  char     db_display[64];    // Combobox display field: "name"
  char     db_value[64];      // Combobox value field: "id"

  int      db_column_count;
  char     db_column_fields[FE_MAX_TABLE_COLUMNS][64];
  char     db_column_titles[FE_MAX_TABLE_COLUMNS][64];
  int      db_column_widths[FE_MAX_TABLE_COLUMNS + 1];
  
  window_t *live_win;    // design-time live control hosted on the canvas (temporary)
} form_element_t;

typedef struct form_doc_t {
  form_element_t elements[MAX_ELEMENTS];
  int    element_count;
  isize16_t form_size;
  uint32_t flags;       // form/window flags exported in form_def_t
  uint8_t layout_mode;  // window_layout_mode_t
  uint8_t layout_columns; // grid columns (0 = default)
  uint8_t layout_spacing; // spacing between direct children; 0 = default
  irect16_t padding;    // inner padding for auto-layout content
  irect16_t margin;     // outer margin for the form when serialized
  bool   modified;
  char   form_id[64];
  char   form_title[128];
  char   required_plugin[64];
  
  // Database context for the form (NeXTSTEP DBKit style)
  char   database_name[64];  // e.g., "db"
  char   table_name[64];     // Primary table: "posts"
  int    next_id;                      // next numeric control ID
  int    type_counters[FE_MAX_COMPONENTS]; // per-component name counter
  window_t *canvas_win;
  window_t *doc_win;
  struct form_doc_t *next;
  // Grid settings
  int    grid_size;       // dot spacing in form pixels (default 8)
  bool   show_grid;       // paint grid dots on the form surface
  bool   snap_to_grid;    // snap moves/resizes to grid
} form_doc_t;

// ============================================================
// Document Model API
// ============================================================
// Pure document model layer - manages document lifecycle and element mutations.

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

// Set element properties by control ID.
// Returns true on success, false if element is not found.
bool fe_doc_set_element_text(form_doc_t *doc, int element_id, const char *text);
bool fe_doc_set_element_frame(form_doc_t *doc, int element_id, irect16_t frame);
bool fe_doc_set_element_name(form_doc_t *doc, int element_id, const char *name);
bool fe_doc_set_element_align(form_doc_t *doc, int element_id, uint8_t h_align, uint8_t v_align);
bool fe_doc_set_element_font(form_doc_t *doc, int element_id, uint8_t font);
bool fe_doc_set_element_color(form_doc_t *doc, int element_id, uint8_t color);

// Database binding setters (NeXTSTEP DBKit style), by control ID.
bool fe_doc_set_element_db_field(form_doc_t *doc, int element_id, const char *field);
bool fe_doc_set_element_db_source(form_doc_t *doc, int element_id, const char *source);
bool fe_doc_set_element_db_display(form_doc_t *doc, int element_id, const char *display);
bool fe_doc_set_element_db_value(form_doc_t *doc, int element_id, const char *value);

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
