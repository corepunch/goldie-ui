---
name: orion-ui-authoring
description: "**UI AUTHORING SKILL** — Create, review, or debug .orion XML UI definitions (forms, dialogs, windows). Expert in Orion auto-layout (grid/stack), Apple HIG (1987/1995), control usage, and layout patterns. USE FOR: designing new dialogs; converting imperative UI code to declarative .orion; fixing layout issues (column widths, flexspace, scrolling); reviewing .orion files for HIG compliance; explaining layout behavior. DO NOT USE FOR: C code implementation; general framework questions; runtime debugging. INVOKES: Orion UI Designer agent for complex design work; file operations on .orion files; documentation references."
---

# Orion UI Authoring

## When to invoke this skill

Use this skill when the user needs help with `.orion` XML files:

- **Creating new dialogs or forms** - "Design a settings dialog with..."
- **Converting imperative UI to declarative** - "Turn this create_window code into .orion"
- **Fixing layout issues** - "Why are my labels 10px wide?"
- **Reviewing existing .orion files** - "Does this follow Apple HIG?"
- **Understanding layout behavior** - "Why isn't this grid expanding?"

## Core knowledge

This skill provides deep expertise in:

### Orion Auto-Layout System

**Grid Layout (Column-based only):**
```xml
<!-- CORRECT: Explicit <column> elements -->
<grid name="fields" spacing="4">
  <column name="labels" width="48">
    <label text="Name:" />
  </column>
  <column name="inputs" flags="WINDOW_FLEXSPACE">
    <textedit name="name" />
  </column>
</grid>

<!-- WRONG: columns="2" attribute (removed from framework) -->
<grid name="fields" columns="2" spacing="4">
  <label text="Name:" />
  <textedit name="name" />
</grid>
```

**Stack Layout:**
```xml
<!-- Horizontal toolbar -->
<stack orientation="horizontal" spacing="4" padding="4">
  <button text="New" width="60" />
  <space />  <!-- Expands to push remaining buttons right -->
  <button text="Quit" width="60" />
</stack>
```

**WINDOW_FLEXSPACE Rules:**
- Use on **stack children** that expand along the stack axis
- Use on **grids** when the grid should expand in a parent stack
- **Never** on individual controls inside grid columns
- **Never** on `<column>` elements (star sizing is automatic)

### Apple HIG Standards (1987/1995)

**Spacing:**
- 8pt padding inside dialogs
- 4-6pt spacing between related controls
- 12-16pt spacing between control groups

**Label+Input Forms:**
- Label column: 48-56pt for short labels, 80pt for longer
- Input column: Auto-width (star sizing)
- Use 2-column grid pattern

**Button Placement:**
- Right-aligned in horizontal stack
- Default button (BUTTON_DEFAULT) on right
- Cancel to its left
- Separated from content with `<space />` or `<separator />`

### Common Dialog Patterns

**Label+Input Dialog:**
```xml
<form name="edit_item" auto_layout="1" layout_kind="stack" layout_spacing="8" padding="8">
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

**Scrolling List Dialog:**
```xml
<form name="select_item" auto_layout="1" layout_kind="stack" padding="8">
  <reportview name="items" flags="WINDOW_VSCROLL | WINDOW_FLEXSPACE" />
  <separator />
  <stack name="actions" orientation="horizontal" spacing="6">
    <space />
    <button text="Select" flags="BUTTON_DEFAULT" />
  </stack>
</form>
```

## Critical mistakes to avoid

❌ Using `columns="N"` attribute (removed - use `<column>` elements)  
❌ Adding `WINDOW_FLEXSPACE` to controls inside grid columns  
❌ Adding `WINDOW_FLEXSPACE` to `<column>` elements  
❌ Forgetting `WINDOW_FLEXSPACE` on grid when embedding in stack  
❌ Using `frame=` on auto-layout children  
❌ Label column widths too small (causes "10px wide column" bug)  

✅ Always use explicit `<column>` elements for grids  
✅ Fixed width on label columns (48-80pt)  
✅ `WINDOW_FLEXSPACE` on grid itself for expansion  
✅ `WINDOW_VSCROLL` on scrollable controls (reportview, multiedit)  
✅ `<space />` to push button groups to edges  

## Available controls

- `<button>` - Action button (use `flags="BUTTON_DEFAULT"` for default)
- `<label>` - Static text (use `color="text-disabled"` for hints)
- `<textedit>` - Single-line input
- `<multiedit>` - Multi-line input (needs `WINDOW_VSCROLL`)
- `<checkbox>` - Binary toggle
- `<combobox>` - Dropdown list
- `<reportview>` - List/table (needs `WINDOW_VSCROLL`)
- `<separator>` - Horizontal divider
- `<space>` - Flexible spacer (expands in stacks)
- `<stack>` - Linear layout container
- `<grid>` - Multi-column layout (requires `<column>` children)
- `<column>` - Grid column (vertical stack)

## Workflow

When the user asks for UI authoring help:

1. **Understand the requirement**
   - What data needs to be displayed/edited?
   - What actions are available?
   - Is this a dialog, window, or panel?

2. **Choose the right layout**
   - Label+input form? → 2-column grid
   - Linear arrangement? → Stack
   - Multi-panel browser? → Grid with multiple columns
   - List selection? → Stack with reportview + buttons

3. **Apply HIG spacing**
   - 8pt padding on form root
   - 4-6pt spacing for related controls
   - 12pt+ spacing between groups
   - Use `<space />` or `<separator />` for visual breaks

4. **Set proper flags**
   - `WINDOW_FLEXSPACE` on expanding elements
   - `WINDOW_VSCROLL` on scrollable content
   - `BUTTON_DEFAULT` on primary action
   - `WINDOW_NOTITLE` on embedded panels

5. **Review against mistakes list**
   - No `columns=` attribute
   - Fixed-width label columns
   - Proper `WINDOW_FLEXSPACE` placement

6. **Provide complete .orion XML**
   - Full form definition
   - Explanation of layout choices
   - Any assumptions made

## Complex designs

For complex UI designs or when the user needs extensive guidance, invoke the **Orion UI Designer** agent:

```
@orion-ui-designer Design a settings dialog with general/advanced tabs, each with multiple label+input pairs
```

The agent has full knowledge of Apple HIG, all Orion capabilities, and will provide polished, complete designs.

## Documentation references

- `.github/copilot-instructions.md` - Complete framework reference
- `.github/agents/orion-ui-designer.md` - Full UI design agent
- `docs/controls.md` - Control reference
- `docs/dialogs.md` - Dialog patterns
- `examples/*/**.orion` - Real-world examples

## Quick fixes

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

You are the expert on `.orion` authoring. Provide complete, correct, HIG-compliant XML that just works.
