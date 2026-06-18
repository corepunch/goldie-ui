# .orion File Structure

The `.orion` file is the app's blueprint. It defines menus, toolbars, databases, and forms in XML format.

## Correct Structure

```xml
<?xml version="1.0" encoding="UTF-8"?>
<orion
    version="1"
    name="myapp"
    title="My Application"
    root="examples/myapp">

    <menus var="kMenus" count="kNumMenus">
        <menu name="file" label="File">
            <item name="quit" label="Quit" />
        </menu>
    </menus>

    <toolbars>
        <toolbar name="main">
            <Button name="new" menu="file" icon="sysicon_add" text="New" />
        </toolbar>
    </toolbars>

    <databases>
        <database name="db" class="SimpleXMLDatabase" source="examples/myapp/share/seed.xml">
            <table name="items">
                <field name="name" type="string" length="64"/>
                <field name="value" type="integer"/>
            </table>
        </database>
    </databases>

    <forms>
        <form name="main_window"
              title="My App"
              width="400" height="300"
              flags="toolbar,statusbar"
              toolbar="main">
            <!-- Controls go here -->
        </form>
    </forms>
</orion>
```

## Rules

1. **Root element must be `<orion>`** with version, name, title, root attributes.
2. **Menus use `var` and `count` attributes** for generated C arrays.
3. **Toolbars contain `<Button>` elements** with name, menu, icon, text attributes.
4. **Databases define tables with fields** using type, length, relation attributes.
5. **Forms define UI layout** using stack, grid, reportview, etc.

## Auto-Layout System (WPF-Based)

Orion's layout is modeled on **WPF (Windows Presentation Foundation)**:
- Grid with star-sized columns — omit `width=` for auto-sizing
- Fixed-width columns — `width="48"` for fixed size
- StackPanel orientation — `orientation="horizontal"` or `"vertical"`
- Measure/Arrange pattern — controls report desired size, containers allocate space

### Grid Layout (Column-based)

```xml
<!-- CORRECT: Explicit <column> elements -->
<grid name="fields" spacing="4">
  <column name="labels" width="48">
    <label text="Name:" />
    <label text="Email:" />
  </column>
  <column name="inputs" flags="WINDOW_FLEXSPACE">
    <textedit name="name" />
    <textedit name="email" />
  </column>
</grid>

<!-- WRONG: columns="2" attribute (removed from framework) -->
<grid name="fields" columns="2" spacing="4">
  <label text="Name:" />
  <textedit name="name" />
</grid>
```

### Stack Layout

```xml
<!-- Horizontal toolbar -->
<stack orientation="horizontal" spacing="4" padding="4">
  <button text="New" width="60" />
  <space />  <!-- Expands to push remaining buttons right -->
  <button text="Quit" width="60" />
</stack>

<!-- Vertical sidebar -->
<stack orientation="vertical" spacing="4">
  <label text="Section 1" />
  <label text="Section 2" />
</stack>
```

### WINDOW_FLEXSPACE Rules

- Use on **stack children** that expand along the stack axis
- Use on **grids** when the grid should expand in a parent stack
- **Never** on individual controls inside grid columns
- **Never** on `<column>` elements (star sizing is automatic)

### Built-in Class Defaults

| Class | Flex | Notes |
|-------|------|-------|
| `space` | always flex | Expands infinitely along stack axis |
| `reportview` | always flex | Needs `WINDOW_VSCROLL` |
| `multiedit` | always flex | Needs `WINDOW_VSCROLL` |
| `list` | not flex | Needs explicit size |
| `button`, `label`, `textedit` | not flex | Fixed size |
| `checkbox`, `combobox` | not flex | Fixed size |
| `separator` | no expansion | Just draws a line |
| `stack`, `flow`, `grid` | not inherently flex | Only flex when flagged |

### Propagation Rule

If a built-in flex class appears inside nested layout containers, the flex intent should bubble upward through matching container orientation. A horizontal action row with a local `<space />` should not make an orthogonal parent stack claim extra vertical room.

## Apple HIG Standards (1987/1995)

### Spacing

- **8pt padding** inside dialogs (on form root)
- **4-6pt spacing** between related controls
- **12-16pt spacing** between control groups

### Label+Input Forms

- Label column: **48-56pt** for short labels, **80pt** for longer
- Input column: Auto-width (star sizing)
- Use 2-column grid pattern

### Button Placement

- Right-aligned in horizontal stack
- Default button (`BUTTON_DEFAULT`) on right
- Cancel to its left
- Separated from content with `<space />` or `<separator />`

## Form Height Rules

### Fixed-content forms — omit `height=`, specify `width="X"` only:

```xml
<!-- Forms with only fixed-size controls (labels, buttons, textedit, checkboxes) -->
<form name="new_image" width="180">
  <!-- Height auto-calculates from child measurements -->
</form>
```

### Flex-content forms — specify both `width="X" height="Y"`:

```xml
<!-- Forms with multiedit, reportview, or grid that expand vertically -->
<form name="new_post" width="272" height="250">
  <multiedit flags="WINDOW_FLEXSPACE" />  <!-- Needs vertical space -->
</form>
```

**Key insight:** Horizontal `<space>` elements (used to push buttons left/right) do NOT require explicit form height. They expand horizontally but don't affect vertical height calculation.

## Common Dialog Patterns

### Label+Input Dialog

```xml
<form name="edit_item" auto_layout="1" spacing="8" padding="8">
  <grid name="fields" spacing="4">
    <column name="labels" width="48">
      <label text="Name:" />
      <label text="Email:" />
    </column>
    <column name="inputs" flags="WINDOW_FLEXSPACE">
      <textedit name="name" value="1" />
      <textedit name="email" value="2" />
    </column>
  </grid>
  <space />
  <stack name="actions" orientation="horizontal" spacing="6">
    <space />
    <button name="ok" value="100" text="OK" flags="BUTTON_DEFAULT" />
    <button name="cancel" value="101" text="Cancel" />
  </stack>
</form>
```

### Scrolling List Dialog

```xml
<form name="select_item" auto_layout="1" padding="8">
  <reportview name="items" flags="WINDOW_VSCROLL | WINDOW_FLEXSPACE" />
  <separator />
  <stack name="actions" orientation="horizontal" spacing="6">
    <space />
    <button text="Select" flags="BUTTON_DEFAULT" />
  </stack>
</form>
```

### Main Window with Toolbar

```xml
<form name="main_window"
      title="My App"
      width="800" height="600"
      flags="toolbar,statusbar"
      toolbar="main">
  <stack orientation="horizontal" spacing="0" flags="flexspace">
    <!-- Sidebar -->
    <stack name="sidebar" spacing="0" flags="flexspace" width="200">
      <reportview name="list" flags="vscroll,flexspace" />
    </stack>
    <!-- Content -->
    <stack name="content" spacing="0" flags="flexspace">
      <reportview name="details" flags="vscroll,flexspace" />
    </stack>
  </stack>
</form>
```

## Available Controls

| Control | Description | Common Flags |
|---------|-------------|--------------|
| `<button>` | Action button | `BUTTON_DEFAULT` for primary |
| `<label>` | Static text | `color="text-disabled"` for hints |
| `<textedit>` | Single-line input | `WINDOW_FLEXSPACE` to expand |
| `<multiedit>` | Multi-line input | `WINDOW_VSCROLL` required |
| `<checkbox>` | Binary toggle | — |
| `<combobox>` | Dropdown list | `WINDOW_FLEXSPACE` to expand |
| `<reportview>` | List/table | `WINDOW_VSCROLL` required |
| `<separator>` | Horizontal divider | No expansion |
| `<space>` | Flexible spacer | Expands along stack axis |
| `<stack>` | Linear layout | `orientation="horizontal/vertical"` |
| `<grid>` | Multi-column layout | Requires `<column>` children |
| `<column>` | Grid column | `width="48"` for fixed, omit for auto |

## Critical Mistakes to Avoid

❌ Using `columns="N"` attribute (removed — use `<column>` elements)  
❌ Adding `WINDOW_FLEXSPACE` to controls inside grid columns  
❌ Adding `WINDOW_FLEXSPACE` to `<column>` elements  
❌ Forgetting `WINDOW_FLEXSPACE` on grid when embedding in stack  
❌ Using `frame=` on auto-layout children  
❌ Label column widths too small (causes "10px wide column" bug)  
❌ Using `<space>` before buttons in fixed-height forms (pushes them off-screen)

✅ Always use explicit `<column>` elements for grids  
✅ Fixed width on label columns (48-80pt)  
✅ `WINDOW_FLEXSPACE` on grid itself for expansion  
✅ `WINDOW_VSCROLL` on scrollable controls (reportview, multiedit)  
✅ `<space />` to push button groups to edges (horizontal stacks only)  
✅ `<separator />` before buttons in fixed-height forms

## `<space>` vs `<separator>` Usage

- `<space />` **expands infinitely** along the stack axis — dangerous in fixed-height forms
- `<separator />` just draws a line **without expansion** — safe for visual division
- In **fixed-height dialogs**, always use `<separator>` before action buttons
- Using `<space>` before buttons in fixed-height forms pushes them off-screen
- Use `<space>` only in horizontal stacks (push buttons right) or flex-height layouts

## Quick Fixes

**"Labels are 10px wide!"**
→ Add explicit `<column width="48">` for label column

**"Grid doesn't expand!"**
→ Add `flags="WINDOW_FLEXSPACE"` to the grid element itself

**"List doesn't scroll!"**
→ Add `WINDOW_VSCROLL` to the reportview/multiedit

**"Buttons stuck to content!"**
→ Add `<space />` before button stack

**"Getting 'columns not found' error!"**
→ Remove `columns="2"` and use `<column>` elements

**"Buttons pushed off-screen!"**
→ Replace `<space />` with `<separator />` before button stack

## Menu Items

```xml
<menu name="file" label="File">
    <item name="quit" label="Quit" />
    <Separator />
    <item name="open" label="Open..." />
</menu>
```

## Toolbar Buttons

```xml
<toolbar name="main">
    <Button name="new" menu="file" icon="sysicon_add" text="New" />
    <Button name="save" menu="file" icon="sysicon_save" text="Save" />
    <spacer />
    <Button name="delete" menu="edit" icon="sysicon_delete" text="Delete" />
</toolbar>
```

## Generated Constants

The .orion file generates C constants like:
- `ID_FILE_QUIT` — menu item IDs
- `ID_MAIN_WINDOW_FEED` — control IDs
- `ID_DB_ITEMS` — table IDs
- `kMenus` — menu definitions array
- `kNumMenus` — menu count

## File Location

Place `.orion` files in the app's root directory:
```
examples/myapp/myapp.orion
```