# WinAPI → Orion Mapping

Complete concept mapping from WinAPI to Orion.

## Window Management

| WinAPI | Orion | Notes |
|--------|-------|-------|
| `HWND` | `window_t *` | Window handle |
| `WNDPROC` | `winproc_t` | `result_t fn(window_t*, uint32_t msg, uint32_t wparam, void *lparam)` |
| `CreateWindow` | `create_window(title, flags, rect, parent, proc, userdata)` | |
| `DestroyWindow` | `destroy_window(win)` | |
| `ShowWindow` | `show_window(win, visible)` | |
| `InvalidateRect` | `invalidate_window(win)` | |
| `SetWindowLongPtr` | `win->userdata` | Use `allocate_window_data(win, size)` |

## Message Loop

| WinAPI | Orion | Notes |
|--------|-------|-------|
| `GetMessage` loop | `while (get_message(&e)) dispatch_message(&e); repost_messages(-1);` | |
| `TranslateAccelerator` | `translate_accelerator(win, table, &e)` | Call before `dispatch_message` |
| `WM_COMMAND` | `evCommand` | Notifications flow through this |

## Message Packing

| WinAPI | Orion | Notes |
|--------|-------|-------|
| `LOWORD(wParam)` | `LOWORD(wparam)` | Control ID |
| `HIWORD(wParam)` | `HIWORD(wparam)` | Notification code |
| `MAKEDWORD(lo,hi)` | `MAKEDWORD(lo,hi)` | Pack ID + code |

## Dialogs

| WinAPI | Orion | Notes |
|--------|-------|-------|
| `DialogBox` | `show_dialog(parent, proc, userdata)` | |
| `EndDialog` | `end_dialog(win, result)` | |

## Geometry

| WinAPI | Orion | Notes |
|--------|-------|-------|
| `RECT` | `irect16_t { int x, y, w, h; }` | Use `MAKERECT(x,y,w,h)` |
| `POINT` | `ipoint16_t { int x, y; }` | |

## Common Controls

| WinAPI | Orion | Notes |
|--------|-------|-------|
| `BN_CLICKED` | `btnClicked` | Button click notification |
| `CB_ADDSTRING` | `CB_ADDSTRING` | Same constant |
| `CBN_SELCHANGE` | `CBN_SELCHANGE` | Same constant |
| Accelerator table | `load_accelerators(accel_t[], count)` / `free_accelerators(table)` | |
