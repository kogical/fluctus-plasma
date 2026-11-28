<div align="center">

<img src="docs/branding/logo.svg" alt="Fluctus" width="128" />

# Fluctus

**A fluid dock and real Liquid Glass for KDE Plasma 6.8+**

[![CI](https://github.com/kogical/fluctus/actions/workflows/ci.yml/badge.svg)](https://github.com/kogical/fluctus/actions/workflows/ci.yml)
[![KDE Plasma](https://img.shields.io/badge/KDE%20Plasma-6.8%2B-1D99F3?logo=kde&logoColor=white)](https://kde.org/plasma-desktop/)
[![Wayland](https://img.shields.io/badge/Wayland-Native-FFBC00?logo=wayland&logoColor=black)](https://wayland.freedesktop.org/)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache--2.0-green.svg)](LICENSE)

</div>

Fluctus is a small family of KDE Plasma extensions that share one goal: a desktop that moves
smoothly and looks like it is made of glass, without costing you a single idle CPU cycle.

| | What it is | Read more |
| --- | --- | --- |
| **Fluctus Dock** | A task manager for your panel with macOS-style magnification, bounce, badges, previews and glass backgrounds. | [dock/](dock/README.md) |
| **Fluctus Glass** | A KWin effect that renders true Liquid Glass (refraction, lensing, dispersion) behind the windows that ask for it, or in place of blur everywhere. | [glass/](glass/README.md) |
| **Glass client** | A tiny Qt/QML library so *any* app or widget can ask for glass on its own terms. | [libs/glassclient/](libs/glassclient/README.md) |
| **Kogical.LiquidGlass** | A drop-in Qt Quick glass material, for apps that draw glass themselves. | [libs/liquidglass/](libs/liquidglass/README.md) |
| **liquidglass** | The Rust rendering engine behind Fluctus Glass. | [crates/liquidglass/](crates/liquidglass/README.md) |

## Install

You need Plasma 6.8+, Qt 6.8+, CMake, Ninja, a C++23 compiler, Rust and
[`just`](https://github.com/casey/just). On Arch Linux, that is:

```bash
sudo pacman -S --needed base-devel cmake ninja just rust extra-cmake-modules \
    qt6-base qt6-declarative qt6-shadertools libplasma plasma-workspace kwin \
    kcoreaddons kconfig ki18n kwindowsystem kglobalaccel kservice kio kcmutils
```

Then:

```bash
git clone https://github.com/kogical/fluctus.git
cd fluctus
just install          # builds and installs everything (asks for sudo)
just glass enable     # turns on Fluctus Glass
```

Now right-click your panel → **Add Widgets…** → **Fluctus Dock**.

Not sure everything is in place? `just doctor` checks your tools. No KWin development files?
`just install` still installs the dock and tells you it skipped the glass effect.

## Everyday commands

```bash
just                  # list every recipe
just update           # rebuild, reinstall and restart plasmashell
just test             # run every test
just uninstall        # remove everything Fluctus installed
just dock update      # just the dock
just glass nested     # try the glass effect in a throwaway nested KWin
```

Arch users can also build a package with `cd packaging/arch && makepkg -si`, and Nix users
can run `nix build`.

## Contributing

Start with the [development guide](docs/development.md): how the repository fits together,
how builds share one cache, and how to add a component. [AGENTS.md](AGENTS.md) lists the rules
that keep the dock smooth; please read it before touching `dock/`. All documentation is indexed
in [docs/](docs/README.md).

## License

Everything is [Apache-2.0](LICENSE), except **Fluctus Glass** (`glass/`), which is
[GPL-3.0-or-later](glass/LICENSE) because it links KWin. The glass client and the rendering
engine stay Apache-2.0, so any app can use them.
