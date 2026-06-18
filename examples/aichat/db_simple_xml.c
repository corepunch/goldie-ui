// SimpleXMLDatabase implementation for AI Chat
#include "../../ui.h"
#include "aichat.h"
#include "../../platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

// ═══════════════════════════════════════════════════════════════════════════
// Internal Context (stored in database_t->userdata)
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    // In-memory tables
    db_session_t *sessions;
    int session_count;
    int session_capacity;
    
    db_message_t *messages;
    int message_count;
    int message_capacity;
    
    // Auto-increment counters
    int next_session_id;
    int next_message_id;
} simple_xml_context_t;

// ═══════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ═══════════════════════════════════════════════════════════════════════════

static void array_ensure_capacity(void **rows, int *capacity, int count, size_t row_size, int initial_capacity) {
    if (count < *capacity)
        return;
    int next_capacity = (*capacity == 0) ? initial_capacity : (*capacity * 2);
    *rows = realloc(*rows, (size_t)next_capacity * row_size);
    *capacity = next_capacity;
}

static void *array_append_with_auto_id(void **rows, int *count, int *capacity, size_t row_size, size_t id_offset, int *next_id, int initial_capacity) {
    array_ensure_capacity(rows, capacity, *count, row_size, initial_capacity);
    char *row = (char *)(*rows) + ((size_t)(*count) * row_size);
    *count = *count + 1;
    *(int *)(row + id_offset) = (*next_id)++;
    return row;
}

static void *array_append_copy(void **rows, int *count, int *capacity, size_t row_size, const void *src, int initial_capacity) {
    array_ensure_capacity(rows, capacity, *count, row_size, initial_capacity);
    char *row = (char *)(*rows) + ((size_t)(*count) * row_size);
    *count = *count + 1;
    memcpy(row, src, row_size);
    return row;
}

static void *array_find_by_id(void *rows, int count, size_t row_size, size_t id_offset, int id) {
    char *base = (char *)rows;
    for (int i = 0; i < count; i++) {
        char *row = base + ((size_t)i * row_size);
        if (*(int *)(row + id_offset) == id)
            return row;
    }
    return NULL;
}

static bool array_delete_by_id(void *rows, int *count, size_t row_size, size_t id_offset, int id) {
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

static void load_table_rows(xmlNode *table, const char *row_tag, void **rows, int *count, int *capacity, size_t row_size, size_t id_offset, int *next_id, const db_field_meta_t *fields, int field_count, int initial_capacity) {
    void *record = calloc(1, row_size);
    if (!record)
        return;
    for (xmlNode *row = table->children; row; row = row->next) {
        if (row->type != XML_ELEMENT_NODE) continue;
        if (xmlStrcmp(row->name, (const xmlChar *)row_tag) != 0) continue;
        if (!db_load_record_from_xml(row, record, fields, field_count))
            continue;
        array_append_copy(rows, count, capacity, row_size, record, initial_capacity);
        int id = *(int *)((char *)record + id_offset);
        if (id >= *next_id)
            *next_id = id + 1;
    }
    free(record);
}

static void save_table_rows(xmlNodePtr root, const char *table_tag, const char *row_tag, void *rows, int count, size_t row_size, const db_field_meta_t *fields, int field_count) {
    xmlNodePtr table = xmlNewChild(root, NULL, (const xmlChar *)table_tag, NULL);
    for (int i = 0; i < count; i++) {
        db_save_record_to_xml(table, row_tag, (char *)rows + ((size_t)i * row_size), fields, field_count);
    }
}

static lresult_t fetch_rows(void *rows, int count, size_t row_size, bool use_filter, size_t filter_offset, int filter_value) {
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

static void *table_find_record(simple_xml_context_t *ctx, int table_id, int search_field, uintptr_t search_value) {
    switch (table_id) {
        case ID_DB_SESSIONS:
            if (search_field == 0 || search_field == ID_DB_SESSIONS_ID)
                return array_find_by_id(ctx->sessions, ctx->session_count, sizeof(db_session_t), offsetof(db_session_t, id), (int)(intptr_t)search_value);
            break;
        case ID_DB_MESSAGES:
            if (search_field == 0 || search_field == ID_DB_MESSAGES_ID)
                return array_find_by_id(ctx->messages, ctx->message_count, sizeof(db_message_t), offsetof(db_message_t, id), (int)(intptr_t)search_value);
            break;
    }
    return NULL;
}

static bool table_delete_record(simple_xml_context_t *ctx, int table_id, int record_id) {
    switch (table_id) {
        case ID_DB_SESSIONS:
            // Delete all messages in this session first
            for (int i = ctx->message_count - 1; i >= 0; i--) {
                if (ctx->messages[i].session_id == record_id) {
                    memmove(&ctx->messages[i], &ctx->messages[i + 1], (size_t)(ctx->message_count - i - 1) * sizeof(db_message_t));
                    ctx->message_count--;
                }
            }
            return array_delete_by_id(ctx->sessions, &ctx->session_count, sizeof(db_session_t), offsetof(db_session_t, id), record_id);
        case ID_DB_MESSAGES:
            return array_delete_by_id(ctx->messages, &ctx->message_count, sizeof(db_message_t), offsetof(db_message_t, id), record_id);
    }
    return false;
}

static void *table_insert_record(simple_xml_context_t *ctx, int table_id, const void *record_data) {
    switch (table_id) {
        case ID_DB_SESSIONS: {
            db_session_t *rec = array_append_with_auto_id((void **)&ctx->sessions, &ctx->session_count, &ctx->session_capacity,
                                                          sizeof(db_session_t), offsetof(db_session_t, id), &ctx->next_session_id, 16);
            memcpy(rec, record_data, sizeof(db_session_t));
            rec->id = ctx->next_session_id - 1;
            return rec;
        }
        case ID_DB_MESSAGES: {
            db_message_t *rec = array_append_with_auto_id((void **)&ctx->messages, &ctx->message_count, &ctx->message_capacity,
                                                          sizeof(db_message_t), offsetof(db_message_t, id), &ctx->next_message_id, 64);
            memcpy(rec, record_data, sizeof(db_message_t));
            rec->id = ctx->next_message_id - 1;
            
            // Update session's updated_at timestamp
            db_session_t *session = array_find_by_id(ctx->sessions, ctx->session_count, sizeof(db_session_t), offsetof(db_session_t, id), rec->session_id);
            if (session) {
                time_t now = time(NULL);
                struct tm *tm = localtime(&now);
                strftime(session->updated_at, sizeof(session->updated_at), "%Y-%m-%d %H:%M:%S", tm);
            }
            
            return rec;
        }
    }
    return NULL;
}

static lresult_t table_fetch_records(simple_xml_context_t *ctx, int table_id, int filter_field, int filter_value) {
    switch (table_id) {
        case ID_DB_SESSIONS:
            return fetch_rows(ctx->sessions, ctx->session_count, sizeof(db_session_t), false, 0, 0);
        case ID_DB_MESSAGES:
            if (filter_field == ID_DB_MESSAGES_SESSION_ID)
                return fetch_rows(ctx->messages, ctx->message_count, sizeof(db_message_t), true, offsetof(db_message_t, session_id), filter_value);
            if (filter_field == 0)
                return fetch_rows(ctx->messages, ctx->message_count, sizeof(db_message_t), false, 0, 0);
            break;
    }
    return (lresult_t)NULL;
}

// ═══════════════════════════════════════════════════════════════════════════
// Database Procedure
// ═══════════════════════════════════════════════════════════════════════════

lresult_t db_simple_xml(database_t *db, uint32_t msg, uint32_t wparam, void *lparam) {
    simple_xml_context_t *ctx = (simple_xml_context_t *)db->userdata;
    
    switch (msg) {
        case dbCreate: {
            ctx = calloc(1, sizeof(simple_xml_context_t));
            if (!ctx) return 0;
            
            ctx->next_session_id = 1;
            ctx->next_message_id = 1;
            
            db->userdata = ctx;
            return 1;
        }
        
        case dbDestroy: {
            if (!ctx) return 0;
            
            free(ctx->sessions);
            free(ctx->messages);
            free(ctx);
            db->userdata = NULL;
            return 1;
        }
        
        case dbLoad: {
            if (!ctx) return 0;
            
            xmlDoc *doc = xmlReadFile(db->source_path, NULL, 0);
            if (!doc) {
                printf("db_simple_xml: Failed to parse %s\n", db->source_path);
                return 0;
            }
            
            xmlNode *root = xmlDocGetRootElement(doc);
            if (!root) {
                xmlFreeDoc(doc);
                return 0;
            }
            
            for (xmlNode *table = root->children; table; table = table->next) {
                if (table->type != XML_ELEMENT_NODE) continue;
                
                if (xmlStrcmp(table->name, (const xmlChar *)"sessions") == 0) {
                    load_table_rows(table, "session",
                                    (void **)&ctx->sessions, &ctx->session_count, &ctx->session_capacity,
                                    sizeof(db_session_t), offsetof(db_session_t, id), &ctx->next_session_id,
                                    STATIC_ARRAY(sessions_fields), 16);
                }
                else if (xmlStrcmp(table->name, (const xmlChar *)"messages") == 0) {
                    load_table_rows(table, "message",
                                    (void **)&ctx->messages, &ctx->message_count, &ctx->message_capacity,
                                    sizeof(db_message_t), offsetof(db_message_t, id), &ctx->next_message_id,
                                    STATIC_ARRAY(messages_fields), 64);
                }
            }
            
            xmlFreeDoc(doc);
            
            AI_DEBUG("Loaded from %s: %d sessions, %d messages", db->source_path, ctx->session_count, ctx->message_count);
            
            db->dirty = false;
            return 1;
        }
        
        case dbSave: {
            if (!ctx || !db->dirty) return 0;
            
            xmlDocPtr doc = xmlNewDoc((const xmlChar *)"1.0");
            if (!doc) return 0;
            
            xmlNodePtr root = xmlNewNode(NULL, (const xmlChar *)"aichat");
            xmlDocSetRootElement(doc, root);

            save_table_rows(root, "sessions", "session", ctx->sessions, ctx->session_count,
                            sizeof(db_session_t), STATIC_ARRAY(sessions_fields));
            save_table_rows(root, "messages", "message", ctx->messages, ctx->message_count,
                            sizeof(db_message_t), STATIC_ARRAY(messages_fields));
            
            int result = xmlSaveFormatFileEnc(db->source_path, doc, "UTF-8", 1);
            xmlFreeDoc(doc);
            
            if (result == -1) {
                printf("db_simple_xml: Failed to write %s\n", db->source_path);
                return 0;
            }
            
            AI_DEBUG("Saved to %s: %d sessions, %d messages", db->source_path, ctx->session_count, ctx->message_count);
            
            db->dirty = false;
            return 1;
        }
        
        case dbInsert: {
            if (!ctx) return (lresult_t)NULL;
            
            void *rec = table_insert_record(ctx, wparam, lparam);
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
            
            bool success = table_delete_record(ctx, wparam, (int)(intptr_t)lparam);
            if (success)
                db->dirty = true;
            return success ? 1 : 0;
        }
        
        case dbFetch: {
            if (!ctx) return (lresult_t)NULL;
            
            return table_fetch_records(ctx, LOWORD(wparam), HIWORD(wparam), (int)(intptr_t)lparam);
        }
        
        case dbFind: {
            if (!ctx) return (lresult_t)NULL;
            
            return (lresult_t)table_find_record(ctx, LOWORD(wparam), HIWORD(wparam), (uintptr_t)lparam);
        }
        
        case dbGetDirty:
            return db->dirty ? 1 : 0;

        case dbGetSchema: {
            // Return schema metadata
            return (lresult_t)NULL;
        }

        case dbGetFieldMeta: {
            int *count_out = (int *)lparam;
            switch (wparam) {
                case ID_DB_SESSIONS:
                    if (count_out) *count_out = ARRAY_LEN(sessions_fields);
                    return (lresult_t)sessions_fields;
                case ID_DB_MESSAGES:
                    if (count_out) *count_out = ARRAY_LEN(messages_fields);
                    return (lresult_t)messages_fields;
                default:
                    if (count_out) *count_out = 0;
                    return (lresult_t)NULL;
            }
        }

        case dbGetApi:
            return (lresult_t)NULL;
    }
    
    return 0;
}