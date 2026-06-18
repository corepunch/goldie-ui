# Controller Layer

Controllers handle business logic and database operations.

## Correct App State

```c
// Application state structure
typedef struct {
    database_t *db;
    window_t *main_win;
    window_t *menubar_win;
    window_t *content_win;
    window_t *feed_win;
    hinstance_t hinstance;
    accel_table_t *accel;
    int selected_idx;
} app_state_t;

// Global app state pointer
app_state_t *g_app = NULL;

// Initialize app state
app_state_t *app_init(void) {
    app_state_t *app = (app_state_t *)calloc(1, sizeof(app_state_t));
    if (!app) return NULL;
    app->selected_idx = -1;
    return app;
}

// Cleanup app state
void app_shutdown(app_state_t *app) {
    if (!app) return;
    if (app->accel) free_accelerators(app->accel);
    free(app);
}
```

## Database Operations

### Insert Record
```c
// Create new record
item_t item = { .name = "New Item" };
item_t *inserted = (item_t *)send_db_message(g_app->db, dbInsert, ID_DB_ITEMS, &item);
if (inserted) {
    printf("Inserted item with id=%d\n", inserted->id);
}
```

### Find Record
```c
// Find by ID
item_t *found = (item_t *)send_db_message(g_app->db, dbFind,
    MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)item_id);
if (found) {
    printf("Found item: %s\n", found->name);
}
```

### Update Record
```c
// Update existing record
found->value = 42;
bool success = send_db_message(g_app->db, dbUpdate, ID_DB_ITEMS, found) != 0;
```

### Delete Record
```c
// Delete by ID
bool success = send_db_message(g_app->db, dbDelete, ID_DB_ITEMS,
    (void *)(intptr_t)item_id) != 0;
```

### Fetch Records
```c
// Fetch all records
result_node_t *items = (result_node_t *)send_db_message(g_app->db, dbFetch,
    MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)0);
int count = count_result_list(items);

// Iterate through results
result_node_t *node = items;
while (node) {
    item_t *item = *(item_t **)node->data;
    printf("Item: %s\n", item->name);
    node = node->next;
}

// Free result list
free_result_list(items);
```

### Fetch with Filter
```c
// Fetch records with filter
result_node_t *filtered = (result_node_t *)send_db_message(g_app->db, dbFetch,
    MAKEDWORD(ID_DB_ITEMS, ID_DB_ITEMS_CATEGORY), (void *)(intptr_t)category_id);
// ... process filtered results ...
free_result_list(filtered);
```

## Incorrect Database Operations

```c
// WRONG: Not checking return values
send_db_message(g_app->db, dbInsert, ID_DB_ITEMS, &item);

// WRONG: Not freeing result lists
result_node_t *items = send_db_message(g_app->db, dbFetch, ...);
// Memory leak!

// WRONG: Using wrong table ID
send_db_message(g_app->db, dbInsert, ID_DB_AUTHORS, &item); // Wrong table

// WRONG: Not casting return values
item_t *item = send_db_message(g_app->db, dbFind, ...); // Missing cast
```

## Business Logic Functions

```c
// Delete post at index
bool app_delete_post(int index) {
    if (!g_app || !g_app->db || index < 0) return false;
    
    // Fetch all posts to get the post at index
    result_node_t *posts = (result_node_t *)send_db_message(g_app->db, dbFetch,
        MAKEDWORD(ID_DB_POSTS, 0), (void *)(intptr_t)0);
    if (!posts) return false;
    
    // Navigate to the post at index
    result_node_t *node = posts;
    for (int i = 0; i < index && node; i++)
        node = node->next;
    
    if (!node) {
        free_result_list(posts);
        return false;
    }
    
    db_post_t *post = *(db_post_t **)node->data;
    int post_id = post->id;
    free_result_list(posts);
    
    // Delete from database
    return send_db_message(g_app->db, dbDelete, ID_DB_POSTS,
        (void *)(intptr_t)post_id) != 0;
}
```

## Common Mistakes

1. **Not checking return values** — silent failures
2. **Not freeing result lists** — memory leaks
3. **Using wrong table IDs** — runtime errors
4. **Not casting return values** — type errors
5. **Not validating inputs** — null pointer crashes