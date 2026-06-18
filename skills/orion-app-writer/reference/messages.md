# Window and Database Message Reference

## Window Messages

### Lifecycle Messages

| Message | Description | wparam | lparam | Return |
|---------|-------------|--------|--------|--------|
| `evCreate` | Window created | 0 | State pointer | true on success |
| `evDestroy` | Window destroyed | 0 | NULL | true to prevent destruction |
| `evClose` | Window close requested | 0 | NULL | true to prevent closing |

### Paint Messages

| Message | Description | wparam | lparam | Return |
|---------|-------------|--------|--------|--------|
| `evPaint` | Window needs redraw | 0 | NULL | false to let framework draw |

### Input Messages

| Message | Description | wparam | lparam | Return |
|---------|-------------|--------|--------|--------|
| `evLeftButtonDown` | Left mouse button pressed | key flags | MAKELONG(x, y) | true if handled |
| `evLeftButtonUp` | Left mouse button released | key flags | MAKELONG(x, y) | true if handled |
| `evRightButtonDown` | Right mouse button pressed | key flags | MAKELONG(x, y) | true if handled |
| `evRightButtonUp` | Right mouse button released | key flags | MAKELONG(x, y) | true if handled |
| `evMouseMove` | Mouse moved | key flags | MAKELONG(x, y) | true if handled |
| `evKeyDown` | Key pressed | virtual key code | 0 | true if handled |
| `evKeyUp` | Key released | virtual key code | 0 | true if handled |

### Notification Messages

| Message | Description | wparam | lparam | Return |
|---------|-------------|--------|--------|--------|
| `evCommand` | Control notification | HIWORD=notification, LOWORD=id | Control pointer | true if handled |
| `evResize` | Window resized | 0 | NULL | false to let framework handle |

### Scroll Messages

| Message | Description | wparam | lparam | Return |
|---------|-------------|--------|--------|--------|
| `evVScroll` | Vertical scroll | scroll bar code | Scroll position | true if handled |
| `evHScroll` | Horizontal scroll | scroll bar code | Scroll position | true if handled |

## Control Notifications

### Button Notifications

| Notification | Description | wparam | lparam |
|--------------|-------------|--------|--------|
| `btnClicked` | Button clicked | Button ID | Button pointer |

### List/TableView Notifications

| Notification | Description | wparam | lparam |
|--------------|-------------|--------|--------|
| `RVN_SELCHANGE` | Selection changed | Row index | List pointer |
| `RVN_DBLCLK` | Double-click | Row index | List pointer |
| `RVN_DELETE` | Delete requested | Row index | List pointer |

### Edit Notifications

| Notification | Description | wparam | lparam |
|--------------|-------------|--------|--------|
| `edUpdate` | Text modified | Edit ID | Edit pointer |

### Combobox Notifications

| Notification | Description | wparam | lparam |
|--------------|-------------|--------|--------|
| `cbSelectionChange` | Selection changed | Combo ID | Combo pointer |

## Database Messages

### Lifecycle Messages

| Message | Description | wparam | lparam | Return |
|---------|-------------|--------|--------|--------|
| `dbCreate` | Database created | 0 | Source path | 1 on success |
| `dbDestroy` | Database destroyed | 0 | NULL | 1 on success |
| `dbLoad` | Load from source | 0 | NULL | 1 on success |
| `dbSave` | Save to source | 0 | NULL | 1 on success |

### CRUD Messages

| Message | Description | wparam | lparam | Return |
|---------|-------------|--------|--------|--------|
| `dbInsert` | Insert record | Table ID | Record data | Record pointer |
| `dbUpdate` | Update record | Table ID | Record pointer | 1 on success |
| `dbDelete` | Delete record | Table ID | Record ID | 1 on success |

### Query Messages

| Message | Description | wparam | lparam | Return |
|---------|-------------|--------|--------|--------|
| `dbFetch` | Fetch records | MAKEDWORD(table, filter_field) | Filter value | Result list |
| `dbFind` | Find single record | MAKEDWORD(table, search_field) | Search value | Record pointer |

### Metadata Messages

| Message | Description | wparam | lparam | Return |
|---------|-------------|--------|--------|--------|
| `dbGetDirty` | Check if dirty | 0 | NULL | 0 or 1 |
| `dbGetSchema` | Get schema | 0 | 0 | Schema pointer |
| `dbGetFieldMeta` | Get field metadata | Table ID | Count out | Metadata pointer |
| `dbGetApi` | Get API definition | 0 | 0 | API pointer |

## Common Message Patterns

### Handling Button Clicks

```c
case evCommand:
    if (HIWORD(wparam) == btnClicked) {
        window_t *source = (window_t *)lparam;
        switch (source->id) {
            case ID_MY_BUTTON:
                // Handle button click
                return true;
        }
    }
    return false;
```

### Handling List Selection

```c
case evCommand:
    if (HIWORD(wparam) == RVN_SELCHANGE) {
        int index = (int)(int16_t)LOWORD(wparam);
        // Handle selection change
        return true;
    }
    return false;
```

### Database Insert

```c
item_t item = { .name = "New Item" };
item_t *inserted = (item_t *)send_db_message(db, dbInsert, ID_DB_ITEMS, &item);
```

### Database Find

```c
item_t *found = (item_t *)send_db_message(db, dbFind,
    MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)item_id);
```

### Database Fetch

```c
result_node_t *items = (result_node_t *)send_db_message(db, dbFetch,
    MAKEDWORD(ID_DB_ITEMS, 0), (void *)(intptr_t)0);
int count = count_result_list(items);
free_result_list(items);
```