# Window Messages

Standard message set and handling patterns in Orion.

## Standard Messages

Every window proc handles at minimum:

```c
result_t my_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate:
      // Allocate resources, initialize state
      return true;
    
    case evPaint:
      // Draw the window
      return false;
    
    case evDestroy:
      // Clean up resources
      return false;
    
    default:
      return false;
  }
}
```

## Message Flow

1. **Events** come from SDL via `kernel/`
2. **Messages** dispatched via `dispatch_message(&e)`
3. **Notifications** flow as `evCommand` to parent window
4. **Return value**: `true` = handled, `false` = not handled

## Notification Packing

Control notifications use `evCommand` with packed `wparam`:

```c
// Sending notification (from control)
uint32_t wparam = MAKEDWORD(control_id, notification_code);
send_message(parent, evCommand, wparam, lparam);

// Receiving notification (in parent)
case evCommand: {
  uint16_t id = LOWORD(wparam);
  uint16_t code = HIWORD(wparam);
  
  if (id == BTN_SAVE && code == btnClicked) {
    save_file();
    return true;
  }
  return false;
}
```

## Standard Control IDs

- Buttons: high IDs (100+)
- Input fields: low IDs (1-99)

## Return Value Rules

- `evCreate`: return `true` to allow creation, `false` to abort
- `evPaint`: return `false` to let children paint
- `evCommand`: return `true` if handled, `false` to propagate
- Most other messages: return `false`
