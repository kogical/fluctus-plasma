# Fluctus Battery

An adaptive **Liquid Glass** battery monitor plasmoid (`com.kogical.fluctus.battery`) for KDE Plasma 6.8+, built in C++23 and powered by [`scintilla`](../crates/scintilla/README.md) and [`Kogical.FluctusGlass`](../libs/glassclient/README.md).

## Features

- **Adapts to every surface & resolution**:
  - **Tiny & standard panels** (down to 20–32 px horizontal bars and narrow vertical panels): crisp vector battery capsule + tabular percentage, optional device icon or multi-battery strip.
  - **Desktop widget**: fluidly transitions between a compact hero radial ring, a medium summary + list card, and a multi-column responsive dashboard grid across 720p, 1080p, 1440p, and 4K/HiDPI displays.
- **Liquid Glass & shared preferences**: uses `Kogical.FluctusGlass` (`GlassArea`, `GlassProfile`, `GlassSettingsDialog`) for compositor-level Liquid Glass and per-widget customization, with automatic fallback to `Kogical.LiquidGlass` when Fluctus Glass is not running.
- **Multi-device & multi-cell telemetry**: discovers laptops, Bluetooth headsets/earbuds (including Left/Right/Case multi-cell splits), split keyboards, wireless mice, controllers, and optional stub devices via `scintilla`.

## Build & Install

```bash
just battery build
just battery test
just battery install
```
