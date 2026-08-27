# Orion Direction

## What we learned

Orion is becoming a declarative, plugin-capable desktop UI system built around
WinAPI-style windows and messages.

The strongest parts of the current design already point in the same direction:

- `.orion` describes databases, forms, layouts, menus, toolbars, shortcuts, and
  data bindings.
- Menus are the canonical action manifest. Toolbars, context menus,
  accelerators, tests, and future command palettes reference the same command
  IDs.
- Forms create real window trees. They are not templates for application code
  to rebuild imperatively.
- Database adaptors give views a uniform query surface without coupling controls
  to a particular backend.
- Gems provide dynamic loading and menu contributions.
- The gitclient `pages/` refactor shows that large applications naturally divide
  into independently owned page windows.

The missing concept is not another kind of control. It is a first-class **page
contribution model** connecting forms, commands, toolbars, and plugins.

## Core direction: pages are windows

A tab page is a window/form in its own right. It owns its child controls, local
state, database bindings, event handling, and contextual commands. The tab host
selects and presents page windows; it does not implement their behavior.

```text
host window
  tab view
    changes page window
    history page window
    GitHub page window
```

A page is therefore useful outside a tab as well. The same page may be hosted in
a tab view, split view, document area, floating window, or plugin-provided
workspace. "Page" describes a contribution and lifecycle, not a new low-level
window class.

## Toolbar projection

The host owns the physical toolbar window and non-client layout. The selected
page publishes a declarative toolbar definition, and the host projects that
definition into its toolbar.

The page does **not** reparent or lend a live toolbar window to the host. It
provides data: toolbar items referencing canonical command IDs. This preserves
clear ownership and avoids duplicated input routing, stale window state, and
layout coupling.

```text
selected page
    |
    | toolbar definition
    v
host toolbar ---- command ID ----> action dispatcher ----> active page/handler
```

The host may compose two sections:

1. A persistent application toolbar for truly global actions, such as opening a
   repository, fetching, syncing, and refreshing.
2. A projected page toolbar for contextual actions, such as staging files,
   creating a commit, navigating history, or operating on an issue.

Changing tabs replaces only the projected section. Universal actions should not
be repeated in every page declaration.

A natural `.orion` shape is:

```xml
<toolbar name="application">
  <Button command="file.repositories" icon="git-repos" text="Repos" />
  <Button command="remote.fetch" icon="git-fetch" text="Fetch" />
</toolbar>

<toolbar name="changes_tools">
  <Button command="files.stage_all" icon="arrow-up" text="Stage All" />
  <Button command="files.unstage_all" icon="arrow-down" text="Unstage All" />
  <Button command="commit.commit" icon="git-commit" text="Commit" />
</toolbar>

<form name="changes_page" toolbar="changes_tools">
  ...
</form>
```

## Ownership

### Host owns

- the page container or `TabView`;
- the physical toolbar and status bar;
- application-wide menu presentation;
- the active-page reference;
- composition of application and page toolbar definitions;
- forwarding unhandled commands and notifications to the active page.

### Page owns

- its declarative form and child window tree;
- its outlets and transient UI state;
- page-specific refresh and selection state;
- its contextual toolbar declaration;
- contextual command handling;
- activation and deactivation behavior.

### `.orion` owns

- action identity and metadata;
- menu, toolbar, context-menu, and shortcut projections;
- forms and layout;
- database schemas and bindings;
- plugin and page contribution declarations.

### Plugin owns

- one or more page contributions;
- page forms and window procedures;
- required component libraries and data sources;
- menu and toolbar contributions;
- command handlers for the capabilities it provides.

## Page lifecycle

Pages should use normal window messages rather than a parallel C function-table
UI API. The minimal lifecycle is:

- `evCreate`: capture outlets and initialize page-local state;
- `evActivate`: refresh stale data, publish context, and establish focus;
- `evDeactivate`: finish transient edits and dismiss page-owned popups;
- `evCommand`: handle commands and child notifications owned by the page;
- `evDestroy`: release page-owned resources.

Toolbar discovery should preferably come from generated form metadata. If a
runtime query is required, it should be a message returning the page's toolbar
definition, not a pointer to a live toolbar control.

The host should forward only commands that remain unhandled after application-
level dispatch. This allows global commands to stay global while contextual
commands naturally follow the selected page. The current gitclient switch over
known tab indices is an intermediate implementation, not the final registry.

## Command model

The menu tree remains the application capability map. A user action has one
stable identity regardless of how it is invoked:

```text
menu / toolbar / context menu / accelerator / command palette / test
                              |
                              v
                       canonical command ID
                              |
                              v
                     one action dispatch path
```

A toolbar is therefore only a projection of commands. It must never define a
second action namespace or contain business logic specific to toolbar clicks.

The next refinement is contextual action state. A small context system can
expose values such as:

- active page;
- repository availability and dirty state;
- current selection type and count;
- active document or data source;
- operation in progress.

Declarative predicates such as `when="page == changes && repo.dirty"` can then
control visibility and enablement consistently across toolbars, menus, context
menus, and a command palette. This follows the useful parts of VS Code context
keys, Eclipse handler activation, and JetBrains action updates without copying
their larger frameworks.

Page registration and toolbar projection should come first. Context predicates
are a refinement after page ownership is established.

## Plugin direction

The target is that a host does not need source changes to add a page. A plugin
contributes a descriptor containing or resolving:

- stable page identity;
- label and icon;
- page form;
- page window procedure or registered class;
- toolbar definition;
- menu/action contributions;
- required databases and component libraries;
- ordering or preferred host slot.

The host registers the contribution, creates its page window, adds it to the
page container, and projects its toolbar whenever selected.

This extends the existing gem model rather than replacing it. Gems already
contribute menus and command handlers; pages and toolbar definitions are the
next contribution types. Plugin-provided UI remains WinAPI-style: registered
classes, window creation, messages, and notifications rather than bespoke
control function tables.

## References worth borrowing from

- **VS Code:** named contribution locations, view-title actions, context keys,
  command palette, and one command identity across surfaces.
- **Eclipse RCP:** commands separated from context-sensitive handlers and
  plugin-contributed views/perspectives.
- **Qt Creator:** modes that own their central page and contextual toolbar.
- **JetBrains Platform:** action groups projected into named UI locations and
  updated from current context.
- **GNOME Builder/libadwaita:** page-local header controls rather than one large
  global toolbar.

Orion should borrow the small compositional ideas, not their object models or
extension complexity.

## What not to do

- Do not make the host know every page by enum or tab index.
- Do not store page-specific control handles in generic host state.
- Do not reparent live toolbar windows when selection changes.
- Do not create separate command IDs for menu, toolbar, and shortcut variants.
- Do not require plugins to imperatively patch host toolbars.
- Do not add a second configuration model beside `.orion` for page metadata.
- Do not turn pages into C function tables when windows and messages already
  express their lifecycle and behavior.
- Do not hide universal actions in every page merely because toolbars are
  contextual.

## Incremental roadmap

### 1. Page-owned toolbar metadata

Allow a form to reference a toolbar definition. Preserve that reference in the
generated `form_def_t` metadata and expose it through the created page window.

**Initial implementation complete:** forms accept `role="host|page"`; generated
form metadata carries the role and page windows retain their declarative toolbar
items.

### 2. Toolbar composition

Extend the existing host toolbar implementation to compose persistent host
items with projected active-page items. Reuse the current toolbar state and
item messages; do not create a parallel toolbar control.

**Projection implemented:** `set_host_page()` replaces the host's toolbar with
the selected page's toolbar and manages activation, deactivation, rehosting, and
destruction cleanup. Persistent-plus-contextual composition remains future work.

### 3. Generic page activation

Teach `TabView` or a small page host abstraction to track the active page and
send activation/deactivation messages. Replace application switches such as
`if (active_tab == ...)` with active-window command forwarding.

**Foundation implemented:** gitclient stores its page windows and activates the
selected page through `set_host_page()`. Automatic `TabView` integration and
generic command forwarding remain future work.

### 4. Page registry

Generate or register page descriptors so hosts enumerate contributions instead
of hardcoding form creation for Changes, History, GitHub, and future pages.

### 5. Gem contributions

Extend the gem contribution surface with page descriptors and toolbar
resources. Keep command IDs and menus merged through the existing action model.

### 6. Context and command palette

Add action enablement/visibility predicates and build a command palette over
the already generated action metadata.

## Gitclient as the proving ground

Gitclient is the right application to validate this direction:

- Changes, History, and GitHub are already separate page forms and procedures.
- The main form already provides tab slots for those pages.
- Menus already define canonical actions consumed by toolbars and context menus.
- GitHub already uses a separate database adaptor.
- The current global toolbar visibly demonstrates which actions are universal
  and which belong to a selected page.

The first experiment should move Stage All, Unstage All, and Commit into a
Changes page toolbar while keeping repository, Fetch, Sync, Repositories, and
Refresh in the host toolbar. History and GitHub can then contribute their own
small toolbars. Success means adding a new page requires only registration and
its own `.orion` declarations, with no new host command-routing branch.

## Destination

Orion's destination is a compact native workbench framework where:

- windows and messages remain the runtime foundation;
- `.orion` is the declarative application and contribution manifest;
- pages are independently owned window trees;
- hosts compose pages and project their chrome;
- commands remain stable across every interaction surface;
- databases provide declarative view binding;
- gems extend applications with pages, actions, data sources, and components.

The organizing principle is simple:

> Pages own behavior and declarations. Hosts own presentation and composition.
