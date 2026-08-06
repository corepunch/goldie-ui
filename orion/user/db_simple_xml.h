// SimpleXMLDatabase — framework-provided generic XML database implementation.
//
// Provides a reusable database backend that:
//   - Stores records in dynamic arrays (auto-growing)
//   - Auto-increments primary keys
//   - Loads/saves XML via reflection (uses generated field metadata)
//   - Handles CRUD via message passing (dbproc_t pattern)
//
// Apps include this file directly or use it as a reference implementation.
// Framework provides db_load_record_from_xml / db_save_record_to_xml helpers.

#include "database.h"
#include <platform/platform.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

// ═══════════════════════════════════════════════════════════════════════════
// Array Helpers (shared by all table operations)
// ═══════════════════════════════════════════════════════════════════════════

void dbx_array_ensure_capacity(void **rows, int *capacity, int count, size_t row_size, int initial_capacity) {
    if (count < *capacity)
        return;
    int next_capacity = (*capacity == 0) ? initial_capacity : (*capacity * 2);
    *rows = realloc(*rows, (size_t)next_capacity * row_size);
    *capacity = next_capacity;
}

void *dbx_array_append_with_auto_id(void **rows, int *count, int *capacity, size_t row_size, size_t id_offset, int *next_id, int initial_capacity) {
    dbx_array_ensure_capacity(rows, capacity, *count, row_size, initial_capacity);
    char *row = (char *)(*rows) + ((size_t)(*count) * row_size);
    *count = *count + 1;
    *(int *)(row + id_offset) = (*next_id)++;
    return row;
}

void *dbx_array_append_copy(void **rows, int *count, int *capacity, size_t row_size, const void *src, int initial_capacity) {
    dbx_array_ensure_capacity(rows, capacity, *count, row_size, initial_capacity);
    char *row = (char *)(*rows) + ((size_t)(*count) * row_size);
    *count = *count + 1;
    memcpy(row, src, row_size);
    return row;
}

void *dbx_array_find_by_id(void *rows, int count, size_t row_size, size_t id_offset, int id) {
    char *base = (char *)rows;
    for (int i = 0; i < count; i++) {
        char *row = base + ((size_t)i * row_size);
        if (*(int *)(row + id_offset) == id)
            return row;
    }
    return NULL;
}

bool dbx_array_delete_by_id(void *rows, int *count, size_t row_size, size_t id_offset, int id) {
    char *base = (char *)rows;
    for (int i = 0; i < *count; i++) {
        char *row = base + ((size_t)i * row_size);
        if (*(int *)(row + id_offset) == id) {
            memmove(row, row + row_size, ((size_t)(*count - i - 1) * row_size));
            *count = *count - 1;
            return true;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// XML Table Loading/Saving (uses framework reflection helpers)
// ═══════════════════════════════════════════════════════════════════════════

void dbx_load_table_rows(xmlNode *table, const char *row_tag,
                         void **rows, int *count, int *capacity,
                         size_t row_size, size_t id_offset, int *next_id,
                         const db_field_meta_t *fields, int field_count,
                         int initial_capacity) {
    void *record = calloc(1, row_size);
    if (!record)
        return;
    for (xmlNode *row = table->children; row; row = row->next) {
        if (row->type != XML_ELEMENT_NODE) continue;
        if (xmlStrcmp(row->name, (const xmlChar *)row_tag) != 0) continue;
        if (!db_load_record_from_xml(row, record, fields, field_count))
            continue;
        dbx_array_append_copy(rows, count, capacity, row_size, record, initial_capacity);
        int id = *(int *)((char *)record + id_offset);
        if (id >= *next_id)
            *next_id = id + 1;
    }
    free(record);
}

void dbx_save_table_rows(xmlNodePtr root, const char *table_tag, const char *row_tag,
                         void *rows, int count, size_t row_size,
                         const db_field_meta_t *fields, int field_count) {
    xmlNodePtr table = xmlNewChild(root, NULL, (const xmlChar *)table_tag, NULL);
    for (int i = 0; i < count; i++) {
        db_save_record_to_xml(table, row_tag, (char *)rows + ((size_t)i * row_size), fields, field_count);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Result List Building
// ═══════════════════════════════════════════════════════════════════════════

lresult_t dbx_fetch_rows(void *rows, int count, size_t row_size,
                         bool use_filter, size_t filter_offset, int filter_value) {
    result_node_t *head = NULL, *tail = NULL;
    for (int i = 0; i < count; i++) {
        char *row = (char *)rows + ((size_t)i * row_size);
        if (use_filter && (*(int *)(row + filter_offset) != filter_value))
            continue;
        result_node_t *node = malloc(sizeof(result_node_t) + sizeof(void *));
        node->next = NULL;
        *(void **)node->data = row;
        if (tail) tail->next = node;
        else head = node;
        tail = node;
    }
    return (lresult_t)head;
}

// ═══════════════════════════════════════════════════════════════════════════
// Standard Database Procedure
//
// This is the generic message handler. Apps provide table-specific callbacks
// via dbx_table_ops_t to customize insert/delete/find/fetch behavior.
//
// Usage:
//   1. Define dbx_context_t with your table arrays
//   2. Define dbx_table_ops_t with your table-specific functions
//   3. Call dbx_main() as your dbproc_t
// ═══════════════════════════════════════════════════════════════════════════

typedef struct dbx_table_ops dbx_table_ops_t;

typedef void *(*dbx_find_fn)(void *ctx, int search_field, uintptr_t search_value);
typedef bool (*dbx_delete_fn)(void *ctx, int record_id);
typedef void *(*dbx_insert_fn)(void *ctx, const void *record_data);
typedef lresult_t(*dbx_fetch_fn)(void *ctx, int filter_field, int filter_value);

struct dbx_table_ops {
    int table_id;
    size_t row_size;
    size_t id_offset;
    dbx_find_fn find_fn;
    dbx_delete_fn delete_fn;
    dbx_insert_fn insert_fn;
    dbx_fetch_fn fetch_fn;
    int initial_capacity;
};

typedef struct {
    void *userdata;
    int table_count;
    const dbx_table_ops_t *tables;
} dbx_context_t;

static void *dbx_table_find(void *ctx, int table_id, int search_field, uintptr_t search_value) {
    dbx_context_t *dbx = (dbx_context_t *)ctx;
    for (int i = 0; i < dbx->table_count; i++) {
        if (dbx->tables[i].table_id == table_id)
            return dbx->tables[i].find_fn(dbx->userdata, search_field, search_value);
    }
    return NULL;
}

static bool dbx_table_delete(void *ctx, int table_id, int record_id) {
    dbx_context_t *dbx = (dbx_context_t *)ctx;
    for (int i = 0; i < dbx->table_count; i++) {
        if (dbx->tables[i].table_id == table_id)
            return dbx->tables[i].delete_fn(dbx->userdata, record_id);
    }
    return false;
}

static void *dbx_table_insert(void *ctx, int table_id, const void *record_data) {
    dbx_context_t *dbx = (dbx_context_t *)ctx;
    for (int i = 0; i < dbx->table_count; i++) {
        if (dbx->tables[i].table_id == table_id)
            return dbx->tables[i].insert_fn(dbx->userdata, record_data);
    }
    return NULL;
}

static lresult_t dbx_table_fetch(void *ctx, int table_id, int filter_field, int filter_value) {
    dbx_context_t *dbx = (dbx_context_t *)ctx;
    for (int i = 0; i < dbx->table_count; i++) {
        if (dbx->tables[i].table_id == table_id)
            return dbx->tables[i].fetch_fn(dbx->userdata, filter_field, filter_value);
    }
    return (lresult_t)NULL;
}

lresult_t dbx_main(database_t *db, uint32_t msg, uint32_t wparam, void *lparam,
                   dbx_context_t *ctx,
                   void (*load_fn)(dbx_context_t *, xmlNode *root),
                   void (*save_fn)(dbx_context_t *, xmlNodePtr root)) {
    switch (msg) {
        case dbCreate:
            return 1;

        case dbDestroy:
            return 1;

        case dbLoad: {
            if (!ctx) return 0;
            xmlDoc *doc = xmlReadFile(db->source_path, NULL, 0);
            if (!doc) {
                printf("dbx: Failed to parse %s\n", db->source_path);
                return 0;
            }
            xmlNode *root = xmlDocGetRootElement(doc);
            if (!root) {
                xmlFreeDoc(doc);
                return 0;
            }
            if (load_fn)
                load_fn(ctx, root);
            xmlFreeDoc(doc);
            db->dirty = false;
            return 1;
        }

        case dbSave: {
            if (!ctx || !db->dirty) return 0;
            xmlDocPtr doc = xmlNewDoc((const xmlChar *)"1.0");
            if (!doc) return 0;
            xmlNodePtr root = xmlNewNode(NULL, (const xmlChar *)db->name);
            xmlDocSetRootElement(doc, root);
            if (save_fn)
                save_fn(ctx, root);
            int result = xmlSaveFormatFileEnc(db->source_path, doc, "UTF-8", 1);
            xmlFreeDoc(doc);
            if (result == -1) {
                printf("dbx: Failed to write %s\n", db->source_path);
                return 0;
            }
            db->dirty = false;
            return 1;
        }

        case dbInsert: {
            if (!ctx) return (lresult_t)NULL;
            void *rec = dbx_table_insert(ctx, wparam, lparam);
            if (rec)
                db->dirty = true;
            return (lresult_t)rec;
        }

        case dbUpdate: {
            if (!ctx) return 0;
            db->dirty = true;
            return 1;
        }

        case dbDelete: {
            if (!ctx) return 0;
            bool success = dbx_table_delete(ctx, wparam, (int)(intptr_t)lparam);
            if (success)
                db->dirty = true;
            return success ? 1 : 0;
        }

        case dbFetch: {
            if (!ctx) return (lresult_t)NULL;
            return dbx_table_fetch(ctx, LOWORD(wparam), HIWORD(wparam), (int)(intptr_t)lparam);
        }

        case dbFind: {
            if (!ctx) return (lresult_t)NULL;
            return (lresult_t)dbx_table_find(ctx, LOWORD(wparam), HIWORD(wparam), (uintptr_t)lparam);
        }

        case dbGetDirty:
            return db->dirty ? 1 : 0;

        case dbGetSchema:
            return (lresult_t)NULL;

        case dbGetFieldMeta:
            return (lresult_t)NULL;

        case dbGetApi:
            return (lresult_t)NULL;
    }

    return 0;
}