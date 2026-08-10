---
name: orion-ui-authoring
description: "**UI AUTHORING SKILL** — Create, review, or debug Orion UI definitions and control behavior. Expert in .orion XML, WPF-style auto-layout, Apple HIG (1987/1995), control usage, scrolling, popup capture, and input routing. USE FOR: designing dialogs; converting imperative UI to declarative .orion; fixing layout, scrolling, scrollbar, dropdown, or popup-dismissal issues; reviewing .orion files for HIG compliance; explaining layout behavior. DO NOT USE FOR: unrelated framework internals or non-UI C implementation."
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

### Orion Auto-Layout System (WPF-Based)

Orion's layout is directly modeled on **WPF (Windows Presentation Foundation)**:
- Grid with star-sized columns — `<ColumnDefinition Width="*" />` in WPF = omit `width=` in Orion
- Fixed-width columns — `<ColumnDefinition Width="48" />` in WPF = `width="48"` in Orion
- StackPanel orientation — `<StackPanel Orientation="Horizontal" />` in WPF = `<stack orientation="horizontal">` in Orion
- Measure/Arrange pattern — controls report desired size, containers allocate space

**Grid Layout (Column-based, WPF Grid equivalent):**
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

**Built-in class defaults:**
- `space` -> always flex
- `reportview` -> always flex
- `multiedit` -> always flex
- `list` -> not flex by default
- `button`, `label`, `textedit`, `checkbox`, `combobox`, `separator` -> not flex by default
- Layout containers like `stack`, `flow`, and `grid` are not inherently flex just because they are containers; they become flex only when their own layout or descendants require it

**Propagation rule:**
- If a built-in flex class appears inside nested layout containers, the flex intent should bubble upward through matching container orientation
- A horizontal action row with a local `<space />` should not make an orthogonal parent stack claim extra vertical room

### Scrollable controls and captured popups

Before changing scrolling, scrollbars, mouse hit-testing, or popup capture, read
`ARCHITECTURE.md#window-and-input-event-routing`.

- Mouse coordinates delivered to a window procedure are in content space and already include that window's scroll offset. Do not add the offset again for row hit-testing.
- A top-level scrollable popup's paint projection already applies its own scroll offset. Draw rows at content coordinates (`row * row_height`); do not also subtract `vscroll.pos`, or the content scrolls twice.
- Built-in scrollbars are fixed non-client chrome. When painting a root window's scrollbar, exclude that root's own scroll offset from the scrollbar projection. A child scrollbar may still inherit its root ancestor's scroll because the child itself moves with the root content.
- Capture sends mouse events to the popup even when the pointer is outside it. Convert delivered content coordinates back to viewport coordinates by subtracting the popup's scroll offsets before bounds-testing.
- On an outside captured click, restore the pre-open value, release capture by destroying/dismissing the popup, return focus to the owner, and send no selection-change notification.
- Do not close the popup on a scrollbar mouse-up. Commit only when a list-item mouse-down established an active item press and mouse-up remains inside the popup.

For a dropdown regression, test wheel scrolling, fixed scrollbar position, keyboard selection visibility, scrollbar mouse-up, outside-click cancellation, and restoration of the original selection.

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

### Form Height Rules

**Fixed-content forms** — omit `height=`, specify `width="X"` only:
- Forms with only fixed-size controls (labels, buttons, textedit, checkboxes, separators, sliders)
- Forms where `<space>` elements only expand **horizontally** (in horizontal stacks)
- Height auto-calculates from child measurements
- Example: `<form name="new_image" width="180">` (no height attribute)

**Flex-content forms** — specify both `width="X" height="Y"`:
- `<multiedit flags="WINDOW_FLEXSPACE">` — text editor that expands/shrinks **vertically**
- `<reportview flags="WINDOW_FLEXSPACE">` — scrolling list that expands/shrinks **vertically**
- `<grid flags="WINDOW_FLEXSPACE">` — grid that expands/shrinks **vertically**
- Framework divides the specified height among flex children
- Example: `<form name="new_post" width="272" height="250">` (multiedit needs vertical space)

**Key insight:** Horizontal `<space>` elements (used to push buttons left/right) do NOT require explicit form height. They expand horizontally but don't affect vertical height calculation.

### Common Dialog Patterns

**Label+Input Dialog:**
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

**Scrolling List Dialog:**
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

## Critical mistakes to avoid

❌ Using `columns="N"` attribute (removed - use `<column>` elements)  
❌ Adding `WINDOW_FLEXSPACE` to controls inside grid columns  
❌ Adding `WINDOW_FLEXSPACE` to `<column>` elements  
❌ Forgetting `WINDOW_FLEXSPACE` on grid when embedding in stack  
❌ Using `frame=` on auto-layout children  
❌ Label column widths too small (causes "10px wide column" bug)  
❌ Subtracting `vscroll.pos` in a top-level popup renderer when its projection already scrolls content
❌ Painting root-window scrollbar chrome with the root's scrolling projection
❌ Treating every captured mouse-up as an item commit

✅ Always use explicit `<column>` elements for grids  
✅ Fixed width on label columns (48-80pt)  
✅ `WINDOW_FLEXSPACE` on grid itself for expansion  
✅ `WINDOW_VSCROLL` on scrollable controls (reportview, multiedit)  
✅ `<space />` to push button groups to edges  
✅ Content-space hit-testing and viewport-space popup bounds checks

## Available controls

- `<button>` - Action button (use `flags="BUTTON_DEFAULT"` for default)
- `<label>` - Static text (use `color="text-disabled"` for hints)
- `<textedit>` - Single-line input
- `<multiedit>` - Multi-line input (needs `WINDOW_VSCROLL`)
- `<checkbox>` - Binary toggle
- `<combobox>` - Dropdown list
- `<reportview>` - List/table (needs `WINDOW_VSCROLL`)
- `<separator>` - Horizontal divider (**no expansion**)
- `<space>` - Flexible spacer (**expands** along stack axis)
- `<stack>` - Linear layout container
- `<grid>` - Multi-column layout (requires `<column>` children)
- `<column>` - Grid column (vertical stack)

**CRITICAL: <space> vs <separator> usage:**
- `<space />` **expands infinitely** along the stack axis — dangerous in fixed-height forms
- `<separator />` just draws a line **without expansion** — safe for visual division
- In **fixed-height dialogs**, always use `<separator>` before action buttons
- Using `<space>` before buttons in fixed-height forms pushes them off-screen
- Use `<space>` only in horizontal stacks (push buttons right) or flex-height layouts

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

## Documentation references

- `AGENTS.md` — complete Orion .orion authoring reference (grid, stack, flex, HIG, controls, common patterns)
- `docs/controls.md` — Control reference
- `docs/dialogs.md` — Dialog patterns
- `examples/*/**.orion` — Real-world examples

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
