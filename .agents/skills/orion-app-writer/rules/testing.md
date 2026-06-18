# Testing

Test database operations and dialog functionality.

## Database Tests

```c
#include "../../ui.h"
#include "myapp.h"
#include <stdio.h>
#include <assert.h>

// Test database creation and basic operations
void test_database_basic(void) {
    printf("=== Database Basic Test ===\n\n");
    
    // Create database
    database_t *db = create_database("test", "db_simple_xml", "test.xml");
    assert(db != NULL);
    printf("✅ Database created\n");
    
    // Register database
    register_database("test", db);
    printf("✅ Database registered\n");
    
    // Test insert
    item_t item = { .name = "Test Item", .value = 42 };
    item_t *inserted = (item_t *)send_db_message(db, dbInsert, ID_DB_ITEMS, &item);
    assert(inserted != NULL);
    assert(inserted->id > 0);
    printf("✅ Item inserted with id=%d\n", inserted->id);
    
    // Test find
    item_t *found = (item_t *)send_db_message(db, dbFind,
        MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)inserted->id);
    assert(found != NULL);
    assert(strcmp(found->name, "Test Item") == 0);
    assert(found->value == 42);
    printf("✅ Item found\n");
    
    // Test update
    found->value = 100;
    bool success = send_db_message(db, dbUpdate, ID_DB_ITEMS, found) != 0;
    assert(success);
    printf("✅ Item updated\n");
    
    // Test fetch
    result_node_t *items = (result_node_t *)send_db_message(db, dbFetch,
        MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)0);
    int count = count_result_list(items);
    assert(count == 1);
    free_result_list(items);
    printf("✅ Fetch returned %d item(s)\n", count);
    
    // Test delete
    success = send_db_message(db, dbDelete, ID_DB_ITEMS,
        (void *)(intptr_t)inserted->id) != 0;
    assert(success);
    printf("✅ Item deleted\n");
    
    // Verify deletion
    items = (result_node_t *)send_db_message(db, dbFetch,
        MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)0);
    count = count_result_list(items);
    assert(count == 0);
    free_result_list(items);
    printf("✅ Deletion verified\n");
    
    // Cleanup
    destroy_database(db);
    printf("✅ Database destroyed\n\n");
}
```

## Dialog Tests

```c
// Test database-aware dialog
void test_db_dialog(void) {
    printf("=== Database Dialog Test ===\n\n");
    
    // Create and register database
    database_t *db = create_database("test", "db_simple_xml", "test.xml");
    register_database("test", db);
    
    // Create test record
    item_t item = { .name = "Test Item", .value = 42 };
    item_t *inserted = (item_t *)send_db_message(db, dbInsert, ID_DB_ITEMS, &item);
    
    // Test edit dialog
    window_t *parent = NULL; // In real test, create a window
    uint32_t result = show_db_dialog(&edit_form, "Edit Item", parent, inserted->id);
    
    if (result == 1) {
        printf("✅ Dialog saved successfully\n");
    } else {
        printf("ℹ️ Dialog cancelled\n");
    }
    
    // Cleanup
    destroy_database(db);
    printf("✅ Test completed\n\n");
}
```

## Test Runner

```c
void run_all_tests(void) {
    printf("Running Orion App Tests...\n\n");
    
    test_database_basic();
    test_database_relationships();
    test_db_dialog();
    
    printf("=== All Tests Passed ===\n");
}

// Main entry point for tests
int main(int argc, char *argv[]) {
    run_all_tests();
    return 0;
}
```

## Test Database Setup

```c
// Create test database with seed data
void setup_test_database(void) {
    // Create temporary XML file
    FILE *f = fopen("test_seed.xml", "w");
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<database>\n");
    fprintf(f, "  <items>\n");
    fprintf(f, "    <item id=\"1\" name=\"Item 1\" value=\"10\" />\n");
    fprintf(f, "    <item id=\"2\" name=\"Item 2\" value=\"20\" />\n");
    fprintf(f, "  </items>\n");
    fprintf(f, "</database>\n");
    fclose(f);
}

// Cleanup test database
void cleanup_test_database(void) {
    remove("test_seed.xml");
    remove("test.xml");
}
```

## Common Test Patterns

### Test Insert and Find
```c
void test_insert_find(void) {
    item_t item = { .name = "Test" };
    item_t *inserted = (item_t *)send_db_message(db, dbInsert, ID_DB_ITEMS, &item);
    
    item_t *found = (item_t *)send_db_message(db, dbFind,
        MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)inserted->id);
    
    assert(found != NULL);
    assert(strcmp(found->name, "Test") == 0);
}
```

### Test Update
```c
void test_update(void) {
    item_t item = { .name = "Test", .value = 10 };
    item_t *inserted = (item_t *)send_db_message(db, dbInsert, ID_DB_ITEMS, &item);
    
    inserted->value = 20;
    send_db_message(db, dbUpdate, ID_DB_ITEMS, inserted);
    
    item_t *found = (item_t *)send_db_message(db, dbFind,
        MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)inserted->id);
    
    assert(found->value == 20);
}
```

### Test Delete
```c
void test_delete(void) {
    item_t item = { .name = "Test" };
    item_t *inserted = (item_t *)send_db_message(db, dbInsert, ID_DB_ITEMS, &item);
    
    send_db_message(db, dbDelete, ID_DB_ITEMS, (void *)(intptr_t)inserted->id);
    
    item_t *found = (item_t *)send_db_message(db, dbFind,
        MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)inserted->id);
    
    assert(found == NULL);
}
```

## Common Mistakes

1. **Not cleaning up test databases** — file leaks
2. **Not checking return values** — silent failures
3. **Not freeing result lists** — memory leaks
4. **Using wrong assertion macros** — unclear test failures
5. **Not testing edge cases** — missing boundary conditions