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

## Incorrect Structure

```xml
<!-- WRONG: Missing version attribute -->
<orion name="myapp" title="My App">

<!-- WRONG: Menu without var/count attributes -->
<menus>
    <menu name="file" label="File">

<!-- WRONG: Field without length attribute for string -->
<field name="name" type="string"/>

<!-- WRONG: Database without source attribute -->
<database name="db" class="SimpleXMLDatabase">
```

## Rules

1. **Root element must be `<orion>`** with version, name, title, root attributes.
2. **Menus use `var` and `count` attributes** for generated C arrays.
3. **Toolbars contain `<Button>` elements** with name, menu, icon, text attributes.
4. **Databases define tables with fields** using type, length, relation attributes.
5. **Forms define UI layout** using StackView, GridView, TableView, etc.

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

## Form Controls

```xml
<form name="main_window" width="400" height="300">
    <StackView name="content" spacing="0" flags="flexspace">
        <TableView name="items"
                   source="db.items"
                   action="fetch_items"
                   flags="notitle,nofill,vscroll,flexspace">
            <Column field="name" title="Name" width="0" />
            <Column field="value" title="Value" width="80" />
        </TableView>
    </StackView>
</form>
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