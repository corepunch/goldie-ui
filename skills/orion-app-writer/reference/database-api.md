# Database API Reference

Complete API for database operations, messages, and schema.

## Database Lifecycle

```c
// Create database instance
database_t *create_database(const char *name, const char *class_name, const char *source_path);

// Destroy database (saves if dirty)
void destroy_database(database_t *db);

// Send message to database
lresult_t send_db_message(database_t *db, uint32_t msg, uint32_t wparam, void *lparam);

// Register database class (call once at startup)
DB_CLASS(proc_name);

// Register database instance (for declarative forms)
bool register_database(const char *name, database_t *db);

// Lookup registered database
database_t *get_database_by_name(const char *name);

// Set global database for framework
void ui_set_database(database_t *db);
```

## Database Messages

### Lifecycle Messages

| Message | wparam | lparam | Returns | Description |
|---------|--------|--------|---------|-------------|
| `dbCreate` | 0 | `const char *source_path` | 1 on success | Allocate context |
| `dbDestroy` | 0 | NULL | 1 on success | Free context |
| `dbLoad` | 0 | NULL | 1 on success | Load from XML |
| `dbSave` | 0 | NULL | 1 on success | Save to XML (only if dirty) |

### CRUD Messages

| Message | wparam | lparam | Returns | Description |
|---------|--------|--------|---------|-------------|
| `dbInsert` | `TABLE_*` | `record_data_ptr` | `record_ptr` | Insert new record |
| `dbUpdate` | `TABLE_*` | `record_ptr` | 1 on success | Update existing record |
| `dbDelete` | `TABLE_*` | `(void*)(intptr_t)id` | 1 on success | Delete by ID |

### Query Messages

| Message | wparam | lparam | Returns | Description |
|---------|--------|--------|---------|-------------|
| `dbFetch` | `MAKEDWORD(table, filter_field)` | `(intptr_t)filter_value` | `result_node_t*` | Query records |
| `dbFind` | `MAKEDWORD(table, search_field)` | `(intptr_t)value` or `(void*)str` | `record_ptr` | Find single record |

### Metadata Messages

| Message | wparam | lparam | Returns | Description |
|---------|--------|--------|---------|-------------|
| `dbGetDirty` | 0 | NULL | 0 or 1 | Check if needs save |
| `dbGetSchema` | 0 | 0 | `db_schema_def_t*` | Get schema metadata |
| `dbGetFieldMeta` | `TABLE_*` | `int* count_out` | `db_field_meta_t*` | Get field metadata |
| `dbGetApi` | 0 | 0 | `db_api_def_t*` | Get API definition |

## MAKEDWORD Macro

```c
// Combine two 16-bit values into 32-bit
uint32_t MAKEDWORD(uint16_t low, uint16_t high);

// Extract 16-bit values
uint16_t LOWORD(uint32_t value);  // Low 16 bits
uint16_t HIWORD(uint32_t value);  // High 16 bits
```

## Result List

```c
// Result node structure
typedef struct result_node_s {
    void *next;          // Next node (NULL for last)
    char data[];         // Flexible array member - record data
} result_node_t;

// Count results in list
int count_result_list(void *head);

// Free result list
void free_result_list(void *head);
```

## Field Types

```c
typedef enum {
    DB_TYPE_INT,      // Integer
    DB_TYPE_STRING,   // String (requires length)
    DB_TYPE_BOOL,     // Boolean
    DB_TYPE_FLOAT,    // Float
    DB_TYPE_DOUBLE    // Double
} db_field_type_t;
```

## Field Metadata

```c
typedef struct {
    uint32_t field_id;        // Generated ID_DB_* field id
    const char *name;         // Field name (e.g., "author_id")
    db_field_type_t type;     // C type
    size_t offset;            // offsetof(struct_t, field)
    int length;               // For strings: buffer size; else 0
} db_field_meta_t;
```

## Schema Structures

```c
// Table schema
typedef struct {
    uint32_t table_id;                    // TABLE_* / ID_DB_* table id
    const char *name;                     // Table name (e.g., "posts")
    uint32_t model_id;                    // Generated model id
    const char *model;                    // Optional logical model name
    const db_field_schema_t *fields;
    int field_count;
    const db_join_schema_t *joins;
    int join_count;
} db_table_schema_t;

// Database schema
typedef struct {
    const char *name;                     // Database instance name
    const char *class_name;               // Database class/proc name
    const char *source_path;              // Backing source, if any
    const db_table_schema_t *tables;
    int table_count;
} db_schema_def_t;
```

## Schema Query Functions

```c
const db_table_schema_t *db_schema_find_table_by_id(const db_schema_def_t *schema, uint32_t table_id);
const db_table_schema_t *db_schema_find_table_by_name(const db_schema_def_t *schema, const char *name);
const db_field_schema_t *db_table_find_field_by_id(const db_table_schema_t *table, uint32_t field_id);
const db_field_schema_t *db_table_find_field_by_name(const db_table_schema_t *table, const char *name);
```

## XML Loading/Saving

```c
// Load field from XML node
bool db_load_field_from_xml(xmlNodePtr node, void *record_base, const db_field_meta_t *field);

// Load record from XML node
bool db_load_record_from_xml(xmlNodePtr node, void *record, const db_field_meta_t *fields, int field_count);

// Save field to XML node
bool db_save_field_to_xml(xmlNodePtr node, const void *record_base, const db_field_meta_t *field);

// Save record to XML node
xmlNodePtr db_save_record_to_xml(xmlNodePtr parent, const char *element_name,
                                 const void *record, const db_field_meta_t *fields, int field_count);
```

## Database Implementation Template

```c
#include "../../ui.h"
#include "myapp.h"
#include <libxml/parser.h>
#include <libxml/tree.h>

typedef struct {
    db_item_t *items;
    int item_count;
    int item_capacity;
    int next_item_id;
} simple_xml_context_t;

lresult_t db_simple_xml(database_t *db, uint32_t msg, uint32_t wparam, void *lparam) {
    simple_xml_context_t *ctx = (simple_xml_context_t *)db->userdata;
    
    switch (msg) {
        case dbCreate: {
            ctx = calloc(1, sizeof(simple_xml_context_t));
            ctx->next_item_id = 1;
            db->userdata = ctx;
            return 1;
        }
        
        case dbDestroy: {
            free(ctx->items);
            free(ctx);
            db->userdata = NULL;
            return 1;
        }
        
        case dbLoad: {
            // Parse XML and load records
            xmlDoc *doc = xmlReadFile(db->source_path, NULL, 0);
            // ... load tables ...
            xmlFreeDoc(doc);
            db->dirty = false;
            return 1;
        }
        
        case dbSave: {
            if (!db->dirty) return 0;
            // Create XML and save
            xmlDocPtr doc = xmlNewDoc((const xmlChar *)"1.0");
            // ... save tables ...
            xmlSaveFormatFileEnc(db->source_path, doc, "UTF-8", 1);
            xmlFreeDoc(doc);
            db->dirty = false;
            return 1;
        }
        
        case dbInsert: {
            // Insert record, auto-increment ID
            // Return pointer to inserted record
        }
        
        case dbUpdate: {
            db->dirty = true;
            return 1;
        }
        
        case dbDelete: {
            // Delete record by ID
            db->dirty = true;
            return 1;
        }
        
        case dbFetch: {
            // Return linked list of records
            // Use MAKEDWORD(table_id, filter_field) from wparam
        }
        
        case dbFind: {
            // Find single record by ID or field
        }
        
        case dbGetDirty:
            return db->dirty ? 1 : 0;
            
        case dbGetFieldMeta: {
            int *count_out = (int *)lparam;
            switch (wparam) {
                case ID_DB_ITEMS:
                    if (count_out) *count_out = ARRAY_LEN(items_fields);
                    return (lresult_t)items_fields;
                default:
                    if (count_out) *count_out = 0;
                    return 0;
            }
        }
    }
    return 0;
}
```

## Seed Data XML Format

```xml
<?xml version="1.0" encoding="UTF-8"?>
<database>
    <items>
        <item id="1" name="Item 1" value="10" />
        <item id="2" name="Item 2" value="20" />
    </items>
    <authors>
        <author id="1" name="alice" />
    </authors>
</database>
```