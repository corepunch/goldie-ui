# Orion Applications

The `apps/` tree contains complete applications built on Orion. Each app is
both usable software and a reference for framework patterns that are too large
to demonstrate in an isolated test.

See the [Application Gallery](../docs/examples.md) for screenshots, capability
summaries, and recommended examples by topic.

## Build And Run

From the repository root:

```bash
make apps                         # all standalone applications
make gems                         # all loadable GEM modules
make build/bin/imageeditor        # one standalone application
make build/gem/imageeditor.gem    # one loadable application

build/bin/imageeditor images/logo.png
build/bin/shell
```

Most apps share one source lifecycle through `gem_init` and
`GEM_STANDALONE_MAIN`. Application arguments reach `gem_init`; the standard
launcher owns framework options such as `--screenshot PATH`.

Released applications can be installed independently:

```bash
orion search
orion install imageeditor
orion list
```

See the [Package Manager guide](../packaging/README.md) for SDK and release
installation.

## Application Shape

Use this structure for a substantial app:

```text
apps/<name>/
  dialogs/       Modal dialog procedures
  components/    Reusable app-specific controls
  pages/         Page-specific controllers and views
  datasource/    Database adapters when the app owns several
  share/         Icons, seed data, fixtures, and app assets
  tests/         App-specific tests
```

At the app root, keep the `.orion` resource definition, public app header,
entry point, controller, and top-level views. Small apps can omit directories
they do not need.

The usual ownership split is:

- **Model/data source** owns persistent data, allocation, CRUD, and validation.
- **Controller** owns app state, active document/page, commands, and refreshes.
- **View** owns window procedures, painting, hit testing, and notifications.
- **Declarative resources** own static forms, menus, toolbars, accelerators, and bindings.

For designer-style apps, keep project I/O, persistent document models,
component metadata, canvas runtime state, layout, and inspectors in separate
modules. Persistent records must not contain `window_t *` or renderer handles.

Read [Architecture](../ARCHITECTURE.md) for the full boundary rules.

## Framework Patterns

- Define command IDs and default shortcuts in `.orion` menus.
- Route controls through `evCommand`; inspect `LOWORD` and `HIWORD`.
- Use accelerator tables for shortcuts instead of raw `evKeyDown` checks.
- Use forms for dialogs or panels containing two or more standard controls.
- Register declarative resources once, then resolve them by name.
- Extend framework controls when behavior is generally reusable.
- Use `get_client_rect()` and central input routing for client coordinates.
- Invalidate on visual state changes and draw only during `evPaint`.
- Pair resources allocated in `evCreate` with cleanup in `evDestroy`.

## Good References

| Topic | Application |
|---|---|
| Minimal lifecycle | `helloworld` |
| Database-bound forms and tables | `socialfeed` |
| Multi-document editing and canvas tools | `imageeditor` |
| Tabs, report views, staging, and diffs | `gitclient` |
| Declarative authoring and component plugins | `formeditor` |
| MVC project workflow | `taskmanager` |
| Custom desktop-style controls | `vibeoffice` |
| Scene editing and headless rendering | `scener` |

## Presenting An Application

Every substantial app should document:

- What it is and who it serves
- Its primary workflows and capabilities
- The Orion concepts it demonstrates
- Build and run commands
- One or more screenshots containing realistic data
- Important architecture or extension points

Standalone apps can generate deterministic JPEGs:

```bash
build/bin/<name> [content-arguments...] \
  --screenshot docs/screenshots/<name>_main.jpg
```

Use F12 for interactive capture. Follow the naming, composition, and review
checklist in [Presenting an Application](../docs/app-presentation.md).
