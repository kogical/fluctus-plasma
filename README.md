<div align="center">

<img src="docs/branding/logo.svg" alt="Fluctus Dock" width="128" />

# Fluctus Dock

**A smooth, native macOS-style animated task manager dock for KDE Plasma 6.8+**

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
- **Custom Glass & Blur Materials** — Switch to **Custom Glass Pill** for **Liquid Glass**, **Frosted Glass**, **Acrylic**, **Clear Blur**, or **Solid** surfaces with customizable tint, opacity, corner radius, borders, and padding — or pick **Plasma Theme Surface** / **Transparent**.
- **Full KDE Task Manager Integration**:
  - **Pin & Unpin** apps via the native KDE right-click context menu or by dragging apps directly onto the dock.
  - **Drag to Reorder** icons smoothly and drop files onto app icons to open them.
  - **Jump Lists**, virtual desktop switching, and window actions in the context menu.
  - **Live PipeWire Window Previews** & **MPRIS Media Controls** in hover tooltips.
  - **SmartLauncher** unread count badges and progress bars.
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
   - **Appearance**: Panel auto-scaling, icon size & spacing, **Default (Panel / Panel Colorizer)** vs. **Custom Glass Pill** (**Liquid Glass**, **Frosted**, **Acrylic**, **Clear Blur**), and icon reflections.
   - **Physics & Animation**: Magnification zoom, wave radius, falloff curve, and bounce height.
   - **Behavior**: Screen/desktop/activity filters, tooltips & live previews, click actions, and launcher ordering.

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
