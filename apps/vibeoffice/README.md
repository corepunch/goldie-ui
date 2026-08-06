# Vibe Office

`vibeoffice` demonstrates Orion's individual `Icon` common control. Each office
role is parented directly to the framework-owned desktop, can be selected and
dragged independently, and renders badges for its selected model and live task status.
Agents also own small full-colour office artefacts on their right (tickets, chats,
plans, files, bugs, reports, and related objects). A count badge represents multiple
objects of one type. While dragging, the artefact follows the pointer as a transparent
drag image. Dropping it onto an accepting agent transfers one object; rejected drops
return it to its original inventory slot.
The artefact artwork convention and instructions for extending the set are in
[`share/artifacts/README.md`](share/artifacts/README.md).

Single-clicking selects a desk. Double-clicking opens that agent's inspector;
if it is already open, the existing window is raised and focused. Each agent
keeps an independent inspector view. Submitting a message writes
`.tasks/desk-{id}.json`, runs `opencode run --model <model> <message>` once, and polls the child
and task file until the response or error is persisted. Every desk owns a
separate process record and JSON file, so multiple desks can run concurrently.
The inspector structure and auto-layout live in `vibeoffice.orion`; C only binds
the generated controls to runtime task behavior. Its model dropdown uses the same
declarative database datasource pattern as SocialFeed.

This is deliberately a one-request proof of wiring: there is no queue, retry,
streaming output, resident process, or backend abstraction yet.
