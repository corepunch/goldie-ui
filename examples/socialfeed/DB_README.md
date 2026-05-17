# SimpleXMLDatabase - NeXTSTEP DBKit-Inspired Database API

A lightweight in-memory database with XML persistence, following Orion's WinAPI-style proc pattern.

## Design Philosophy

This implementation follows **Orion's architecture principles**:

1. **Proc-based pattern** - Database classes use `dbproc_t` (analogous to `winproc_t`)
2. **Message-based API** - CRUD via messages (like `evCreate`, `evPaint` for windows)
3. **Class registration** - `DB_CLASS(db_simple_xml)` (like `UI_CLASS(win_button)`)
4. **Userdata pattern** - Implementation stores context in `database_t->userdata`
5. **In-memory performance** - All data loaded into RAM for fast queries
6. **Schema as structs** - Database tables map to C structs (author_t, post_t, comment_t)

## Architecture

```
socialfeed.orion (schema definition)
    ↓
database.h (common database_t + dbproc_t + messages)
    ↓
db_simple_xml.h (schema structs: author_t, post_t, comment_t)
    ↓
db_simple_xml.c (proc implementation with message handlers)
    ↓
socialfeed_seed.xml (persistent data)
```

## Schema

Based on `socialfeed.orion` table definitions:

### Authors
- `id` (integer, primary key)
- `name` (string)
- `avatar` (string)

### Posts
- `id` (integer, primary key)
- `author_id` (integer, foreign key → authors.id)
- `title` (string)
- `body` (string)
- `like_count` (integer)
- `comment_count` (integer, auto-calculated)

### Comments
- `id` (integer, primary key)
- `post_id` (integer, foreign key → posts.id)
- `author_id` (integer, foreign key → authors.id)
- `text` (string)
- `like_count` (integer)

## API Overview

### Lifecycle (message-based)
```c
// Register database class (like registering a window class)
DB_CLASS(db_simple_xml);

// Create database (sends dbCreate + dbLoad)
database_t *db = create_database("socialfeed", "db_simple_xml", "socialfeed_seed.xml");

// ... CRUD operations via messages ...

// Destroy (sends dbSave if dirty, then dbDestroy)
destroy_database(db);
```

### Message-based CRUD

```c
// ── Insert ────────────────────────────────────────────────────
post_t post_data = { .author_id = 1 };
strcpy(post_data.title, "Hello World");
strcpy(post_data.body, "First post!");
post_t *post = (post_t *)send_db_message(db, dbInsert, TABLE_POSTS, &post_data);

// ── Find ──────────────────────────────────────────────────────
find_params_t find = {
  .table_id = TABLE_AUTHORS,
  .search_field = 1,  // by name
  .search_value.str_value = "alice"
};
author_t *author_ptr = NULL;
find.result_out = (void **)&author_ptr;
send_db_message(db, dbFind, TABLE_AUTHORS, &find);

// ── Update ────────────────────────────────────────────────────
post->like_count++;
send_db_message(db, dbUpdate, TABLE_POSTS, post);

// ── Delete ────────────────────────────────────────────────────
send_db_message(db, dbDelete, TABLE_POSTS, (void *)(intptr_t)post_id);

// ── Fetch (query) ─────────────────────────────────────────────
fetch_params_t fetch = { 
  .table_id = TABLE_COMMENTS, 
  .filter_field = 2,  // filter by post_id
  .filter_value = post_id 
};
void **results = NULL;
int count = send_db_message(db, dbFetch, TABLE_COMMENTS, &fetch);
results = fetch.results_out;
for (int i = 0; i < count; i++) {
  comment_t *c = (comment_t *)results[i];
  printf("%s\n", c->text);
}
free(results);
```

## Database Messages

Analogous to window messages (`evCreate`, `evPaint`, etc.):

| Message | wparam | lparam | Returns | Notes |
|---------|--------|--------|---------|-------|
| `dbCreate` | 0 | `const char *source_path` | bool | Allocate userdata context |
| `dbDestroy` | 0 | NULL | bool | Free userdata |
| `dbLoad` | 0 | NULL | bool | Load from XML file |
| `dbSave` | 0 | NULL | bool | Save to XML (only if dirty) |
| `dbInsert` | `TABLE_*` | `record_data_ptr` | `record_ptr` | Insert new record |
| `dbUpdate` | `TABLE_*` | `record_ptr` | bool | Update existing record |
| `dbDelete` | `TABLE_*` | `(void*)(intptr_t)id` | bool | Delete by ID |
| `dbFetch` | `TABLE_*` | `fetch_params_t*` | count | Query records |
| `dbFind` | `TABLE_*` | `find_params_t*` | `record_ptr` | Find single record |
| `dbGetDirty` | 0 | NULL | bool | Check if needs save |

## Features

✅ **WinAPI-style proc pattern** - Same architecture as window controls  
✅ **Message-based dispatch** - Consistent with framework design  
✅ **Class registration** - DB_CLASS() macro like UI_CLASS()  
✅ **Auto-incrementing primary keys** - No manual ID management  
✅ **Foreign key relationships** - Enforced in schema  
✅ **Cascading deletes** - Deleting a post removes its comments  
✅ **Computed fields** - `post.comment_count` auto-updated  
✅ **Dirty tracking** - Only saves when data changes  

## Comparison: Proc Pattern vs. Direct Functions

**Old style** (direct function calls):
```c
simple_xml_db_t *db = db_create("seed.xml");
db_load(db);
post_t *post = db_post_insert(db, author_id, title, body);
author_t *author = db_author_find_by_name(db, "alice");
db_save(db);
db_free(db);
```

**New style** (proc + messages, like windows):
```c
DB_CLASS(db_simple_xml);  // register once
database_t *db = create_database("feed", "db_simple_xml", "seed.xml");
send_db_message(db, dbInsert, TABLE_POSTS, &post_data);
send_db_message(db, dbFind, TABLE_AUTHORS, &find_params);
destroy_database(db);  // auto-saves if dirty
```

**Why?**  
Consistency with Orion's window system. Windows use `create_window()` + `send_message()` + `winproc_t`, so databases use `create_database()` + `send_db_message()` + `dbproc_t`.

## Integration with Orion Forms

The binding system uses this API automatically:

```xml
<!-- Form bindings trigger CRUD operations -->
<combobox name="author" 
          bind="posts.author_id"
          source="authors"
          display="name"
          value="id" />
          
<button action="posts.insert" ... />
```

When the user clicks "Post", the framework:
1. Collects values from bound controls
2. Calls `send_db_message(db, dbInsert, TABLE_POSTS, &post_data)`
3. Updates the feed tableview automatically
4. Marks database dirty for next save

## Building

```bash
cd examples/socialfeed
gcc -o db_test db_test.c db_simple_xml.c ../../user/database.c -I../../
./db_test
```

## TODO

- [ ] Full XML parsing (currently uses stub data)
- [ ] XML saving (currently prints to stdout)
- [ ] Query filtering (WHERE clauses)
- [ ] Sorting/ordering
- [ ] Transactions
- [ ] Indexing for faster lookups
