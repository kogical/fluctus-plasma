# AGENTS.md — Fluctus Developer & Agent Guidelines

The architectural invariants, coding standards and workflow rules for contributing to
**Fluctus**, a monorepo of KDE Plasma extensions. Repository layout, shared caches and
justfile conventions: [docs/development.md](docs/development.md).

---

## 1. Architecture Overview

| Path | What | License |
| --- | --- | --- |
| `dock/` | Fluctus Dock, Plasma 6.8+ applet `com.kogical.fluctus.dock` | Apache-2.0 |
| `glass/` | Fluctus Glass, KWin effect `fluctusglass` | GPL-3.0-or-later |
| `libs/glassclient/` | `Fluctus::GlassClient` + `Kogical.FluctusGlass` QML module (D-Bus client of the effect) | Apache-2.0 |
| `libs/liquidglass/` | `Kogical.LiquidGlass` Qt Quick material | Apache-2.0 |
| `crates/liquidglass/` | Rust rendering engine linked by the effect (`gl` + `ffi` features) | Apache-2.0 |

Fluctus Dock is a compiled `Plasma::Applet` with a strict three-layer separation (paths under `dock/`):

- **`src/core/` (`fluctus_core`)** — Pure C++23 physics, math, and layout engine. **Zero QtQuick or KDE dependencies.** Must remain headless-testable via `test_dock_physics`.
- **`src/quick/` (`DockLayoutItem`, `DockAnimationDriver`, `BlurRegionController`, `DockIconItem`)** — Qt Quick integration layer bridging `fluctus_core` to `QQuickItem`, `QAbstractItemModel`, and `KWindowEffects`.
- **`src/backend/` (`FluctusApplet`, `SmartLauncherBackend`, `TaskBackend`, `ShortcutManager`)** — Plasma/KF6 system integration (D-Bus SmartLauncher singleton, KService Jump Lists, global shortcut routing).
- **`package/contents/`** — Thin declarative QML shell (`main.qml`, `TaskDelegate.qml`, `TaskContextMenu.qml`, `ToolTipDelegate.qml`, `GroupDialog.qml`, `TaskGroupWindows.qml`, Kirigami `Config*.qml`, and KConfigXT `main.xml`).

Fluctus Glass is a thin KWin adapter (`glass/src/`): window/shape selection, global mode and the D-Bus service; all rendering goes through `crates/liquidglass`.

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
7. **Never keep a `QModelIndex` of TasksModel across model changes in QML.**
   - Rebuild it from the row (`tasksModel.makeModelIndex(row[, child])`) when you use it. Read a group's windows through `TaskGroupWindows.qml`, never a `DelegateModel` rooted at a stored index (it goes stale when TasksModel regroups).
8. **Never link GPL code from `libs/` or `crates/`.**
   - They must stay usable by any app; they talk to the effect over D-Bus (`GlassProtocol.hpp` is the only shared definition).
9. **Never overwrite an installed plugin in place.**
   - plasmashell and KWin keep plugins memory-mapped. Install through the justfiles, which unlink the previous files first.

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
- **Rust**: edition 2024 in the root Cargo workspace; keep `just liquidglass lint` (clippy) clean and the C ABI in `include/liquidglass.h` regenerated (`just liquidglass header`) whenever `ffi.rs` changes.
- **Every source file** starts with SPDX headers (`SPDX-FileCopyrightText` + `SPDX-License-Identifier`): Apache-2.0, or GPL-3.0-or-later under `glass/`.

---

## 4. Adding New QML Files or Config Keys

Whenever you add a new QML file under `dock/package/contents/`:
1. Add it to `FLUCTUS_QML_SOURCES` in [dock/CMakeLists.txt](dock/CMakeLists.txt). Every file in that list gets its `QT_RESOURCE_ALIAS` automatically, so `plasma_add_applet` embeds it cleanly.

Whenever you add a new configuration setting:
1. Declare the `<entry>` with a sensible `<default>` in [dock/package/contents/config/main.xml](dock/package/contents/config/main.xml).
2. Expose `cfg_<name>` in the appropriate `Config*.qml` page (`ConfigAppearance.qml`, `ConfigPhysics.qml`, or `ConfigBehavior.qml`).
3. Bind `Plasmoid.configuration.<name>` in `main.qml`.

---

## 5. Build, Test & Verification Commands

Run from the repository root (`just` lists everything; `cd dock && just test` works too).

- **Fast dock build**: `just dock build` (only the applet target)
- **Everything, with all tests**: `just test` (Rust engine + every C++ test)
- **Sanitizer tests (ASan + UBSan)**: `just test-san`
- **Lint**: `just lint` (clang-tidy + clippy)
- **What CI runs**: `just ci`
- **Install / Update**: `just install` / `just update restart=no` (or per component: `just dock update`, `just glass update`)
- **Force CMake reconfigure** (after adding files or changing options): `just reconfigure`

Always run `just test` (and `just test-san` after modifying `dock/src/core/`, `dock/src/quick/`, `glass/src/` or `libs/`) before considering a change complete.
