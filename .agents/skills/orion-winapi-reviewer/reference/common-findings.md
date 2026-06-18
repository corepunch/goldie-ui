# Common Review Findings

Detailed examples of code review findings with fixes.

## 🔴 Raw Key Handling Instead of Accelerators

```c
// BAD — polling WM_KEYDOWN for shortcuts is error-prone and bypasses the framework
case evKeyDown:
  if (wparam == SDL_SCANCODE_S && /* ctrl check? */)
    save_file();
  break;
```

> "In WinAPI, we'd use an accelerator table for this. Keyboard shortcuts belong in `load_accelerators` so they fire as `evCommand` with `kAcceleratorNotification`, not via raw key polling."

**Fix:**
```c
// Define accelerator table
accel_t accelerators[] = {
  { FCONTROL, SDL_SCANCODE_S, 100 }  // Ctrl+S = Save
};

// In init
table = load_accelerators(accelerators, ARRAY_SIZE(accelerators));

// In message loop
while (get_message(&e)) {
  if (!translate_accelerator(win, table, &e))
    dispatch_message(&e);
}

// In window proc
case evCommand:
  if (LOWORD(wparam) == 100 && HIWORD(wparam) == kAcceleratorNotification) {
    save_file();
    return true;
  }
```

## 🔴 HIWORD/LOWORD Packed Backwards

```c
// BAD — notification code and ID are swapped
send_message(parent, evCommand, MAKEDWORD(btnClicked, btn->id), NULL);
```

> "In WinAPI, `LOWORD(wParam)` is the control ID and `HIWORD(wParam)` is the notification code. These are reversed here."

**Fix:**
```c
send_message(parent, evCommand, MAKEDWORD(btn->id, btnClicked), NULL);
```

## 🔴 Forgetting evDestroy

```c
result_t my_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case evCreate: /* allocates resources */ return true;
    case evPaint:  /* draws */ return false;
    // No evDestroy — resource leak!
  }
}
```

> "In WinAPI, every window that acquires resources in `WM_CREATE` must release them in `WM_DESTROY`. This window proc leaks."

**Fix:**
```c
case evDestroy:
  // Free all resources allocated in evCreate
  free(my_data);
  return false;
```

## 🟡 Application Code That Belongs in Framework

> "In WinAPI, timer management lives in `SetTimer`/`KillTimer` — they're OS-level. If Orion lacks a timer API, add one to `kernel/` rather than polling `SDL_GetTicks` in the window proc."

## 🟡 Missing invalidate_window After State Change

> "In WinAPI, any state change that affects visual appearance must be followed by `InvalidateRect`. Without it, the window won't repaint until the next incidental repaint event."

**Fix:**
```c
my_state = new_value;
invalidate_window(win);  // Trigger repaint
```

## 🟡 Drawing Outside WM_PAINT

> "In WinAPI, you should never call drawing functions outside the `WM_PAINT` (or `evPaint`) handler. Move this draw call into the paint handler and trigger it via `invalidate_window`."

## 🔵 Parallel Coordinate Fields Instead of Structs

> "In WinAPI, `POINT` and `RECT` are first-class structs. Use Orion's `ipoint16_t` and `irect16_t` rather than loose `x`/`y` pairs — it makes the intent clear and matches the WinAPI convention."

**Fix:**
```c
// Before
int x, y, w, h;

// After
irect16_t rect = MAKERECT(x, y, w, h);
```
