# AGENTS.md — Fluctus Plasma Developer & Agent Guidelines

This document defines the architectural invariants, coding standards, and workflow rules for contributing to **Fluctus Plasma** (`com.kogical.fluctus`).

---

## 1. Architecture Overview

Fluctus Plasma is a compiled KDE Plasma 6.8+ `Plasma::Applet` with a strict three-layer separation:

- **`src/core/` (`fluctus_core`)** — Pure C++23 physics, math, and layout engine. **Zero QtQuick or KDE dependencies.** Must remain headless-testable via `test_dock_physics`.
- **`src/quick/` (`DockLayoutItem`, `DockAnimationDriver`, `BlurRegionController`)** — Qt Quick integration layer bridging `fluctus_core` to `QQuickItem`, `QAbstractItemModel`, and `KWindowEffects`.
- **`src/backend/` (`FluctusApplet`, `SmartLauncherBackend`, `TaskBackend`, `ShortcutManager`)** — Plasma/KF6 system integration (D-Bus SmartLauncher singleton, KService Jump Lists, global shortcut routing).
- **`package/contents/`** — Thin declarative QML shell (`main.qml`, `TaskDelegate.qml`, `TaskContextMenu.qml`, `ToolTipDelegate.qml`, `GroupDialog.qml`, Kirigami `Config*.qml`, and KConfigXT `main.xml`).

---

## 2. Hard Invariants (Do Not Break)

1. **Never bind `x`, `y`, `width`, `height`, or `scale` in `TaskDelegate.qml`.**
   - `DockLayoutItem` owns delegate lifecycle and drives `setPosition()` + `setScale()` directly from C++.
   - Never replace `DockLayoutItem`'s C++ delegate management with a QML `Repeater` (breaks ghost-slot removal animations and `rowsMoved` ordering).
2. **Never call `setSize()` per animation frame.**
   - Delegates have a fixed logical size (`effectiveDelegateLogicalSize`, updated only on config or container resize). Per-frame updates must only call `setPosition()`, `setScale()`, and `setOpacity()`.
3. **Animate strictly on the GUI thread.**
   - `DockAnimationDriver` (`QAbstractAnimation`) runs on the GUI thread and stops immediately when `DockPhysicsEngine::isSettled()` returns `true` (0% idle CPU). Never mutate `QQuickItem` from `QQuickWindow::beforeSynchronizing` (render thread).
4. **Never clobber Plasma's panel blur/contrast when disabled.**
   - `BlurRegionController` tracks `m_appliedBlur` and `m_appliedContrast` and only calls `KWindowEffects::enableBlurBehind(..., false)` / `enableBackgroundContrast(..., false)` when Fluctus itself previously enabled them.
   - In **Default (Panel / Panel Colorizer)** mode (`backgroundMode === 0`), `blurEnabled` must stay `false` so Plasma's `PanelView` and Panel Colorizer keep full control over the panel window.
5. **Structural-only implicit sizing.**
   - Update `implicitWidth`/`implicitHeight` only on model count, config, or container geometry changes — never on hover animation frames (prevents Wayland panel surface resize storms).
6. **Always use resting coordinates for drag-and-drop insertion indices.**
   - Call `DockLayoutItem::restingInsertionIndex(x, y)` so reordering under magnification never thrashes between adjacent slots.

---

## 3. C++23 & QML Coding Standards

- **C++23** (`CMAKE_CXX_STANDARD 23`, no GNU extensions).
- All targets build cleanly with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor`.
- **Memory safety**:
  - Use `std::unique_ptr` for non-`QObject` heap allocations; no raw `new`/`delete`.
  - Use `QPointer<T>` for all non-owning `QObject`/`QQuickItem`/`QWindow` observers and null-check before dereference.
  - Mark pure math helpers `constexpr` and `[[nodiscard]]`.
- **QML**:
  - Use KDE Plasma 6 / Qt 6 unversioned imports (`import QtQuick`, `import org.kde.kirigami as Kirigami`, `import org.kde.plasma.plasmoid`).
  - Prefer native KDE Plasma components (`PlasmaExtras.Menu` for context menus, `PlasmaCore.ToolTipArea` for tooltips, `Kirigami.FormLayout` + `Kirigami.Separator { Kirigami.FormData.isSection: true }` for config pages).
  - Never commit hardcoded absolute paths or system-specific hacks.

---

## 4. Adding New QML Files or Config Keys

Whenever you add a new QML file under `package/contents/`:
1. Add it to `FLUCTUS_QML_SOURCES` in [CMakeLists.txt](CMakeLists.txt).
2. Add a matching `set_source_files_properties(... PROPERTIES QT_RESOURCE_ALIAS <filename>)` entry so `plasma_add_applet` embeds it cleanly.

Whenever you add a new configuration setting:
1. Declare the `<entry>` with a sensible `<default>` in [package/contents/config/main.xml](package/contents/config/main.xml).
2. Expose `cfg_<name>` in the appropriate `Config*.qml` page (`ConfigAppearance.qml`, `ConfigPhysics.qml`, or `ConfigBehavior.qml`).
3. Bind `Plasmoid.configuration.<name>` in `main.qml`.

---

## 5. Build, Test & Verification Commands

- **Fast applet build**: `just build-applet` (skips test binaries; skips redundant CMake reconfigure).
- **Full build & unit tests**: `just test`
- **Sanitizer tests (ASan + UBSan)**: `just test-san`
- **Install / Update**: `just install` / `just update restart=no`
- **Force CMake reconfigure** (after adding files or changing options): `just reconfigure`

Always run `just test` (and `just test-san` after modifying `src/core/` or `src/quick/`) before considering a change complete.
