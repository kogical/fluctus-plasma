# Fluctus Plasma (`com.kogical.fluctus-plasma`) — Research & Architecture Docs

This directory consolidates Stage 1 research for **Fluctus Plasma**: a clean, modern C++23 implementation of a macOS-style animated task manager plasmoid for **KDE Plasma 6.8+**, targeting core feature parity with [WaveTask (`org.vicko.wavetask`)](https://github.com/vickoc911/org.vicko.wavetask) while eliminating its QML/JS bottlenecks, Wayland glitches, and fragile hacks.

## Documents

1. [wavetask-audit.md](wavetask-audit.md) — Deep technical audit of `org.vicko.wavetask`: architecture, features, Plasma 6.6+ plugin lockdown, exact magnification/blur/bounce/evdev mechanics, config key parity decisions, and what we keep vs. drop.
2. [macos-dock-physics.md](macos-dock-physics.md) — Mathematical & UX analysis of the macOS Dock: continuous scale-density integral (zero feedback drift), closed-form magnification curves, exact slack bounds, critically-damped springs, latched bounce physics, and high-DPI scale-only rendering.
3. [plasma6-wayland-constraints.md](plasma6-wayland-constraints.md) — KDE Plasma 6.8 + Wayland realities of running inside a Plasmoid: compiled `Plasma::Applet` (`plasma_add_applet`), panel headroom & bounce thickness formula, `PanelView` blur/mask interplay, `requestPublishDelegateGeometry`, `PW::LibTaskManager`, stock Meta+1..9 shortcut reuse, and local SDK findings.
4. [architecture.md](architecture.md) — Target system design: compiled `FluctusApplet : Plasma::Applet`, C++ `DockLayoutItem` owning delegate lifecycle (no `Repeater`), GUI-thread animation loop, axis-agnostic 4-edge + RTL coordinate system, C++23 safety standards, unit tests, and `justfile` workflow.

## Agreed Design Decisions

| Area | Decision |
| --- | --- |
| **Plasmoid ID & Packaging** | `com.kogical.fluctus-plasma`, built as a **compiled `Plasma::Applet`** via `plasma_add_applet` (installing into `${KDE_INSTALL_PLUGINDIR}/plasma/applets/`, avoiding global QML namespace pollution). *Note: verified in `PlasmaMacros.cmake` that `plasma_add_applet` derives `URI "plasma.applet.${id}"`; we will confirm Qt6 accepts a hyphen in internal `QT_NO_PLUGIN` URIs during Stage 2 scaffolding and fall back to `com.kogical.fluctus` if `qt_add_qml_module` rejects it.* |
| **Feature Scope** | Core Plasma task manager parity (window management, grouping, PipeWire tooltips, PulseAudio mute indicator, MPRIS controls, Smart Launcher badges/progress, drag-and-drop pinning/reordering, KWin minimize geometry, Meta+1..9 shortcuts) + macOS dock physics (zoom, bounce, optional reflection, clean Plasma/glass dock pill). **Explicitly dropped:** PNG skin packs, `CairoPenguin`, and WaveTask's `/dev/input/event*` evdev keylogger. |
| **C++ / QML Boundary** | **C++ `DockLayoutItem` owns delegate instantiation & animation**: manages delegates directly from `model` + `delegate` (no `Repeater`), steps physics on the **GUI thread** (`QAbstractAnimation` / `afterAnimating` + `updatePolish`), and drives delegates via **`setPosition` + `setScale` only** (never per-frame `setSize`). QML is a thin declarative shell for delegates, tooltips, context menus, and configuration UI. |
| **Panel Overflow Strategy** | **Transparent-headroom panel + clean dock pill**: panel thickness sized from an explicit zoom + bounce headroom formula, dock pill rendered at resting height with rounded `KWindowEffects::enableBlurBehind`, pre-allocated primary-axis slack `(M - 1)R` per side, and a bundled `Plasma/LayoutTemplate`. |
| **Panel Orientations & RTL** | **Axis-agnostic 4-edge support** (Bottom, Top, Left, Right) + primary-axis direction sign for RTL / `reverseMode` from day one; zoom and bounce grow away from the screen edge via per-edge `transformOrigin`. |
| **C++ Standard & Safety** | **C++23**, strict warnings (`-Wall -Wextra -Wpedantic -Wconversion` with Qt headers as `SYSTEM`), optional ASan/UBSan debug preset, `clang-tidy`, and headless `QtTest`/`CTest` unit tests for `fluctus_core`. |
| **Build & Install Workflow** | `justfile` with `build`, `test`, `install`, `uninstall`, `update` (`restart=ask\|yes\|no`), using `-DKDE_INSTALL_USE_QT_SYS_PATHS=ON`, persisting `install_manifest.txt` in `$XDG_STATE_HOME/fluctus-plasma/`, and pruning stale files on update. Optional `PKGBUILD` noted for Arch/CachyOS. |

## Stage 2 Implementation Roadmap (High-Level)

1. **Scaffold & Tooling**: `CMakeLists.txt` (`plasma_add_applet`), `CMakePresets.json` (`release`, `debug`, `debug-asan-ubsan`), `.clang-tidy`, `justfile`, `metadata.json` (`X-Plasma-Provides: org.kde.plasma.multitasking`), and loadable `FluctusApplet` skeleton.
2. **Pure C++23 Physics & Layout Engine (`fluctus_core`)**: Closed-form scale-density integral, magnification curves, critically/under-damped spring solver (handling `|ζ - 1| < 1e-4`), latched parabolic bounce solver, 4-edge + RTL projection, and full `QtTest` suite under ASan/UBSan.
3. **C++ Qt Quick & Plasma Layer (`src/quick/`, `src/backend/`)**: `FluctusApplet`, `DockLayoutItem` (delegate lifecycle, ghost slots, GUI-thread animation driver, passive pointer/drag input, `requestPublishDelegateGeometry`), `BlurRegionController` (re-applying on `PanelView` resets), process-singleton `SmartLauncherBackend`, `ShortcutManager` router, and Jump List / Places / Recent Docs actions.
4. **Thin QML Plasmoid Shell**: `main.qml` (`Plasmoid.constraintHints: CanFillArea`, `activateTaskAtIndex`, edit-mode freeze), `TaskDelegate.qml` (fixed logical size + mipmapped icon + reflection), `ToolTipDelegate.qml` (PipeWire + MPRIS), `ContextMenu.qml`, `GroupDialog.qml`, `MouseHandler.qml` (DnD + wheel), and Kirigami `Config*.qml` pages.
5. **Panel Layout Template & Verification**: Bundled `layout-templates/` (`KPackageStructure: Plasma/LayoutTemplate`), multi-edge + fractional-scaling verification on Plasma 6.8 Wayland, and default physics tuning.
