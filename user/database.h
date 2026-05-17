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
  dbInsert,          // wparam=table_id; lparam=insert_params_t* → returns (lresult_t)record_ptr
  dbUpdate,          // wparam=table_id; lparam=record_data_ptr → returns 1 on success
  dbDelete,          // wparam=table_id; lparam=(void*)(intptr_t)record_id → returns 1 on success
  dbFetch,           // wparam=table_id; lparam=fetch_params_t* → returns (lresult_t)results_array
  dbFind,            // wparam=table_id; lparam=find_params_t* → returns (lresult_t)record_ptr
  dbGetDirty,        // returns dirty flag as lresult_t (0 or 1)
  dbUser = 1000      // custom database implementations can use dbUser+
};

// Insert parameters (for dbInsert message)
typedef struct {
  void *record_data;      // input: record with fields filled in (id will be assigned)
  // Result returned directly as lresult_t (cast to record_type*)
} insert_params_t;

// Fetch parameters (for dbFetch message)
typedef struct {
  int filter_field;       // field to filter on (0=none, 1=author_id, 2=post_id, etc.)
  int filter_value;       // value to match
  int *count_out;         // output: number of records
  // Result array returned directly as lresult_t (cast to record_type**; caller frees)
} fetch_params_t;

// Find parameters (for dbFind message)
typedef struct {
  int search_field;       // field to search (0=id, 1=name, etc.)
  union {
    int int_value;        // for integer fields
    const char *str_value; // for string fields
  } search_value;
  // Result returned directly as lresult_t (cast to record_type*; do not free)
} find_params_t;

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
