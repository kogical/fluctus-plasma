# WaveTask (`org.vicko.wavetask`) Technical Audit

Source repository: [github.com/vickoc911/org.vicko.wavetask](https://github.com/vickoc911/org.vicko.wavetask) (Plasma 6.6+ compiled fork; previously `org.kde.plasma.wavetask` for Plasma ≤ 6.5).

## 1. Repository & Architecture Overview

WaveTask is a fork of KDE's built-in `org.kde.plasma.taskmanager` plasmoid with macOS-style dock magnification, reflections, bouncing icons, a PNG skin loader, and a `CairoPenguin` Easter egg layered on top.

```text
org.vicko.wavetask/
├── CMakeLists.txt                       # C++23, ECM, Plasma, KWin, installs global QML plugin + package + layout-template
├── layout-templates/contents/layout.js  # Pre-configured 86px transparent FitContent bottom panel
├── package/
│   ├── metadata.json                    # KPackageStructure: Plasma/Applet, Provides: org.kde.plasma.multitasking
│   ├── contents/config/main.xml         # KConfigXT schema (taskmanager defaults + dock zoom/skin entries)
│   ├── contents/skins/*                 # 10 PNG skin folders (Big Sur, Tahoe, Vidrio, etc.)
│   ├── contents/assets/*                # Sprite sheets for CairoPenguin
│   └── contents/ui/                     # ~20 QML files + 3 JS files (main.qml ~1342 LOC, Task.qml ~982 LOC)
└── plugin/
    ├── CMakeLists.txt                   # Builds libwavetaskplugin.so into ${KDE_INSTALL_QMLDIR}/org/vicko/wavetask
    ├── plugin.cpp                       # QQmlExtensionPlugin boilerplate
    ├── backend.{h,cpp}                  # ~953 LOC: KDE taskmanager Backend + setBlurBehind + KGlobalAccel + evdev Meta reader
    └── smartlauncher{backend,item}.{h,cpp}  # Unity/DBus LauncherEntry badge counts, progress bars, urgency
```

### Why WaveTask Must Be Compiled on Plasma 6.6+
Starting in **Plasma 6.6**, KDE stopped exposing `org.kde.plasma.private.taskmanager` as a public QML import for third-party applets, and Plasma 6.8 ships its own task manager as a compiled `Plasma::Applet` (`plugins/plasma/applets/org.kde.plasma.taskmanager.so`). Any third-party task manager on Plasma 6.6–6.8+ must compile C++ code linking against `PW::LibTaskManager` and `KF6::*` for:
- Jump list actions, KIO places, and recent documents (`KActivities::Stats`)
- Smart Launcher DBus integration (badges, progress overlays, urgency)
- Global keyboard shortcuts (`KF6::GlobalAccel`, e.g., Meta+1..9)
- Window-level effects (`KWindowEffects::enableBlurBehind`)

## 2. How WaveTask Implements Key Effects (and Its Weaknesses)

### A. Magnification (`package/contents/ui/Task.qml`)
- **Formula:** Truncated Gaussian evaluated per-task in a QML JavaScript binding:
  ```js
  zoomFactor = 1.0 + (_zoom * entryProgress) * Math.exp(-(distance * distance) / (2 * _sigma * _sigma));
  ```
  where `_sigma = iconSize * amplitud`, cutoff at `distance > 3 * _sigma`.
- **Entry/Exit Smoothing:** `entryProgress` is a QML `NumberAnimation` (`200ms`, `Easing.OutCubic`), and `dockRef.smoothMouse` tracks the pointer.
- **Weaknesses to fix in Fluctus:**
  1. **Per-frame QML JS binding churn:** Every `Task` delegate re-evaluates JS `zoomFactor`, 4 `iconAlign` margin bindings, `scale`, `bounceCompensation`, and `backgroundItem` width on every pointer move.
  2. **Hard cutoff at `3σ`:** Truncating a Gaussian leaves a non-zero step (`e^-4.5 ≈ 0.011 * zoom`) at the influence boundary.
  3. **Spacing-blind center math:** `iconCenter = centerOffset + (index * _baseSize) + (_baseSize / 2)` (`Task.qml:~128`) ignores `taskList.spacing`, so pointer-to-icon drift grows linearly toward the ends of the dock.
  4. **Inconsistent vertical vs. horizontal & reverse handling:** Numerous `if (tasksRoot.vertical)` / `isTopPanel` / `isLeftPanel` branches with magic `Kirigami.Units.smallSpacing * 0.6` / `0.8` fudge factors, plus `rotation: 180` (`main.qml:34`) to fake `reverseMode` on vertical panels (requiring every child to counter-rotate by 180°).

### B. High-DPI Icon Rendering Trick (Worth Keeping, Refined)
WaveTask renders icons at `baseRenderSize = iconSize * 2` and scales them down at rest (`scale: 1 / (baseRenderSize / width)`), avoiding blurry upscaling during zoom. Fluctus will generalize this to `ceil(iconSize * maxZoomScale)` with mipmapping (`layer.mipmap`) for downscales above ~2× and optional native-size crispness when settled at rest (`α = 0`).

### C. Panel Headroom, Transparency & Blur (`layout.js`, `main.qml`, `backend.cpp`)
- `layout-templates/contents/layout.js` configures a bottom panel: `height = 86`, `lengthMode = 1` (`FitContent`), `alignment = "center"`, `hiding = "dodgewindows"`, `backgroundHints = 4`, `userBackgroundHints = 4`, `opacityMode = 1`, `floating = 0`.
- `Backend::setBlurBehind` constructs a rounded-rectangle `QRegion` and calls `KWindowEffects::enableBlurBehind`.
- Because Plasma's `PanelView` overwrites blur on resize/theme/edit-mode changes, WaveTask has to re-apply blur in two places (`main.qml:740`, `:899`).
- **Weaknesses to fix:**
  - `86 px` panel height is too short to fit both `1.8×` magnification and a `0.6×` bounce without clipping.
  - `main.qml:103-107` walks `parent` string-matching `"ContainmentItem_QML"` (`lookForContainer`), which is brittle across Plasma updates — adopting a compiled `Plasma::Applet` gives direct C++ access to the containment and window.

### D. Bounce Animation (`Task.qml`)
- Triggered on `model.IsStartup || model.IsDemandingAttention || smartLauncherItem.urgent`.
- Uses a QML `SequentialAnimation` with `jumpHeight = iconSize * zoomFactor * 0.6`.
- **Weakness to fix:** Because `jumpHeight` depends dynamically on `zoomFactor`, hovering/unhovering mid-hop makes the bounce arc jump abruptly. Fluctus will latch hop height at takeoff and clamp it to remaining panel headroom.

### E. Global Shortcuts & Meta-Hold Overlay (`backend.cpp`)
- Registers shortcuts under a separate `"Wave Task Manager"` `KGlobalAccel` component, which still conflicts with `[plasmashell]` in `~/.config/kglobalshortcutsrc` (only one owner receives Meta+1).
- Uses `static std::once_flag` with lambdas calling `findTargetBackend()`, risking dangling pointers when applet instances are destroyed.
- **Security/permissions anti-pattern (DO NOT PORT):** WaveTask's `showOnMetaKey` / `showTaskNumbersOnMeta` (`backend.cpp` ~L195–333) opens every `/dev/input/event*` device and reads raw evdev key events directly. This only works if the user is in the `input` group and behaves like a keylogger. Fluctus will **never** read `/dev/input`; instead, we will show timed index badges (~1.5 s) after Meta+N presses plus an optional toggle shortcut.

### F. Minimize Geometry (`Task.qml:226`, `main.qml:278`)
- WaveTask calls `tasksModel.requestPublishDelegateGeometry` so KWin knows where to animate minimizing windows. Fluctus must preserve this using each task's **resting** slot rect in global coordinates (never the magnified rect), republishing on settle or panel move.

## 3. Feature & Config Key Parity Matrix (WaveTask → Fluctus)

| Feature / Config Key | WaveTask | Fluctus Target | Notes |
| --- | --- | --- | --- |
| Core window management (`TasksModel`) | Yes | **Yes** | Screen/desktop/activity filtering, sorting, pinning, grouping |
| Smooth macOS magnification wave | Yes (QML JS) | **Yes (C++ engine)** | Closed-form integral, configurable curve, scale, radius, spring |
| Bouncing icons (launch / attention) | Yes | **Yes (all 4 edges, C++)** | Parabolic gravity physics, latched & clamped hop height |
| Optional icon reflection | Yes | **Yes (toggleable)** | Axis-aware reflection with gradient fade & bounce compensation |
| Rounded blur-behind dock pill | Yes (per-skin) | **Yes (Plasma/glass pill)** | Clean theme-integrated pill, no PNG skin packs |
| PipeWire window preview tooltips | Yes | **Yes** | Thin QML delegates reusing KDE's `PipeWireThumbnail` / `ToolTipInstance` |
| PulseAudio stream & mute badge | Yes | **Yes** | `indicateAudioStreams`, `interactiveMute` |
| MPRIS media controls in tooltips | Yes | **Yes** | `tooltipControls` + `PlayerController` |
| Smart Launcher badges & progress | Yes | **Yes (C++ singleton)** | Shared process-wide DBus backend |
| Jump lists, Places, Recent Docs | Yes | **Yes (C++ backend)** | KF6/KIO/KActivitiesStats |
| Drag-and-drop reorder & URL drop | Yes | **Yes** | Resting-coord insertion index + `DropArea` pointer feed |
| Minimize animation geometry | Yes | **Yes** | `requestPublishDelegateGeometry` with resting global rects |
| Meta+1..9 shortcuts & `activateTaskAtIndex` | Yes | **Yes (C++ router)** | Reuse stock action names (verified on 6.8), `QPointer` router |
| `unhideOnAttention` | Yes | **Yes** | Sets `Plasmoid.status = NeedsAttentionStatus` |
| `middleClickAction`, `minimizeActiveTaskOnClick` | Yes | **Yes** | Full click behavior parity |
| `wheelEnabled`, `wheelSkipMinimized` | Yes | **Yes** | Mouse wheel task switching |
| `highlightWindows` | Yes | **Yes** | KWin `HighlightWindow` DBus call on tooltip hover |
| `separateLaunchers`, `hideLauncherOnStart` | Yes | **Yes** | Passed directly to `TasksModel` |
| `onlyGroupWhenFull`, `groupedTaskVisualization` | Yes | **Yes** | Grouping + `GroupDialog` / tooltip cycling |
| `reverseMode` & RTL | Yes (`rotation: 180`) | **Yes (AxisGeometry sign)** | Native primary-axis sign, no 180° rotation hack |
| `maxStripes`, `forceStripes` | Yes (grid) | **Single-stripe dock** | macOS docks are single-row/column; omit multi-stripe grid complexity |
| Bundled Panel layout template | Yes | **Yes** | `KPackageStructure: Plasma/LayoutTemplate`, computed headroom |
| PNG Skin Pack system (10 skins) | Yes | **Dropped** | Replaced by configurable Plasma/glass dock pill |
| `CairoPenguin` Easter egg | Yes | **Dropped** | Out of scope |
| `/dev/input/event*` Meta-hold reader | Yes | **Dropped** | Security/permissions hazard; replaced by timed badges + toggle shortcut |
