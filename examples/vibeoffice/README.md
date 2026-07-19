# Vibe Office

`vibeoffice` demonstrates Orion's individual `Icon` common control. Each office
role is parented directly to the framework-owned desktop, can be selected and
dragged independently, and renders badges for its role and live task status.

Selecting a desk opens its inspector. Submitting a message writes
`.tasks/desk-{id}.json`, runs `opencode run <message>` once, and polls the child
and task file until the response or error is persisted. Every desk owns a
separate process record and JSON file, so multiple desks can run concurrently.
The inspector structure and auto-layout live in `vibeoffice.orion`; C only binds
the generated controls to runtime task behavior.

This is deliberately a one-request proof of wiring: there is no queue, retry,
streaming output, resident process, or backend abstraction yet.
