# Repository Layout

Orion framework directory structure and responsibilities.

## Directory Structure

```
ui.h              ← include this in every app; pulls in all subsystems
user/             ← window management, message queue, drawing, text, accelerators (USER.DLL)
kernel/           ← SDL event loop, init, renderer (KERNEL.DLL)
commctl/          ← reusable controls: button, checkbox, edit, label, list, combobox, console (COMCTL32.DLL)
samples/          ← sample applications that demonstrate and exercise the framework
tests/            ← all test source files (*.c)
tests/test_framework.h   ← the test framework (include this, nothing else from tests/)
tests/test_env.h    ← SDL-init helper for tests that do need a display
Makefile          ← `make test` builds and runs all tests/
```

## Layer Responsibilities

### user/ (USER.DLL)
- Window management
- Message queue
- Drawing primitives
- Text rendering
- Accelerator tables

### kernel/ (KERNEL.DLL)
- SDL event loop
- Initialization
- Renderer (OpenGL)
- **renderer.c / renderer_impl.c** — only place for raw OpenGL calls

### commctl/ (COMCTL32.DLL)
- Button
- Checkbox
- Text edit
- Label
- List
- Combobox
- Console

### samples/
- Each sample in its own subdirectory
- Has `main.c`
- Includes `../../ui.h`
- Follows helloworld pattern
- Also serves as integration tests — must compile and run cleanly

## Include Pattern

Every app includes just `ui.h`:

```c
#include "../../ui.h"  // or appropriate path
```

`ui.h` pulls in all subsystems transitively.
