# UI Framework

This directory contains the extracted UI framework for DOOM-ED, organized in a Windows-like architecture.

## Directory Structure

```
ui/
├── ui.h              # Main header that includes all UI subsystems
├── user/             # Window management and user interface (USER.DLL equivalent)
│   ├── user.h        # Window structures and management functions
│   ├── messages.h    # Window message constants and macros
│   ├── draw.h        # Drawing primitives (rectangles, icons)
│   ├── text.h        # Text rendering functions (NEW)
│   ├── text.c        # Text rendering implementation (small font, DOOM/Hexen fonts)
│   ├── window.c      # Window management implementation
│   ├── message.c     # Message queue implementation
│   └── draw_impl.c   # Drawing primitives implementation
├── kernel/           # Event loop and SDL integration (KERNEL.DLL equivalent)
│   ├── kernel.h      # Event management and SDL initialization
│   ├── event.c       # Event loop implementation
│   ├── init.c        # SDL initialization
│   └── joystick.c    # Joystick/gamepad support
└── commctl/          # Common controls (COMCTL32.DLL equivalent)
    ├── commctl.h     # Common control window procedures
    ├── button.c      # Button control implementation
    ├── checkbox.c    # Checkbox control implementation
    ├── edit.c        # Text edit control implementation
    ├── label.c       # Label (static text) control implementation
    ├── list.c        # List control implementation
    ├── combobox.c    # Combobox (dropdown) control implementation
    ├── console.h     # Console control header (NEW)
    └── console.c     # Console control implementation (NEW)
```

## Architecture

The UI framework follows a layered architecture similar to Windows:

### ui/user/ - Window Management Layer
Handles window creation, destruction, message passing, and basic rendering primitives.

**Key Components:**
- Window structure (`window_t`)
- Window creation and lifecycle management
- Message queue and dispatch
- Drawing primitives (rectangles, text, icons)
- Window messages (WM_CREATE, WM_PAINT, WM_LBUTTONUP, etc.)

### ui/kernel/ - Event Management Layer
Manages the SDL event loop and translates SDL events into window messages.

**Key Components:**
- SDL initialization
- Event loop (`get_message`, `dispatch_message`)
- Global state (screen dimensions, running flag)

### ui/commctl/ - Common Controls Layer
Implements standard UI controls that can be used to build interfaces.

**Available Controls:**
- **Button**: Clickable button with text label
- **Checkbox**: Toggle control with checkmark
- **Edit**: Single-line text input control
- **Label**: Static text display
- **List**: Scrollable list of items
- **Combobox**: Dropdown selection control
- **Console**: Message display console with automatic fading and scrolling

## Usage

Include the main header in your code:

```c
#include "ui/ui.h"
```

Or include specific subsystems:

```c
#include "ui/user/user.h"
#include "ui/commctl/commctl.h"
```

### Creating a Window

```c
rect_t frame = {100, 100, 200, 150};
window_t *win = create_window("My Window", 0, &frame, NULL, my_window_proc, NULL);
show_window(win, true);
```

### Creating Controls

```c
// Create a button
window_t *btn = create_window("Click Me", 0, &btn_frame, parent, win_button, NULL);

// Create a checkbox
window_t *chk = create_window("Enable Feature", 0, &chk_frame, parent, win_checkbox, NULL);

// Create an edit box
window_t *edit = create_window("Enter text", 0, &edit_frame, parent, win_textedit, NULL);

// Create a console window
window_t *console = create_window("Console", 0, &console_frame, parent, win_console, NULL);
```

### Using the Console

```c
#include "ui/commctl/console.h"

// Initialize the console system (call once at startup)
init_console();

// Print messages to the console
conprintf("Game started");
conprintf("Player health: %d", player_health);

// Messages automatically fade after 5 seconds
// Draw the console overlay (called from your render loop or window procedure)
// draw_console() is called automatically by win_console in WM_PAINT

// Toggle console visibility
toggle_console();

// Clean up (call at shutdown)
shutdown_console();
```

## Window Messages

The framework uses a message-based architecture. Common messages include:

- `WM_CREATE` - Window is being created
- `WM_DESTROY` - Window is being destroyed
- `WM_PAINT` - Window needs to be redrawn
- `WM_LBUTTONDOWN` - Left mouse button pressed
- `WM_LBUTTONUP` - Left mouse button released
- `WM_KEYDOWN` - Key pressed
- `WM_KEYUP` - Key released
- `WM_COMMAND` - Control notification

## Control-Specific Messages

### Button Messages
- `BN_CLICKED` - Button was clicked

### Checkbox Messages
- `BM_SETCHECK` - Set checkbox state
- `BM_GETCHECK` - Get checkbox state

### Combobox Messages
- `CB_ADDSTRING` - Add item to combobox
- `CB_GETCURSEL` - Get currently selected item
- `CB_SETCURSEL` - Set currently selected item
- `CBN_SELCHANGE` - Selection changed notification

### Edit Box Messages
- `EN_UPDATE` - Text was modified

## Text Rendering API

The UI framework provides text rendering through `ui/user/text.h`:

### Small Bitmap Font (6x8 pixels)

```c
#include "ui/user/text.h"

// Initialize text rendering system (call once at startup)
init_text_rendering();

// Draw text with small bitmap font
draw_text_small("Hello World", x, y, 0xFFFFFFFF); // color as RGBA

// Measure text width
int width = strwidth("Hello World");
int partial_width = strnwidth("Hello", 5);

// Clean up (call at shutdown)
shutdown_text_rendering();
```

### DOOM/Hexen Game Font

```c
// Load game font from WAD file (call after loading WAD)
load_console_font();

// Draw text with game font (includes fade-out effect)
draw_text_gl3("DOOM", x, y, 1.0f); // alpha 0.0-1.0

// Measure game font text width
int width = get_text_width("DOOM");
```

### Notes
- Small bitmap font supports all 128 ASCII characters
- DOOM/Hexen font supports characters 33-95 (printable ASCII)
- Font atlas is created automatically for efficient rendering
- Text rendering uses OpenGL for hardware acceleration


## Status

This is an in-progress refactoring. The framework currently:

✅ Has header files defining the API structure
✅ Has extracted common control implementations (button, checkbox, edit, label, list, combobox, console)
✅ Has extracted text rendering to `ui/user/text.c` (small bitmap font and DOOM/Hexen fonts)
✅ Has moved console to `ui/commctl/console.c` (console message management and display)
✅ Has integrated with build system (Makefile)
⏳ Still needs core window management code to be moved from mapview/window.c
⏳ Still needs drawing primitives to be moved from mapview/sprites.c

## Recent Changes

### Text Rendering Module (ui/user/text.c, ui/user/text.h)
- **Small bitmap font rendering**: `draw_text_small()`, `strwidth()`, `strnwidth()`
- **DOOM/Hexen font rendering**: `draw_text_gl3()`, `get_text_width()`, `load_console_font()`
- **Font atlas management**: Automatic creation of font texture atlas for efficient rendering
- Extracted from mapview/windows/console.c to make text rendering reusable

### Console Module (ui/commctl/console.c, ui/commctl/console.h)
- **Console message management**: Circular buffer for console messages with timestamps
- **Message display**: Automatic fading and scrolling of recent messages
- **Public API**: `init_console()`, `conprintf()`, `draw_console()`, `shutdown_console()`, `toggle_console()`
- Uses text rendering module for display
- Moved from mapview/windows/console.c to UI framework

## Future Work

1. Move core window management functions to `ui/user/window.c`
2. Move message queue to `ui/user/message.c`
3. Move drawing primitives to `ui/user/draw.c`
4. Update Xcode project with new file locations
5. Add documentation for each function
6. Add example code
7. Add unit tests for UI components
