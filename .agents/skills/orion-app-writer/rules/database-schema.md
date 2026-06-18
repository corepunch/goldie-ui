# Database Schema

Define database tables in `.orion` files with field types, relationships, and constraints.

## Correct Schema

```xml
<databases>
    <database name="db" class="SimpleXMLDatabase" source="examples/myapp/share/seed.xml">
        <table name="authors">
            <field name="name" type="string" length="64"/>
            <field name="avatar" type="string" length="256"/>
            <field name="posts" type="relationship" relation="posts" many="true"/>
        </table>

        <table name="posts">
            <field name="author" type="relationship" relation="authors"/>
            <field name="comments" type="relationship" relation="comments" many="true"/>
            <field name="title" type="string" length="256"/>
            <field name="body" type="string" length="2048"/>
            <field name="like_count" type="integer"/>
            <field name="comment_count" type="integer"/>
        </table>

        <table name="comments">
            <field name="post" type="relationship" relation="posts"/>
            <field name="author" type="relationship" relation="authors"/>
            <field name="text" type="string" length="1024"/>
            <field name="like_count" type="integer"/>
        </table>
    </database>
</databases>
```

## Incorrect Schema

```xml
<!-- WRONG: Missing length attribute for string -->
<field name="name" type="string"/>

<!-- WRONG: Unsupported field type -->
<field name="date" type="datetime"/>

<!-- WRONG: Relationship without relation attribute -->
<field name="author" type="relationship"/>

<!-- WRONG: Duplicate table names -->
<table name="items">
<table name="items">
```

## Field Types

| Type | Description | Required Attributes |
|------|-------------|---------------------|
| `string` | Text field | `length` (buffer size) |
| `integer` | Whole numbers | None |
| `bool` | Boolean values | None |
| `float` | Floating point | None |
| `relationship` | Foreign key | `relation`, optional `many` |

## Relationships

### One-to-Many
```xml
<table name="posts">
    <field name="author" type="relationship" relation="authors"/>
</table>
```

### Many-to-One (Inverse)
```xml
<table name="authors">
    <field name="posts" type="relationship" relation="posts" many="true"/>
</table>
```

## Database Class

Use `SimpleXMLDatabase` for XML persistence:
```xml
<database name="db" class="SimpleXMLDatabase" source="path/to/seed.xml">
```

## Seed Data Format

Create XML file with initial data:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<database>
    <authors>
        <author id="1" name="alice" avatar="alice.png" />
        <author id="2" name="bob" avatar="bob.png" />
    </authors>
    <posts>
        <post id="1" author_id="1" title="Hello" like_count="5">Body text</post>
    </posts>
</database>
```

## Generated Constants

From the schema, the compiler generates:
- `ID_DB_AUTHORS` — table ID
- `ID_DB_AUTHORS_ID` — field ID
- `ID_DB_AUTHORS_NAME` — field ID
- `authors_fields[]` — field metadata array

## Implementation

Create `db_simple_xml.c` with message handlers:
```c
lresult_t db_simple_xml(database_t *db, uint32_t msg, uint32_t wparam, void *lparam) {
    switch (msg) {
        case dbCreate:    // Allocate context
        case dbDestroy:   // Free context
        case dbLoad:      // Load from XML
        case dbSave:      // Save to XML
        case dbInsert:    // Insert record
        case dbUpdate:    // Update record
        case dbDelete:    // Delete record
        case dbFetch:     // Query records
        case dbFind:      // Find single record
    }
}
```

## Common Mistakes

1. **Forgetting `length` on string fields** — causes buffer overflows
2. **Using wrong relationship direction** — breaks foreign key constraints
3. **Duplicate table names** — compiler errors
4. **Missing seed data file** — runtime crashes
5. **Not registering database class** — `DB_CLASS()` macro required