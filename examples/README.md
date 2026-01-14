# UI Framework Examples

This directory contains example programs demonstrating the use of the UI framework.

## Hello World Example

**File:** `helloworld.c`

A simple standalone application that demonstrates:
- Initializing SDL and OpenGL
- Creating windows using the UI framework
- Handling window messages and events
- Drawing text in a window
- Basic event loop

### Building

```bash
make ui-helloworld
```

### Running

```bash
./ui-helloworld
```

### What it does

The example creates a simple window with "Hello World!" text displayed in the center. The window can be moved, and clicking the close button will exit the application.

### Code Structure

1. **Initialization** - Sets up SDL, creates an OpenGL context
2. **Window Creation** - Uses the UI framework to create a desktop and a hello world window
3. **Event Loop** - Processes SDL events and window messages
4. **Rendering** - Draws the windows and their contents
5. **Cleanup** - Properly shuts down SDL

### Current Limitations

The example currently links against some mapview files (sprites.c, font.c, icons.c) for drawing functions. This is temporary until these functions are fully extracted into the UI framework.

## Future Examples

Additional examples to be added:
- **Button Example** - Demonstrating button controls
- **Form Example** - Creating forms with various controls
- **Dialog Example** - Modal dialogs and message boxes
- **Multi-Window Example** - Managing multiple windows

## Dependencies

The UI framework examples require:
- SDL2
- OpenGL 3.2+

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install libsdl2-dev libgl1-mesa-dev
```

**macOS:**
```bash
brew install sdl2
```

**Fedora/RHEL:**
```bash
sudo dnf install SDL2-devel mesa-libGL-devel
```
