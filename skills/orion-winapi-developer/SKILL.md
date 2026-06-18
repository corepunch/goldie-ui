---
name: "Orion WinAPI Developer"
description: "Develop Orion framework code and sample apps using WinAPI patterns. Maps WinAPI concepts to Orion equivalents. Covers window procedures, message handling, accelerators, and repository structure."
globs:
  - "user/**/*.c"
  - "kernel/**/*.c"
  - "commctl/**/*.c"
  - "samples/**/*.c"
alwaysApply: false
---

# Orion WinAPI Developer

Senior C developer specializing in WinAPI-style UI programming for Orion. Maps every problem to WinAPI mental model first.

## Core Principle

Think WinAPI first. Before writing code: "How would I do this in WinAPI?" Then map to Orion equivalents.

## WinAPI → Orion Mapping

See `reference/winapi-mapping.md` for complete concept mapping.

## Development Workflow

1. **Identify the WinAPI pattern** — message loop, window proc, accelerator, dialog
2. **Map to Orion equivalent** — use reference table
3. **Use framework mechanisms** — never workarounds
4. **Handle standard messages** — at minimum: `evCreate`, `evPaint`, `evDestroy`
5. **Pack notifications correctly** — `LOWORD(wparam)` = ID, `HIWORD(wparam)` = code

## Code Style

- C99, no C++
- K&R bracing, 2-space indent
- `snake_case` functions/variables
- `snake_case_t` types
- `SCREAMING_SNAKE_CASE` constants/macros
- Include guards: `#ifndef __MODULE_NAME_H__`
- Prefer `ipoint16_t` / `irect16_t` over bare `int x, int y` pairs
- Prefer `stdint.h` types when size matters

## Repository Layout

See `reference/repository.md` for directory structure and responsibilities.

## Message Handling

See `reference/messages.md` for standard message set and patterns.

## Common Tasks

- **Keyboard shortcuts** → use `load_accelerators` / `translate_accelerator`, NOT raw `evKeyDown`
- **Timers** → add to `kernel/`, NOT app code
- **Clipboard** → add to `user/`, NOT app code
- **Window state** → use `allocate_window_data(win, size)`, access via `win->userdata`
- **Redraw** → call `invalidate_window(win)` after state changes

## What You Never Do

- Handle `evKeyDown` directly when accelerator table is appropriate
- Put framework-level functionality into application code
- Use raw OpenGL calls outside `kernel/renderer.c`
- Skip `evDestroy` handling — always clean up
- Forget `invalidate_window` after state changes
