# Agent Terminal Workspace

> A workspace operating system for developers and AI agents.

> This document describes the vision behind the project. It is intentionally aspirational rather than a fixed architecture. Like any successful product, the concept is expected to evolve over time.

---

# Vision

Software development is becoming increasingly **process-oriented**.

The editor is no longer the center of the workflow.

Modern developers routinely run multiple long-lived processes simultaneously:

* AI coding agents
* editors
* Git interfaces
* build systems
* test runners
* log streams
* debuggers
* language servers
* custom project tools

Today's tools approach this in two ways:

* **IDEs** embed everything as plugins inside a single application.
* **Terminal multiplexers** manage panes but know very little about what those panes represent.

Agent Terminal Workspace takes a different approach.

**Processes become the primary abstraction.**

The application is not another editor, nor another terminal emulator.

It is a **workspace manager** that orchestrates terminal-native tools into a coherent development environment.

---

# The Core Idea

Every window is simply a process.

```text
Editor        -> exec("nvim")
Git           -> exec("lazygit")
AI Agent      -> exec("opencode")
Shell         -> exec("/bin/zsh")
Debugger      -> exec("lldb")
Logs          -> exec("tail -f")
GitHub        -> exec("gh")
```

If an application can run inside a PTY, it automatically becomes a first-class citizen of the workspace.

No plugins.

No custom integrations.

No reimplementation.

The PTY is effectively the plugin API.

---

# Workspace Instead of Terminal

Traditional terminal applications manage sessions.

Traditional IDEs manage editors.

Agent Terminal Workspace manages **projects**.

Opening a workspace restores:

* repositories
* worktrees
* running agents
* editor instances
* Git sessions
* layouts
* scripts
* history
* task metadata

A Workspace is the unit of work.

Not a window.

Not a terminal.

Not an editor.

---

# Window = Process

Every visible window represents exactly one running process.

Examples:

```text
Workspace

├── Neovim
├── Claude Code
├── OpenCode
├── lazygit
├── Build
├── Tests
├── Logs
└── Python REPL
```

Internally every one of these is simply:

* PTY
* stdin
* stdout
* stderr
* process state

The workspace manager does not care what the process actually is.

---

# Layout Is Part of the Project

Layouts are not temporary UI state.

They are part of the workspace.

Returning to a project next week restores:

* tiled layout
* open windows
* running processes
* scrollback
* worktree assignments
* active agents

Exactly where you left them.

---

# Desktop Instead of File Explorer

Traditional IDEs expose files.

Agent Terminal Workspace exposes **actions**.

Instead of opening files, desktop icons launch workflows.

Examples:

```text
🚀 Build
🧪 Run Tests
🤖 Spawn Review Agent
🎨 Icon Pipeline
📦 Package
📊 Coverage
📋 Open Pull Request
```

Actions may execute one command or entire pipelines.

Example:

```text
Generate icons
    ↓
Remove background
    ↓
Slice sprites
    ↓
Resize
    ↓
Create atlas
```

The desktop becomes an executable control panel for the project.

---

# Explorer Is Just Another Application

The file tree is not embedded inside the editor.

It is simply another client of the workspace.

Selecting a file requests:

```text
Open src/main.cpp
```

The workspace manager decides whether to:

* reuse an existing editor
* create a new one
* split the layout
* open another process

The explorer does not know or care which editor is running.

This makes every component replaceable.

---

# AI Agents Become First-Class Citizens

Modern workflows increasingly revolve around AI agents.

Instead of embedding an AI panel inside an editor, every agent is simply another managed process.

Example:

```text
Workspace

├── Coding Agent
├── Review Agent
├── Documentation Agent
└── Release Agent
```

Each agent has:

* repository
* worktree
* status
* logs
* history
* cost
* model
* permissions

The workspace becomes the control center for collaborative human/AI development.

---

# Why This Is Not "Another Terminal"

The terminal space is already crowded:

* WezTerm
* tmux
* Zellij
* Warp
* Pane

Competing directly as a terminal emulator is not the goal.

The opportunity lies elsewhere:

Managing multiple AI agents across repositories and worktrees.

Examples:

* repository ↔ worktree ↔ task mapping
* idle/running/needs approval states
* GitHub workflow integration
* session replay
* safety layer
* project persistence

This is much closer to a workspace operating system than a terminal emulator.

---

# Guiding Principles

## 1. Window = Process

Any CLI application that runs inside a PTY automatically works.

No plugins.

No custom UI.

---

## 2. Workspace Over Terminal

Users open projects, not terminal sessions.

Repositories, layouts, agents and history are restored together.

---

## 3. Layout Is Persistent

Window arrangement belongs to the project.

Not to the application.

---

## 4. Desktop = Actions

Desktop icons launch workflows instead of opening files.

---

## 5. GPU-Based UI

The UI is rendered entirely by the application.

Goals:

* identical rendering everywhere
* independence from host window systems
* portable to macOS
* Linux
* Windows
* iPadOS
* Android

A mobile client should be capable of managing development workspaces rather than being a limited companion application.

---

## 6. Don't Patch The Agent

The workspace should observe agents rather than modify them.

Information may come from:

* terminal output
* structured integrations
* APIs

The agent itself should remain unchanged.

---

# Technical Foundation

The project builds on an existing framework:

* C
* Lua
* custom window manager
* internal application windows
* process management
* UI toolkit

Much of the infrastructure already exists.

Remaining work includes:

* PTY layer
* monospace GPU text renderer
* tiling layout engine
* workspace persistence
* GitHub integration through `gh`
* desktop action layer

Cross-platform support is considered a core architectural requirement rather than a future porting effort.

---

# MVP

The first milestone should include:

* multiple tiled terminal panes
* panes associated with repository/worktree/task
* process status indicators
* workspace persistence
* hotkeys for common workflows
* GitHub sidebar via `gh`
* session logging and replay
* basic safety layer

Status examples:

* idle
* running
* needs approval
* tests failed
* diff ready

Typical actions:

* create worktree
* spawn agent
* run tests
* open issue
* create PR
* show diff

---

# Example Workflow

One repository.

Several worktrees.

Multiple AI agents.

One unified workspace.

```text
open-realm

├── worktree #1
│     └── Coding Agent
│
├── worktree #2
│     └── Review Agent
│
├── worktree #3
│     └── Experimental Agent
│
├── lazygit
├── Build
├── Tests
└── GitHub
```

The entire environment is saved and restored as a single workspace.

---

# Safety Layer

The workspace should provide a permission layer between users and autonomous agents.

Examples:

* allow / deny dangerous commands
* confirmation dialogs
* token protection
* command auditing
* session replay

Initial implementations may use a simple allowlist before evolving toward stronger sandboxing.

---

# Open Questions

* Should native UI windows and PTY windows share a single layout engine from day one?
* What is the best workspace format?
  * JSON
  * Lua
  * binary
* How much isolation belongs in the MVP?
* Should mobile begin as a thin SSH client or a standalone runtime?
* Which information should come from terminal parsing versus structured integrations?

---

# Why Now?

Five years ago this idea would have been premature.

Today many developers already spend their day orchestrating:

* Claude Code
* Codex CLI
* OpenCode
* Aider
* lazygit
* GitHub CLI
* build systems
* test runners
* multiple worktrees

Existing terminals see these as anonymous panes.

IDEs attempt to absorb them as plugins.

Agent Terminal Workspace treats them as first-class workspace objects.

---

# Long-Term Vision

The Workspace is to developer tools what the Desktop is to applications.

Editors.

Agents.

Git.

Debuggers.

Build systems.

Logs.

Everything becomes an independent process cooperating through a common runtime.

The workspace manager does not replace existing tools.

It elevates them into a persistent, process-oriented development environment.

We are not building another terminal.

We are not building another IDE.

We are building a workspace operating system for human developers and AI agents.
