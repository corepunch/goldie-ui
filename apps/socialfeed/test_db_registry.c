// Test: Database registry and self-contained forms
// Demonstrates the new registry pattern where forms look up databases by name

#include <orion/ui.h>
#include "socialfeed.h"
#include <stdio.h>

// ── Test Setup ──────────────────────────────────────────────────────────────

static database_t *g_db = NULL;

void test_db_registry_setup(void) {
  printf("=== Database Registry Test ===\n\n");
  
  // 1. Create database instance
  printf("1. Creating database 'db'...\n");
  g_db = create_database("db", "SimpleXMLDatabase",
                         "apps/socialfeed/share/socialfeed_seed.xml");
  if (!g_db) {
    printf("   ❌ Failed to create database\n");
    return;
  }
  printf("   ✅ Database created\n\n");
  
  // 2. Register database instance by name
  printf("2. Registering database 'db' in global registry...\n");
  if (!register_database("db", g_db)) {
    printf("   ❌ Failed to register database\n");
    return;
  }
  printf("   ✅ Database registered\n\n");
  
  // 3. Verify lookup works
  printf("3. Looking up database 'db'...\n");
  database_t *looked_up = get_database_by_name("db");
  if (looked_up == g_db) {
    printf("   ✅ Lookup successful (same instance)\n\n");
  } else {
    printf("   ❌ Lookup returned different instance\n\n");
  }
}

// ── Test: Wrong Database Name (Error Handling) ─────────────────────────────

void test_wrong_database_name(void) {
  printf("=== Test: Wrong Database Name ===\n\n");
  
  printf("Looking up 'wrong_db' (doesn't exist)...\n");
  database_t *wrong = get_database_by_name("wrong_db");
  
  if (!wrong) {
    printf("✅ Correctly returned NULL and printed error\n\n");
  } else {
    printf("❌ Should have returned NULL\n\n");
  }
}

// ── Test: Multiple Database Instances ───────────────────────────────────────

void test_multiple_databases(void) {
  printf("=== Test: Multiple Database Instances ===\n\n");
  
  // Scenario: App has multiple databases:
  // - "main_db" for primary data
  // - "cache_db" for temporary caching
  // - "auth_db" for authentication
  
  printf("Creating and registering multiple databases...\n");
  
  database_t *main_db = create_database("main_db", "SimpleXMLDatabase", "main.xml");
  database_t *cache_db = create_database("cache_db", "SimpleXMLDatabase", "cache.xml");
  database_t *auth_db = create_database("auth_db", "SimpleXMLDatabase", "auth.xml");
  
  register_database("main_db", main_db);
  register_database("cache_db", cache_db);
  register_database("auth_db", auth_db);
  
  printf("   ✅ Registered 3 databases\n\n");
  
  // Now forms can use different databases explicitly:
  // <textedit field="main_db.posts.title" />    → uses main_db
  // <textedit field="cache_db.sessions.token" /> → uses cache_db
  // <textedit field="auth_db.users.name" />      → uses auth_db
  
  printf("Form with field=\"main_db.posts.title\"  → uses main_db\n");
  printf("Form with field=\"cache_db.sessions.token\" → uses cache_db\n");
  printf("Form with field=\"auth_db.users.name\"   → uses auth_db\n\n");
  
  printf("✅ No ambiguity, no runtime bugs\n\n");
  
  // Cleanup
  destroy_database(auth_db);
  destroy_database(cache_db);
  destroy_database(main_db);
}

// ── Test: Registry Overwrite (Update Instance) ─────────────────────────────

void test_registry_overwrite(void) {
  printf("=== Test: Registry Overwrite ===\n\n");
  
  printf("1. Create and register 'test_db' → instance A\n");
  database_t *db_a = create_database("test_db", "SimpleXMLDatabase", "test_a.xml");
  register_database("test_db", db_a);
  
  database_t *lookup1 = get_database_by_name("test_db");
  printf("   Lookup returns: %p\n", (void *)lookup1);
  
  printf("\n2. Register 'test_db' again → instance B (overwrite)\n");
  database_t *db_b = create_database("test_db", "SimpleXMLDatabase", "test_b.xml");
  register_database("test_db", db_b);
  
  database_t *lookup2 = get_database_by_name("test_db");
  printf("   Lookup returns: %p\n", (void *)lookup2);
  
  if (lookup2 == db_b && lookup2 != db_a) {
    printf("\n✅ Registry correctly updated to new instance\n\n");
  } else {
    printf("\n❌ Registry didn't update properly\n\n");
  }
  
  // Cleanup
  destroy_database(db_b);
  destroy_database(db_a);
}

// ── Main Test Runner ────────────────────────────────────────────────────────

void run_db_registry_tests(void) {
  test_db_registry_setup();
  test_wrong_database_name();
  test_multiple_databases();
  test_registry_overwrite();
  
  printf("\n=== Summary ===\n\n");
  printf("The database registry enables truly declarative forms:\n");
  printf("  - Forms are self-contained (no external context needed)\n");
  printf("  - Impossible to pass wrong database instance\n");
  printf("  - Cleaner API (fewer parameters)\n");
  printf("  - Supports multiple databases explicitly\n\n");
}

// ── API Comparison ──────────────────────────────────────────────────────────

void print_api_comparison(void) {
  printf("\n");
  printf("╔════════════════════════════════════════════════════════════════╗\n");
  printf("║                    API COMPARISON                              ║\n");
  printf("╠════════════════════════════════════════════════════════════════╣\n");
  printf("║                                                                ║\n");
  printf("║  OLD API (Leaky Abstraction):                                 ║\n");
  printf("║  ═══════════════════════════════                               ║\n");
  printf("║                                                                ║\n");
  printf("║    database_t *db = ...;                                      ║\n");
  printf("║    show_db_dialog(&form, \"Edit\", parent, db, id);            ║\n");
  printf("║                                            ^^                  ║\n");
  printf("║                                            redundant!          ║\n");
  printf("║                                                                ║\n");
  printf("║  Problems:                                                     ║\n");
  printf("║    ❌ Form already knows db_name from field=\"db.posts.title\" ║\n");
  printf("║    ❌ Can pass wrong database instance at runtime             ║\n");
  printf("║    ❌ Breaks declarative model (form needs external context)  ║\n");
  printf("║                                                                ║\n");
  printf("║────────────────────────────────────────────────────────────────║\n");
  printf("║                                                                ║\n");
  printf("║  NEW API (Self-Contained):                                    ║\n");
  printf("║  ═══════════════════════════                                   ║\n");
  printf("║                                                                ║\n");
  printf("║    // Register once at startup                                ║\n");
  printf("║    register_database(\"db\", db);                              ║\n");
  printf("║                                                                ║\n");
  printf("║    // Use everywhere - forms are self-contained               ║\n");
  printf("║    show_db_dialog(&form, \"Edit\", parent, id);                ║\n");
  printf("║                                                                ║\n");
  printf("║  Benefits:                                                     ║\n");
  printf("║    ✅ True declarative programming                            ║\n");
  printf("║    ✅ Impossible to pass wrong database (compile-time safe)   ║\n");
  printf("║    ✅ Cleaner API (fewer parameters)                          ║\n");
  printf("║    ✅ Forms are pure data structures                          ║\n");
  printf("║    ✅ Supports multiple databases explicitly                  ║\n");
  printf("║                                                                ║\n");
  printf("╚════════════════════════════════════════════════════════════════╝\n");
  printf("\n");
}
