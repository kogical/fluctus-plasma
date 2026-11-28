<div align="center">

<img src="docs/branding/logo.svg" alt="Fluctus Dock" width="128" />

# Fluctus Dock

**A smooth, native macOS-style animated task manager dock for KDE Plasma 6.8+**

[![CI](https://github.com/kogical/fluctus-plasma/actions/workflows/ci.yml/badge.svg)](https://github.com/kogical/fluctus-plasma/actions/workflows/ci.yml)
[![KDE Plasma](https://img.shields.io/badge/KDE%20Plasma-6.8%2B-1D99F3?logo=kde&logoColor=white)](https://kde.org/plasma-desktop/)
[![Wayland](https://img.shields.io/badge/Wayland-Native-FFBC00?logo=wayland&logoColor=black)](https://wayland.freedesktop.org/)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

</div>

---

## Features

- **Fluid Dock Magnification & Bounce** — Smooth hover magnification wave (Raised Cosine, Quintic Smootherstep, or Gaussian curves) and gravity bounce for launching or attention-demanding apps, with 0% idle CPU usage when at rest.
- **Adapts to Your Plasma Panel** — Automatically scales icons and zoom headroom to fit the surrounding KDE Plasma panel on any screen edge (Bottom, Top, Left, or Right), preventing Wayland clipping and overflowing.
- **Panel Colorizer & Theme Friendly** — Ships in **Default (Panel Managed)** mode out of the box so your Plasma panel theme or tools like **Panel Colorizer** can style the background and blur natively.
- **Custom Glass & Blur Materials** — Switch to **Glass pill** for **Liquid Glass** (lens bezel, angled rim light; needs the [Glass KWin effect](https://github.com/4v3ngR/kwin-effects-glass) for real refraction, e.g. `yay -S kwin-effects-glass-git` on Arch), **Frosted Glass**, **Acrylic** (with grain), **Clear Glass**, or **Solid** surfaces with tint (theme / custom / accent), opacity, corner radius, outline style (single, dual hairline, accent glow), drop shadow and rim highlight — or pick **Plasma theme surface** (now blurred) / **Transparent**. Rendered by the reusable [`Kogical.LiquidGlass`](modules/liquidglass/README.md) Qt Quick module.
- **Presets** — One-click styles (**macOS Tahoe**, **macOS Sequoia**, **Glass Capsule**, **Plasma Native**, **Midnight Solid**, **Neon Aurora**, **Paper**, **Floating Icons**) with live previews; choose which parts to apply (look, indicators, spacing, panel background, physics).
- **Hide Plasma panel background** — Remove the panel's own frame, blur and shadow so only the dock is visible; restored automatically when turned off or when the widget is removed.
- **Full KDE Task Manager Integration**:
  - **Pin & Unpin** apps via the native KDE right-click context menu (including per-activity pinning), by dragging apps onto the dock, or by dragging pinned icons off the dock.
  - **Smooth Spring Drag Reordering** with a live insertion guide, plus file drop-to-open and hover-to-raise on minimized windows/groups.
  - **Clickable Audio / Mute Badges** for apps playing sound, **SmartLauncher** unread counts & progress bars, and customizable running indicator styles.
  - **Jump Lists**, virtual desktop switching, and window actions in the context menu.
  - **Live PipeWire Window Previews** & **MPRIS Media Controls** in hover tooltips.
  - **Group Window Picker** for multi-window apps and **Meta+1..9** shortcut support.

---

## Requirements

- **KDE Plasma 6.8+** (Wayland recommended)
- **Qt 6.8+** (`Core`, `Gui`, `Qml`, `Quick`, `DBus`, `Test`)
- **KDE Frameworks 6** (`ECM`, `CoreAddons`, `I18n`, `WindowSystem`, `GlobalAccel`, `Service`, `KIO`)
- **LibTaskManager** (`plasma-workspace` development headers)
- **CMake 3.28+**, **Ninja**, and a C++23 compiler (GCC 13+ or Clang 17+)
- Optional: [`just`](https://github.com/casey/just), `lld`/`mold`, `ccache`/`sccache` for faster builds

---

## Installation

### Quick Install with `just`

```bash
git clone https://github.com/kogical/fluctus-plasma.git
cd fluctus-plasma
just install
```

### Manual Build & Install (CMake)

```bash
cmake --preset release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build --preset release
sudo cmake --install build/release
```

### Arch Linux (`PKGBUILD`) & Nix (`flake.nix`)

- **Arch Linux**: `cd packaging/arch && makepkg -si`
- **Nix Flakes**: `nix build` (or `nix develop` for a dev shell)

### Updating & Uninstalling

```bash
# Rebuild, prune stale files, reinstall, and optionally restart plasmashell
just update

# Non-interactive update (auto-restart or skip restart)
just update restart=yes
just update restart=no

# Cleanly uninstall all installed files
just uninstall
```

---

## Getting Started

1. Right-click your KDE Plasma panel → **Enter Edit Mode** → **Add Widgets…**
2. Search for **Fluctus Dock** and drag it onto your panel (or add the bundled **Fluctus Dock Panel** layout template via *Add Panel*).
3. Right-click the dock → **Configure Fluctus Dock…** to customize:
   - **Presets**: Ready-made styles with live previews. Tick which parts to apply (look, indicators, spacing, panel background, magnification).
   - **Appearance**: Live preview, the **Hide the panel behind the dock** switch, background (None, Glass, Plasma theme, Icons only), material, color, shape and edges, and running-app indicators.
   - **Size & Spacing**: A panel height calculator (Small, Medium, Large or a custom icon size, with a one-click **Use this height**), icon size and spacing, and padding.
   - **Magnification**: Magnified size (shown in pixels), wave width and shape, and launch bounce.
   - **Behavior**: Which windows appear, clicking, pinned apps, hover tooltips and previews, and badges and alerts.

> **Using Panel Colorizer?** Keep **Background mode** set to **Default (Panel / Panel Colorizer)** — Fluctus will not draw its own pill or override KWin's panel blur.

---

## Development & Documentation

Run the unit test suite:

```bash
just test       # Release tests
just test-san   # Debug + ASan/UBSan tests
```

For architecture notes, dock physics math, and Plasma 6 Wayland engineering details, see the [`docs/`](docs/README.md) directory and [`AGENTS.md`](AGENTS.md).

---

## License

Fluctus Dock is licensed under the [MIT License](LICENSE).
