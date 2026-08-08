# 3ds Max UI Architecture — Design Inspiration

Classic 3ds Max was not structured like a modern declarative UI framework. It was much closer to a **Win32 application with a custom docking/layout manager sitting above HWNDs**. Autodesk's old SDK exposes a surprising amount of this architecture.

## Classic 3ds Max: roughly this hierarchy

```text
3DSMAX main HWND
│
├── Menu bar
│
├── TOP docking region
│   ├── Main Toolbar CUIFrame
│   │    └── CustToolbar
│   │         ├── CustButton
│   │         ├── spinner
│   │         ├── combo
│   │         └── separators / custom HWNDs
│   ├── other toolbars
│   └── Ribbon / later additions
│
├── LEFT docking region
│   ├── toolbars
│   ├── Scene Explorer, etc.
│   └── optional panels
│
├── CENTER CLIENT AREA
│   └── View Panel
│        ├── Viewport HWND 0
│        ├── Viewport HWND 1
│        ├── Viewport HWND 2
│        └── Viewport HWND 3
│
├── RIGHT docking region
│   └── Command Panel CUIFrame
│        ├── Create
│        ├── Modify
│        ├── Hierarchy
│        ├── Motion
│        ├── Display
│        └── Utilities
│             └── RollupWindow
│                  ├── Rollup page
│                  ├── Rollup page
│                  └── Rollup page
│
└── BOTTOM region
     ├── Time Slider
     ├── Track Bar
     └── Status Panel
          ├── MAXScript Mini Listener
          ├── status line
          ├── prompt line
          ├── XYZ transform fields
          ├── animation controls
          └── viewport navigation controls
```

That is essentially what you see visually in Max. Autodesk's own UI overview divides it almost exactly this way: toolbars/ribbon at the top, Command Panel on the right, viewports in the middle, and the status/time/navigation machinery at the bottom. ([Autodesk Help][1])

## The important bit: `CUIFrameMgr`

The old architecture revolves around something called **CUI Frames**.

Autodesk describes a `CUIFrame` literally as the window that contains things such as:

> toolbars, menus, the command panel, etc.

There is one global `CUIFrameMgr`. It knows all of the dockable frames and manages the four docking regions:

```cpp
CUI_TOP_DOCK
CUI_BOTTOM_DOCK
CUI_LEFT_DOCK
CUI_RIGHT_DOCK
```

It exposes operations like:

```cpp
DockCUIWindow(hwnd, panel, rect);
FloatCUIWindow(hwnd, rect);
MinimizeCUIWindow(hwnd);

RecalcLayout();
```

So this was not some magical WinAPI feature. **Discreet/Autodesk built their own DockManager.** When something docks or changes size, `CUIFrameMgr::RecalcLayout()` recomputes the strips around the application and therefore the remaining rectangle available for the viewports. Autodesk explicitly says `RecalcLayout()` recalculates the top/bottom/left/right panels and, optionally, the whole application including viewports. ([Autodesk Help][2])

That is probably extremely relevant to your own renderer/editor because it is conceptually very simple:

```text
available = mainClientRect

layout TOP dock rows
available.top += topHeight

layout BOTTOM dock rows
available.bottom -= bottomHeight

layout LEFT dock columns
available.left += leftWidth

layout RIGHT dock columns
available.right -= rightWidth

viewportArea = available
```

Max's system is more sophisticated because there can be multiple rows, multiple frames per row, floating frames, locked frames, minimized frames, etc., but that is basically the algorithm.

Interestingly, the manager even exposes lookup by:

```cpp
GetICUIFrame(panel, rank, subrank)
```

which strongly indicates that the dock regions internally have something resembling **rows/columns (`rank`) and ordering inside the row (`subrank`)** rather than a generic constraint-layout tree. ([Autodesk Help][2])

## Toolbars are just another kind of CUI frame

Inside the frame is an `ICustToolbar`.

Autodesk's old `ICustToolbar` could contain push buttons, toggle buttons, flyouts, status controls, separators and arbitrary Windows/user-defined controls. It could be horizontal, vertical or multi-row. You retrieved it from its HWND:

```cpp
ICustToolbar *toolbar = GetICustToolbar(hwnd);
```

So the classic toolbar was basically:

```text
CUIFrame HWND
    ↓
CustToolbar HWND/control
    ↓
items
```

rather than each toolbar being hard-coded into the main window layout. ([Autodesk Help][3])

This explains why Max's toolbar system has always felt unusually freeform: buttons can be moved between toolbars, toolbars can become vertical, float, dock on another edge, etc.

The dock flags were also very explicit. Frames could say things like:

```text
CUI_TOP_DOCK
CUI_BOTTOM_DOCK
CUI_LEFT_DOCK
CUI_RIGHT_DOCK
CUI_FLOATABLE
CUI_MAX_SIZED
CUI_DONT_SAVE
CUI_HAS_MENUBAR
```

`CUI_MAX_SIZED`, for example, means the frame takes the entire dock row and nothing may sit next to it. ([Autodesk Help][4])

So essentially they built a very small predecessor to what later became things like `QMainWindow/QDockWidget`.

## The right-hand Command Panel is special, but not radically special

The Command Panel is itself managed as a dockable UI element; historically it could even be floated and docked left or right. Autodesk specifically says the Command Panel can be docked only on the left or right. ([Autodesk Help][5])

Inside it is another old Max-specific abstraction:

```cpp
IRollupWindow
```

The six top tabs switch the active task mode:

```cpp
TASK_MODE_CREATE
TASK_MODE_MODIFY
TASK_MODE_HIERARCHY
TASK_MODE_MOTION
TASK_MODE_DISPLAY
TASK_MODE_UTILITY
```

and the content underneath consists primarily of **rollup pages**.

The SDK exposes:

```cpp
GetCommandPanelRollup()
AddRollupPage(...)
ReplaceRollupPage(...)
```

Historically these pages were essentially Win32 dialogs/dialog templates embedded in Max's scrolling rollup container. ([Autodesk Help][6])

So the famous Max Modifier panel is approximately:

```text
CommandPanel
    ↓
current task mode
    ↓
IRollupWindow
    ↓
[ Modifier Stack ]
[ Parameters ▼ ]
[ Soft Selection ▼ ]
[ Edit Geometry ▼ ]
...
```

That extremely characteristic Max UI is therefore basically a **vertically scrolling accordion of embedded dialogs**.

This is one reason the old UI could be extended by plugins so easily: a modifier didn't have to understand Max's complete application layout. It basically supplied parameter UI/rollout pages.

## The viewports are real independent windows

This is another useful architectural detail.

A viewport isn't just a rectangle painted by the main window. Max exposes each viewport as a `ViewExp`, and:

```cpp
ViewExp::GetHWnd()
```

returns its HWND.

Even more interestingly, Autodesk says that this HWND is the **transparent input window that receives mouse input**, and it is distinct from:

```cpp
getGW()->getHWnd()
```

which is the window where the graphics are actually rendered. ([Autodesk Help][7])

So conceptually:

```text
Viewport
   ├── input HWND      ← mouse/buttons/etc.
   └── GraphicsWindow  ← Direct3D/OpenGL/Nitrous rendering
```

That's a very old-school but clever architecture.

The layout manager merely gives the viewport system the remaining center rectangle. The viewport manager subdivides *that* rectangle according to the current layout:

```text
+---------------+---------------+
|      TOP      |     FRONT     |
|               |               |
+---------------+---------------+
|      LEFT     |  PERSPECTIVE  |
|               |               |
+---------------+---------------+
```

Max currently documents 14 predefined subdivision configurations, and a scene remembers its viewport layout. ([Autodesk Help][8])

## The bottom area is multiple independent systems piled together

This is not one coherent status bar.

The traditional Max bottom area evolved into roughly:

```text
────────────────────────────────────────────
TIME SLIDER
────────────────────────────────────────────
TRACK BAR
────────────────────────────────────────────
MINI LISTENER | STATUS/PROMPT | XYZ | KEYING
────────────────────────────────────────────
           animation      viewport navigation
────────────────────────────────────────────
```

Autodesk itself distinguishes the **Time Slider**, **Track Bar**, and **Status Panel**. The Track Bar is explicitly dockable/floating, which tells you it participates in the UI docking infrastructure rather than merely being a piece of one status widget. ([Autodesk Help][9])

The Status Panel then contains another collection of controls: Mini Listener, prompt line, status line, selection lock, XYZ input, time controls, keying controls and viewport navigation. It can even be hidden independently through the `StatusPanel` interface. ([Autodesk Help][10])

That explains why the bottom looks like someone kept saying:

> we need one more permanent control down there.

for thirty years.

It is effectively **several horizontal mini-toolbars/panels stacked together**, rather than a carefully designed single component.

## How the layout was stored

Classic Max did **not** have the whole UI defined in something resembling XAML.

The executable/plugin code created the actual controls and CUI frames. Then a **CUI configuration file stored placement/customization state**.

Modern-ish classic Max uses `.cuix`; Autodesk says it stores **toolbar and panel layouts**. Menus, colors, mouse configuration etc. were stored separately. ([Autodesk Help][11])

So the model was closer to:

```text
C++ code
    defines:
        Main Toolbar exists
        Command Panel exists
        Track Bar exists
        buttons/actions exist
        allowed docking sides
        contents

CUI file
    defines:
        visible/hidden
        docked/floating
        side
        position
        row/order
        toolbar button arrangement
        floating rectangle
```

rather than:

```xml
<Window>
   <DockPanel>
       ...
   </DockPanel>
</Window>
```

That distinction is important.

## Then Autodesk moved the shell to Qt

The assumption that current Max is pure WinAPI is no longer correct.

**3ds Max 2018 switched the main application window to Qt.** Autodesk explicitly documents that pre-2018 the main HWND used the Windows class `3DSMAX`, while from 2018 onward the main window is a Qt window. ([Autodesk Help][12])

They replaced much of the old CUI frame infrastructure with:

```text
QmaxMainWindow  : QMainWindow
QmaxDockWidget : QDockWidget
QmaxToolBar    : QToolBar
```

`QmaxMainWindow` is explicitly described as Autodesk's replacement/extension of `QMainWindow` for the Max docking UI. It can enumerate attached toolbars/dock widgets and save/load a `.layout` docking layout. ([Autodesk Help][13])

But this is particularly funny:

> `QmaxToolBar::setWidget()` is used by 3ds Max to put a `QmaxDockingWinHost` containing a **legacy Win32 toolbar** inside the Qt toolbar.

So modern Max is to some extent:

```text
Qt shell
   ↓
QmaxToolBar / QmaxDockWidget
   ↓
Win32-host adapter
   ↓
25-year-old Max HWND control
```

Autodesk says exactly that in the SDK. ([Autodesk Help][14])

Which probably explains some of Max's UI oddities today.

## What to steal from old Max for SimpleSketch3D

The old Max architecture is actually much simpler than its appearance suggests. Copy the **structural idea**, not the accumulated Max UI:

```text
MainWindow
    DockManager
        TopDock[]
        BottomDock[]
        LeftDock[]
        RightDock[]

    centralView
```

Every UI chunk gets a tiny descriptor:

```cpp
struct DockPanel {
    const char *id;

    DockSide side;
    int row;
    int order;

    int preferredSize;
    int minSize;

    bool floatable;
    bool resizable;
    bool visible;

    Window *content;
};
```

Then the actual application definition could trivially be XML:

```xml
<Window id="main">

    <Toolbar id="main"
             dock="top"
             row="0"/>

    <Toolbar id="tools"
             dock="left"
             row="0"/>

    <Viewport id="scene"/>

    <Panel id="properties"
           dock="right"
           width="280"/>

    <Toolbar id="status"
             dock="bottom"
             row="0"/>

</Window>
```

That is basically **the old 3ds Max CUI architecture made declarative and cleaned up**.

And it fits this framework unusually well: you do not need Qt-style arbitrary nested layouts for the application shell. Four edge dock lists + one remaining client rectangle get you almost the complete structural layout of classic Max.

## References

[1]: https://help.autodesk.com/cloudhelp/2026/ENU/3DSMax-Basics/files/GUID-A62CEC88-5390-4CAF-97BB-C7D07EBC6F65.htm "Status Bar Controls"
[2]: https://help.autodesk.com/cloudhelp/2016/ENU/Max-SDK/cpp_ref/class_c_u_i_frame_mgr.html "CUIFrameMgr Class Reference"
[3]: https://help.autodesk.com/cloudhelp/2024/ENU/Max-Developer-Help/cpp_ref/class_i_cust_toolbar.html "3ds Max C++ API Reference: ICustToolbar Class Reference"
[4]: https://help.autodesk.com/cloudhelp/2026/ENU/MAXDEV-CPP-API-REF/group__cui_frame_position_types.html "3ds Max C++ API Reference: CUI Frame Position Types"
[5]: https://help.autodesk.com/cloudhelp/2016/ENU/3DSMax/files/GUID-DBC08DD5-F581-486A-BD08-56A115CD32ED.htm "Customize Display Right-Click Menu"
[6]: https://help.autodesk.com/cloudhelp/2025/ENU/MAXDEV-CPP-API-REF/class_interface.html "3ds Max C++ API Reference: Interface Class Reference"
[7]: https://help.autodesk.com/cloudhelp/2024/ENU/Max-Developer-Help/cpp_ref/class_view_exp.html "3ds Max C++ API Reference: ViewExp Class Reference"
[8]: https://help.autodesk.com/view/3DSMAX/2026/ENU/?guid=GUID-C461579F-BCB4-4822-87AE-E2B137C2D276 "3ds Max 2026 Help | Layout Panel | Autodesk"
[9]: https://help.autodesk.com/cloudhelp/2021/ENU/3DSMax-Basics/files/GUID-A55E4702-263E-4768-9964-5866698784DA.htm "Track Bar"
[10]: https://help.autodesk.com/cloudhelp/2021/ENU/MAXScript-Help/files/3ds-Max-Objects-and-Interfaces/Interfaces/Core-Interfaces/Core-Interfaces-Documentation/S/GUID-D62137AE-E500-4B42-983D-E7443E3DFF04.html "Interface: StatusPanel"
[11]: https://help.autodesk.com/cloudhelp/2024/ENU/3DSMax-Customizing/files/GUID-41CA005E-8B79-4823-A586-C27ABDDD1B2B.htm "Saving and Loading Custom User Interfaces"
[12]: https://help.autodesk.com/cloudhelp/2023/ENU/Max-Developer-Help/3ds_max_sdk_features/user_interface/3dsmax_main_window_hwnd.html "Getting the 3ds Max Main Window"
[13]: https://help.autodesk.com/cloudhelp/2025/ENU/MAXDEV-CPP-API-REF/class_max_s_d_k_1_1_qmax_main_window.html "3ds Max C++ API Reference: QmaxMainWindow Class Reference"
[14]: https://help.autodesk.com/cloudhelp/2026/ENU/MAXDEV-CPP-API-REF/class_max_s_d_k_1_1_qmax_tool_bar.html "3ds Max C++ API Reference: QmaxToolBar Class Reference"
