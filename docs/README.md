# Fluctus documentation

READMEs are for people *using* a component. This directory and each component's `docs/` hold the
deeper material: architecture, math, research notes and the rules that keep things working.

## Repository-wide

- [development.md](development.md): how the repository is laid out, how the builds share one
  cache, the justfile conventions, CI, and how to add a component.
- [AGENTS.md](../AGENTS.md): hard invariants and coding standards for humans and AI agents alike.
- [preferences-guidelines.md](preferences-guidelines.md): how settings pages are organized, worded
  and previewed, and the fallback every component needs when Liquid Glass is missing.
- [branding/](branding/): logos (`just branding` re-exports the PNGs).

## Fluctus Dock: [dock/docs/](../dock/docs/README.md)

| Document | About |
| --- | --- |
| [architecture.md](../dock/docs/architecture.md) | The compiled applet, C++-owned layout and animation, 4-edge + RTL geometry, C++23 standards |
| [macos-dock-physics.md](../dock/docs/macos-dock-physics.md) | Magnification curves, drift-free displacement, springs, bounce |
| [plasma6-wayland-constraints.md](../dock/docs/plasma6-wayland-constraints.md) | What Plasma 6.8 and Wayland allow a panel applet to do |
| [wavetask-audit.md](../dock/docs/wavetask-audit.md) | Research notes on an earlier Plasma dock |

## Fluctus Battery: [battery/README.md](../battery/README.md)

Adaptive Liquid Glass battery monitor plasmoid powered by `scintilla` and `Kogical.FluctusGlass`.

## Fluctus Glass: [glass/docs/](../glass/docs/README.md)

| Document | About |
| --- | --- |
| [liquid-glass-math.md](../glass/docs/liquid-glass-math.md) | Optics and shader specification of the Liquid Glass look |
| [apple-liquid-glass-re.md](../glass/docs/apple-liquid-glass-re.md) | What is known about Apple's implementation, and how sure we are |

## Libraries

- [libs/glassclient](../libs/glassclient/README.md): asking Fluctus Glass for glass, from QML or C++;
  the D-Bus protocol lives in `GlassProtocol.hpp`.
- [libs/liquidglass](../libs/liquidglass/README.md): the `Kogical.LiquidGlass` Qt Quick material.
- [crates/liquidglass](../crates/liquidglass/README.md): the Rust rendering engine
  (`just liquidglass doc` for the API reference).
