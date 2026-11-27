# Fluctus Plasma — Engineering & Design Docs

This directory contains technical architecture, physics math, and KDE Plasma 6 Wayland engineering documentation for **Fluctus Plasma** (`com.kogical.fluctus`).

> Looking for user documentation and installation instructions? See the root [README.md](../README.md).

## Documents

1. [architecture.md](architecture.md) — Target system design: compiled `FluctusApplet : Plasma::Applet`, C++ `DockLayoutItem` owning delegate lifecycle (no `Repeater`), GUI-thread animation loop, axis-agnostic 4-edge + RTL coordinate system, C++23 safety standards, unit tests, and `justfile` workflow.
2. [macos-dock-physics.md](macos-dock-physics.md) — Mathematical & UX analysis of the dock physics: continuous scale-density integral (zero feedback drift), closed-form magnification curves, exact slack bounds, critically-damped springs, latched bounce physics, and high-DPI scale-only rendering.
3. [plasma6-wayland-constraints.md](plasma6-wayland-constraints.md) — KDE Plasma 6.8 + Wayland engineering constraints: compiled `Plasma::Applet` (`plasma_add_applet`), panel headroom & bounce thickness formula, `PanelView` blur/mask interplay, `requestPublishDelegateGeometry`, `PW::LibTaskManager`, and global shortcut routing.
4. [wavetask-audit.md](wavetask-audit.md) — Historical research notes and Plasma 6 task manager packaging findings.

## Key Engineering Decisions

| Area | Decision |
| --- | --- |
| **Plasmoid ID & Packaging** | `com.kogical.fluctus`, built as a **compiled `Plasma::Applet`** via `plasma_add_applet` (installing into `${KDE_INSTALL_PLUGINDIR}/plasma/applets/`). |
| **C++ / QML Boundary** | **C++ `DockLayoutItem` owns delegate instantiation & animation**: manages delegates directly from `model` + `delegate` (no `Repeater`), steps physics on the **GUI thread** (`DockAnimationDriver`), and drives delegates via **`setPosition` + `setScale` only** (never per-frame `setSize`). QML is a thin declarative shell. |
| **Panel Scaling & Background** | Adaptive panel-aware scaling (`autoScaleToPanel` + cross/primary clamping) prevents Wayland clipping. **Default** background mode leaves panel surface and KWin blur untouched for Plasma themes & Panel Colorizer; **Custom Glass Pill** adds Liquid Glass, Frosted, Acrylic, and Clear Blur via `KWindowEffects::enableBlurBehind` + `enableBackgroundContrast`. |
| **Panel Orientations & RTL** | **Axis-agnostic 4-edge support** (Bottom, Top, Left, Right) + primary-axis direction sign for RTL / `reverseMode`. |
| **C++ Standard & Safety** | **C++23**, strict warnings (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor`), optional ASan/UBSan preset, `clang-tidy`, and headless `QtTest`/`CTest` unit tests. |
| **Build Efficiency** | Ninja presets, auto-detected `sccache`/`ccache` & `mold`/`lld`, precompiled headers (`FLUCTUS_USE_PCH`), conditional `fluctus_quick` test target, and incremental `just` recipes. |
