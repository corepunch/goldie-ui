# Message Reference

Complete list of window and database messages.

## Window Messages

### Lifecycle

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evCreate` | 0 | State pointer | true | Window created |
| `evDestroy` | 0 | NULL | true | Window destroyed |
| `evShowWindow` | bool show | 0 | 0 | Window shown/hidden |
| `evClose` | 0 | NULL | true | Close requested |

### Paint

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evPaint` | 0 | NULL | false | Needs redraw |
| `evNCPaint` | 0 | NULL | 0 | Non-client paint |
| `evRefreshStencil` | 0 | NULL | 0 | Stencil refresh |
| `evPaintStencil` | 0 | NULL | 0 | Stencil paint |

### Focus

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evSetFocus` | 0 | 0 | 0 | Gained focus |
| `evKillFocus` | 0 | 0 | 0 | Lost focus |
| `evActivate` | state | 0 | 0 | Window activated |

### Mouse

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evMouseMove` | keys | MAKELONG(x,y) | 0 | Mouse moved |
| `evLeftButtonDown` | keys | MAKELONG(x,y) | 0 | Left button down |
| `evLeftButtonUp` | keys | MAKELONG(x,y) | 0 | Left button up |
| `evLeftButtonDoubleClick` | keys | MAKELONG(x,y) | 0 | Left double-click |
| `evRightButtonDown` | keys | MAKELONG(x,y) | 0 | Right button down |
| `evRightButtonUp` | keys | MAKELONG(x,y) | 0 | Right button up |
| `evMouseLeave` | 0 | 0 | 0 | Mouse left window |
| `evHitTest` | MAKELONG(x,y) | 0 | HT_* | Hit test |

### Keyboard

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evKeyDown` | vk_code | repeat | 0 | Key pressed |
| `evKeyUp` | vk_code | 0 | 0 | Key released |
| `evTextInput` | 0 | text | 0 | Text input |

### Scroll

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evHScroll` | code | pos | 0 | Horizontal scroll |
| `evVScroll` | code | pos | 0 | Vertical scroll |

### Resize

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evResize` | 0 | MAKELONG(w,h) | 0 | Window resized |
| `evDisplayChange` | 0 | 0 | 0 | Display changed |

### Timer

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evTimer` | timer_id | 0 | 0 | Timer expired |

### Command

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evCommand` | MAKEDWORD(id,code) | control_ptr | true | Control notification |

### Drag and Drop

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evMouseDragEnter` | keys | MAKELONG(x,y) | 0 | Drag entered |
| `evMouseDragLeave` | 0 | 0 | 0 | Drag left |
| `evMouseDrag` | keys | MAKELONG(x,y) | 0 | Drag moving |
| `evMouseDrop` | keys | MAKELONG(x,y) | 0 | Drop occurred |

### HTTP

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evHttpDone` | 0 | response_ptr | 0 | HTTP request complete |
| `evHttpProgress` | 0 | progress_ptr | 0 | HTTP progress update |

### Custom

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `evUser` | custom | custom | custom | User-defined base |
| `evStatusBar` | 0 | text_ptr | 0 | Set status bar text |

## Control Messages

### Button

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `btnClicked` | 0 | button_ptr | 0 | Button clicked |
| `btnSetCheck` | state | 0 | 0 | Set check state |
| `btnGetCheck` | 0 | 0 | state | Get check state |
| `btnSetImage` | icon_index | strip_ptr | 0 | Set button icon |
| `btnStateUnchecked` | 0 | 0 | 0 | Checkbox unchecked |
| `btnStateChecked` | 0 | 0 | 0 | Checkbox checked |

### TextBox/Edit

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `edGetText` | buf_size | char* dst | length | Get text |
| `edSetText` | 0 | const char* src | 0 | Set text |
| `edUpdate` | 0 | edit_ptr | 0 | Text modified |

### ComboBox

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `cbAddString` | 0 | string_ptr | index | Add item |
| `cbGetCurrentSelection` | 0 | int* out | index | Get selection |
| `cbGetCurrentValue` | 0 | 0 | value | Get value field |
| `cbSetCurrentSelection` | index | 0 | 0 | Set selection |
| `cbGetListBoxText` | index | char* buf | length | Get item text |
| `cbClear` | 0 | 0 | 0 | Clear all items |
| `cbSelectionChange` | 0 | combo_ptr | 0 | Selection changed |

### ListBox

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `lstSetItem` | index | 0 | 0 | Set selected item |

### TableView/ColumnView

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `RVM_ADDCOLUMN` | column_def | 0 | 0 | Add column |
| `RVM_ADDITEM` | 0 | item_ptr | 0 | Add item |
| `RVM_DELETEITEM` | index | 0 | 0 | Delete item |
| `RVM_GETITEMCOUNT` | 0 | 0 | count | Get item count |
| `RVM_GETSELECTION` | 0 | 0 | index | Get selection |
| `RVM_SETSELECTION` | index | 0 | 0 | Set selection |
| `RVM_CLEAR` | 0 | 0 | 0 | Clear all items |
| `RVM_SETCOLUMNWIDTH` | width | 0 | 0 | Set column width |
| `RVM_GETCOLUMNWIDTH` | 0 | 0 | width | Get column width |
| `RVM_GETITEMDATA` | index | 0 | data | Get item data |
| `RVM_SETITEMDATA` | index | data | 0 | Set item data |
| `RVM_SETREDRAW` | enable | 0 | 0 | Enable/disable redraw |
| `RVN_SELCHANGE` | index | list_ptr | 0 | Selection changed |
| `RVN_DBLCLK` | index | list_ptr | 0 | Double-click |
| `RVN_DELETE` | index | list_ptr | 0 | Delete requested |

### TableView (db-aware)

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `tvRefresh` | 0 | 0 | 0 | Refresh from database |
| `tvSetFilter` | 0 | filter_value | 0 | Set filter value |

### Toolbar

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `tbButtonClick` | button_id | 0 | 0 | Button clicked |
| `tbSetStrip` | 0 | strip_ptr | 0 | Set icon strip |
| `tbSetActiveButton` | button_id | 0 | 0 | Set active button |
| `tbSetButtonSize` | size | 0 | 0 | Set button size |
| `tbLoadStrip` | tile_size | path_ptr | 0 | Load icon strip |
| `tbSetItems` | count | items_ptr | 0 | Set toolbar items |
| `tbDropdown` | button_id | 0 | 0 | Dropdown clicked |

### ScrollBar

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `sbAddWindow` | 0 | window_ptr | 0 | Add scrollable window |
| `sbSetInfo` | 0 | info_ptr | 0 | Set scroll info |
| `sbGetPos` | 0 | 0 | position | Get scroll position |
| `sbSetContent` | 0 | content_ptr | 0 | Set content |
| `sbChanged` | MAKEDWORD(id,code) | new_pos | 0 | Scroll position changed |

### Slider

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `slSetRange` | 0 | range_ptr | 0 | Set range |
| `slGetRange` | 0 | range_ptr | 0 | Get range |
| `slSetCount` | count | 0 | 0 | Set handle count |
| `slSetPos` | handle | pos | 0 | Set position |
| `slGetPos` | handle | int* out | pos | Get position |
| `sliderValueChanged` | 0 | slider_ptr | 0 | Value changed |

### Toolbox

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `bxSetItems` | count | items_ptr | 0 | Set items |
| `bxSetActiveItem` | item_id | 0 | 0 | Set active item |
| `bxSetStrip` | 0 | strip_ptr | 0 | Set icon strip |
| `bxSetButtonSize` | size | 0 | 0 | Set button size |
| `bxLoadStrip` | tile_size | path_ptr | 0 | Load icon strip |
| `bxSetIconTintBrush` | br_index | 0 | 0 | Set icon tint |
| `bxClicked` | MAKEDWORD(id,code) | 0 | 0 | Item clicked |

### Grid

| Message | wparam | lparam | Return | Description |
|---------|--------|--------|--------|-------------|
| `grSetColors` | left_rgba | right_rgba | 0 | Set grid colors |

## Notification Codes (evCommand)

### HIWORD(wparam) values

| Code | Description |
|------|-------------|
| `btnClicked` | Button clicked |
| `cbSelectionChange` | ComboBox selection changed |
| `RVN_SELCHANGE` | TableView selection changed |
| `RVN_DBLCLK` | TableView double-click |
| `RVN_DELETE` | TableView delete requested |
| `edUpdate` | Edit text modified |
| `sbChanged` | ScrollBar position changed |
| `bxClicked` | Toolbox item clicked |
| `sliderValueChanged` | Slider value changed |
| `tbButtonClick` | Toolbar button clicked |
| `kMenuBarNotificationItemClick` | Menu item clicked |
| `kAcceleratorNotification` | Accelerator triggered |
| `ddxDataChanged` | DDX data changed |

## Key Flags (wparam for mouse messages)

```c
#define MK_LBUTTON   0x0001  // Left button down
#define MK_RBUTTON   0x0002  // Right button down
#define MK_SHIFT     0x0004  // Shift key
#define MK_CONTROL   0x0008  // Control key
#define MK_MBUTTON   0x0010  // Middle button down
```

## Scroll Bar Codes (wparam for evVScroll/evHScroll)

```c
#define SB_LINEUP        0
#define SB_LINEDOWN      1
#define SB_PAGEUP        2
#define SB_PAGEDOWN      3
#define SB_THUMBPOSITION 4
#define SB_THUMBTRACK    5
#define SB_ENDSCROLL     8
#define SB_HORZ          0
#define SB_VERT          1
```

## Dialog Result Codes

```c
#define IDOK       1
#define IDCANCEL   2
#define IDYES      6
#define IDNO       7
```

## MAKELONG Macro

```c
// Combine two 16-bit values
#define MAKELONG(low, high) ((LONG)(((WORD)(low)) | ((DWORD)((WORD)(high))) << 16))

// Example:
lparam = MAKELONG(x, y);  // Pack coordinates
int x = LOWORD(lparam);   // Unpack x
int y = HIWORD(lparam);   // Unpack y
```