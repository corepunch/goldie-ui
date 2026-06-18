# Database API Reference

## Database Lifecycle

### Create Database
```c
database_t *create_database(const char *name, const char *class_name, const char *source_path);
```
- `name` — Database instance name
- `class_name` — Database class (e.g., "db_simple_xml")
- `source_path` — Path to XML seed file
- Returns: Database pointer or NULL on failure

### Destroy Database
```c
void destroy_database(database_t *db);
```
- `db` — Database pointer
- Saves if dirty, then frees resources

### Send Message
```c
lresult_t send_db_message(database_t *db, uint32_t msg, uint32_t wparam, void *lparam);
```
- `db` — Database pointer
- `msg` — Message ID (dbCreate, dbInsert, etc.)
- `wparam` — Message-specific parameter
- `lparam` — Message-specific parameter
- Returns: Message-specific result

## CRUD Operations

### Insert Record
```c
// Insert new record
item_t item = { .name = "New Item", .value = 42 };
item_t *inserted = (item_t *)send_db_message(db, dbInsert, ID_DB_ITEMS, &item);
```
- `wparam` — Table ID (ID_DB_ITEMS)
- `lparam` — Pointer to record data
- Returns: Pointer to inserted record (with auto-generated ID)

### Find Record
```c
// Find by ID
item_t *found = (item_t *)send_db_message(db, dbFind,
    MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)item_id);

// Find by field
item_t *found = (item_t *)send_db_message(db, dbFind,
    MAKEDWORD(ID_DB_ITEMS, ID_DB_ITEMS_NAME), (void *)"search name");
```
- `wparam` — MAKEDWORD(table_id, search_field_id)
- `lparam` — Search value (int for ID, const char* for string)
- Returns: Pointer to found record or NULL

### Update Record
```c
// Update existing record
found->value = 100;
bool success = send_db_message(db, dbUpdate, ID_DB_ITEMS, found) != 0;
```
- `wparam` — Table ID
- `lparam` — Pointer to updated record
- Returns: 1 on success, 0 on failure

### Delete Record
```c
// Delete by ID
bool success = send_db_message(db, dbDelete, ID_DB_ITEMS,
    (void *)(intptr_t)item_id) != 0;
```
- `wparam` — Table ID
- `lparam` — Record ID (cast to void*)
- Returns: 1 on success, 0 on failure

## Query Operations

### Fetch All Records
```c
// Fetch all records
result_node_t *items = (result_node_t *)send_db_message(db, dbFetch,
    MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)0);
int count = count_result_list(items);
free_result_list(items);
```

### Fetch with Filter
```c
// Fetch records with filter
result_node_t *filtered = (result_node_t *)send_db_message(db, dbFetch,
    MAKEDWORD(ID_DB_ITEMS, ID_DB_ITEMS_CATEGORY), (void *)(intptr_t)category_id);
int count = count_result_list(filtered);
free_result_list(filtered);
```

### Result List Operations
```c
// Count results
int count = count_result_list(head);

// Free result list
free_result_list(head);

// Iterate results
result_node_t *node = head;
while (node) {
    item_t *item = *(item_t **)node->data;
    // Process item
    node = node->next;
}
```

## Database Registration

### Register Database Class
```c
// Register database class (call once at startup)
DB_CLASS(db_simple_xml);
```

### Register Database Instance
```c
// Register database instance (for declarative forms)
register_database("db", db);

// Lookup registered database
database_t *db = get_database_by_name("db");
```

## Utility Macros

### MAKEDWORD
```c
// Combine two 16-bit values into 32-bit
uint32_t MAKEDWORD(uint16_t low, uint16_t high);
```

### LOWORD/HIWORD
```c
// Extract 16-bit values from 32-bit
uint16_t LOWORD(uint32_t value);
uint16_t HIWORD(uint32_t value);
```

## Common Patterns

### Insert and Get ID
```c
item_t item = { .name = "Test" };
item_t *inserted = (item_t *)send_db_message(db, dbInsert, ID_DB_ITEMS, &item);
int new_id = inserted->id;
```

### Find and Update
```c
item_t *item = (item_t *)send_db_message(db, dbFind,
    MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)item_id);
if (item) {
    item->value = 42;
    send_db_message(db, dbUpdate, ID_DB_ITEMS, item);
}
```

### Fetch and Process
```c
result_node_t *items = (result_node_t *)send_db_message(db, dbFetch,
    MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)0);

result_node_t *node = items;
while (node) {
    item_t *item = *(item_t **)node->data;
    printf("Item: %s = %d\n", item->name, item->value);
    node = node->next;
}

free_result_list(items);
```

### Delete with Cleanup
```c
// Delete record
send_db_message(db, dbDelete, ID_DB_ITEMS, (void *)(intptr_t)item_id);

// Verify deletion
item_t *deleted = (item_t *)send_db_message(db, dbFind,
    MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)item_id);
assert(deleted == NULL);
```

## Error Handling

### Check Return Values
```c
item_t *inserted = (item_t *)send_db_message(db, dbInsert, ID_DB_ITEMS, &item);
if (!inserted) {
    // Handle error
    return false;
}
```

### Check Success
```c
bool success = send_db_message(db, dbDelete, ID_DB_ITEMS,
    (void *)(intptr_t)item_id) != 0;
if (!success) {
    // Handle error
    return false;
}
```

### Validate Inputs
```c
if (!db || item_id <= 0) {
    return false;
}
```

## Memory Management

### Result Lists
```c
// Always free result lists after use
result_node_t *items = (result_node_t *)send_db_message(db, dbFetch, ...);
// ... process items ...
free_result_list(items); // Don't forget!
```

### Database Cleanup
```c
// Always destroy database when done
destroy_database(db);
```