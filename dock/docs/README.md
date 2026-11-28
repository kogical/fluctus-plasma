# Fluctus Dock: engineering docs

The deep material behind [Fluctus Dock](../README.md) (applet id `com.kogical.fluctus.dock`).
The rules that must not be broken are in [AGENTS.md](../../AGENTS.md).

1. [architecture.md](architecture.md): the compiled `FluctusApplet : Plasma::Applet`, the C++
   `DockLayoutItem` that owns delegates (no `Repeater`), the GUI-thread animation loop, the
   axis-agnostic 4-edge + RTL coordinate system, C++23 standards, tests and the justfile.
2. [macos-dock-physics.md](macos-dock-physics.md): the scale-density integral (no feedback
   drift), closed-form magnification curves, exact slack bounds, critically damped springs,
   latched bounce physics, and scale-only rendering on high-DPI screens.
3. [plasma6-wayland-constraints.md](plasma6-wayland-constraints.md): `plasma_add_applet`, panel
   headroom and the bounce-thickness formula, how `PanelView` blur and masks interact,
   `requestPublishDelegateGeometry`, `PW::LibTaskManager` and global shortcut routing.
4. [wavetask-audit.md](wavetask-audit.md): historical research notes on another Plasma dock.

## Key decisions

| Area | Decision |
| --- | --- |
| **Packaging** | A **compiled `Plasma::Applet`** built with `plasma_add_applet`, installed to `${KDE_INSTALL_PLUGINDIR}/plasma/applets/`. Before the monorepo split the id was `com.kogical.fluctus`; `just dock update` migrates installs and panels. |
| **C++ / QML boundary** | **C++ `DockLayoutItem` owns delegate creation and animation.** It manages delegates straight from `model` + `delegate` (no `Repeater`), steps physics on the **GUI thread** (`DockAnimationDriver`), and moves delegates with **`setPosition` + `setScale` only**, never a per-frame `setSize`. QML is a thin declarative shell. |
| **Panel scaling & background** | Panel-aware scaling (`autoScaleToPanel` plus cross/primary clamping) prevents Wayland clipping. **Default** mode leaves the panel surface and KWin blur alone; the glass materials use `KWindowEffects` blur + contrast, and real refraction through [Fluctus Glass](../../glass/README.md). |
| **Orientation & RTL** | **Axis-agnostic, all 4 edges**, plus a primary-axis direction sign for RTL and `reverseMode`. |
| **Grouped tasks** | Tooltips and the window picker read a group's windows by TasksModel row (`TaskGroupWindows.qml`) instead of a `DelegateModel` rooted at a stored `QModelIndex`, which goes stale when TasksModel regroups. |
| **C++ standard & safety** | **C++23**, strict warnings, an ASan/UBSan preset, `clang-tidy`, and headless QtTest/CTest unit tests (label `dock`). |
