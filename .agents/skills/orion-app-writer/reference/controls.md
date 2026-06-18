# Available Controls

## Basic Controls

### Button
```xml
<Button name="ok" text="OK" action="db.items.insert" flags="default" />
```

### Label
```xml
<Label name="lbl_name" text="Name:" />
```

### TextBox
```xml
<TextBox name="title" field="db.posts.title" />
```

### MultiEdit
```xml
<MultiEdit name="body" field="db.posts.body" />
```

### CheckBox
```xml
<CheckBox name="enable" text="Enable Feature" />
```

### ComboBox
```xml
<ComboBox name="author" 
          field="db.posts.author_id"
          source="db.authors"
          display="name"
          value="id" />
```

## Layout Controls

### StackView
```xml
<StackView name="content" spacing="0" flags="flexspace">
    <!-- Child controls -->
</StackView>

<!-- Horizontal stack -->
<StackView name="row" orientation="horizontal" spacing="6">
    <Label name="lbl" text="Label:" />
    <TextBox name="edit" />
</StackView>
```

### GridView
```xml
<GridView name="fields" spacing="4">
    <Column name="labels" width="56">
        <Label name="lbl_name" text="Name:" />
        <Label name="lbl_email" text="Email:" />
    </Column>
    <Column name="inputs">
        <TextBox name="name" />
        <TextBox name="email" />
    </Column>
</GridView>
```

### Space
```xml
<Space name="flex" /> <!-- Flexible space -->
<Space name="fixed" width="20" /> <!-- Fixed width space -->
```

### Separator
```xml
<Separator name="section_sep" />
```

## Data Controls

### TableView
```xml
<TableView name="items"
           source="db.items"
           action="fetch_items"
           flags="notitle,nofill,vscroll,flexspace">
    <Column field="name" title="Name" width="0" />
    <Column field="value" title="Value" width="80" />
</TableView>
```

### ListView
```xml
<ListView name="list" flags="vscroll,flexspace">
    <!-- Items added programmatically -->
</ListView>
```

## Container Controls

### Window (Form)
```xml
<form name="main_window"
      title="My App"
      width="400" height="300"
      flags="toolbar,statusbar"
      toolbar="main">
    <!-- Child controls -->
</form>
```

### Dialog
```xml
<form name="dialog"
      title="Dialog Title"
      width="300" height="200"
      padding="8">
    <!-- Child controls -->
</form>
```

## Control Attributes

### Common Attributes
- `name` — Control name (for lookup)
- `id` — Control ID (generated from .orion)
- `text` — Display text
- `flags` — Control flags
- `field` — Database binding (e.g., `db.table.field`)

### Layout Attributes
- `width` — Control width
- `height` — Control height
- `spacing` — Space between children
- `padding` — Internal padding
- `orientation` — horizontal/vertical

### Binding Attributes
- `field` — Database field binding
- `source` — Data source
- `display` — Display field
- `value` — Value field
- `action` — Action on click

## Control Flags

### Window Flags
- `WINDOW_NOTITLE` — No title bar
- `WINDOW_NORESIZE` — Not resizable
- `WINDOW_VSCROLL` — Vertical scrollbar
- `WINDOW_HSCROLL` — Horizontal scrollbar
- `WINDOW_DIALOG` — Dialog window
- `WINDOW_TOOLBAR` — Toolbar window
- `WINDOW_STATUSBAR` — Status bar

### StackView Flags
- `flags="flexspace"` — Flexible space
- `orientation="horizontal"` — Horizontal layout
- `orientation="vertical"` — Vertical layout

### TableView Flags
- `flags="notitle"` — No column titles
- `flags="nofill"` — Don't fill parent
- `flags="vscroll"` — Vertical scrollbar
- `flags="flexspace"` — Flexible space

## Control Messages

### Button Messages
- `btnClicked` — Button clicked
- `btnSetImage` — Set button icon

### TextBox Messages
- `edGetText` — Get text
- `edSetText` — Set text

### ComboBox Messages
- `cbAddString` — Add item
- `cbGetCurrentSelection` — Get selection
- `cbSetCurrentSelection` — Set selection

### TableView Messages
- `tvRefresh` — Refresh data
- `tvSetFilter` — Set filter
- `RVM_ADDCOLUMN` — Add column
- `RVM_ADDITEM` — Add item
- `RVM_CLEAR` — Clear items
- `RVM_GETITEMCOUNT` — Get item count
- `RVM_GETSELECTION` — Get selection
- `RVM_SETSELECTION` — Set selection

## Common Patterns

### Form with Database Binding
```xml
<form name="edit_item" width="300" height="150">
    <GridView name="fields" spacing="4">
        <Column name="labels" width="60">
            <Label name="lbl_name" text="Name:" />
            <Label name="lbl_value" text="Value:" />
        </Column>
        <Column name="inputs">
            <TextBox name="name" field="db.items.name" />
            <TextBox name="value" field="db.items.value" />
        </Column>
    </GridView>
    <Separator name="sep" />
    <StackView name="actions" orientation="horizontal" spacing="6">
        <Space name="flex" />
        <Button name="ok" text="OK" flags="default" />
        <Button name="cancel" text="Cancel" />
    </StackView>
</form>
```

### List with Columns
```xml
<TableView name="list"
           source="db.items"
           flags="notitle,vscroll,flexspace">
    <Column field="name" title="Name" width="0" />
    <Column field="value" title="Value" width="80" />
    <Column field="status" title="Status" width="60" />
</TableView>
```