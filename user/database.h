#ifndef __UI_DATABASE_H__
#define __UI_DATABASE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct database_s database_t;
typedef intptr_t lresult_t;  // Pointer-sized result (like WinAPI LRESULT)

// Database procedure callback type (analogous to winproc_t)
typedef lresult_t (*dbproc_t)(database_t *, uint32_t, uint32_t, void *);

// Database messages
enum {
  dbCreate,          // wparam=0; lparam=const char* source_path → returns 1 on success
  dbDestroy,         // cleanup → returns 1 on success
  dbLoad,            // load from source → returns 1 on success
  dbSave,            // save to source (only if dirty) → returns 1 on success
  dbInsert,          // wparam=table_id; lparam=record_data → returns (lresult_t)record_ptr
  dbUpdate,          // wparam=table_id; lparam=record_data → returns 1 on success
  dbDelete,          // wparam=table_id; lparam=(void*)(intptr_t)record_id → returns 1 on success
  dbFetch,           // wparam=MAKEDWORD(table_id,filter_field); lparam=fetch_params_t* → returns (lresult_t)results_array
  dbFind,            // wparam=MAKEDWORD(table_id,search_field); lparam=(intptr_t)value or (void*)str → returns (lresult_t)record_ptr
  dbGetDirty,        // returns dirty flag as lresult_t (0 or 1)
  dbUser = 1000      // custom database implementations can use dbUser+
};

// Fetch parameters (for dbFetch message)
// wparam carries MAKEDWORD(table_id, filter_field)
typedef struct {
  intptr_t filter_value;  // value to match (cast from int or pointer)
  int *count_out;         // output: number of records
  // Result array returned directly as lresult_t (cast to record_type**; caller frees)
} fetch_params_t;

// Database class descriptor
typedef struct {
  const char *class_name;  // "SimpleXMLDatabase", etc.
  dbproc_t proc;           // database procedure
} db_class_desc_t;

// Database structure (analogous to window_t)
struct database_s {
  const char *name;        // database instance name
  const char *class_name;  // registered class name
  dbproc_t proc;           // database procedure
  void *userdata;          // implementation-specific data
  char source_path[512];   // path to XML/SQLite/etc. file
  bool dirty;              // needs save
};

// Database management functions
database_t *create_database(const char *name, const char *class_name, const char *source_path);
void destroy_database(database_t *db);
lresult_t send_db_message(database_t *db, uint32_t msg, uint32_t wparam, void *lparam);

// Database class registry
bool register_database_class(const db_class_desc_t *desc);
dbproc_t find_database_class_proc(const char *class_name);

#define DB_CLASS(proc_sym) \
  register_database_class(&(db_class_desc_t){ .class_name = #proc_sym, .proc = (proc_sym) })

#endif // __UI_DATABASE_H__
