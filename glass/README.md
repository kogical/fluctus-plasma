# Fluctus Glass

**Real Liquid Glass for KDE Plasma: a KWin effect that bends the light behind your windows.**

Blur makes a surface frosted. Fluctus Glass makes it *glass*: the edges act like a lens and
refract what is behind them, colors split slightly at the rim, and a soft highlight follows your
pointer. The center stays crisp. The effect is drawn by the compositor, so it refracts other
windows as well as the wallpaper.

It works in two ways, and you can use both at once:

- **On request.** Apps and widgets ask for glass on the parts of their windows that should be
  glass. Fluctus Dock does this, and so can any app that uses the
  [glass client](../libs/glassclient/README.md).
- **Global mode.** Liquid Glass replaces blur everywhere: windows, title bars, menus,
  panels and notifications, each of which you can toggle. You only ever need one glass effect.

## Install

You need KWin's development files and Rust (on Arch both come with `kwin` and `rust`). Then,
from the repository root:

```bash
just glass install    # build and install (asks for sudo)
just glass enable     # turn it on now and on every login
```

You can also turn it on under **System Settings → Desktop Effects → Fluctus Glass**, where its
settings page lives too.

> **After every KWin update, rebuild:** `just glass update`. KWin effects are built against
> KWin's private headers, so an effect built for an older KWin will not load. Log out and back
> in if the reload does not pick up the new build.

## Settings

All of these are in the effect's settings page:

- **Global mode.** Use Liquid Glass in place of blur, and choose where: app windows, title
  bars, menus and popups, panels and docks, notifications and OSDs. Lists of window classes
  let you never or always use glass for particular apps.
- **Shape.** Corner radius for windows, menus and panels (or follow the window's own shape).
- **Material.** Overall intensity, refraction, glass thickness, magnification, color fringing,
  frost, saturation, brightness, tint, edge highlight and shading, sheen.
- **Pointer.** Light and lens that follow the pointer, and how much the lens bulges.

## Asking for glass from your own app

Apps stay in control of their own surfaces. They can turn glass on or off per area, choose a
preset (`liquid` or `crystal`), tune any parameter, ask for plain blur instead, or opt an area
out entirely, even when global mode is on. From QML:

```qml
import Kogical.FluctusGlass

GlassArea {
    anchors.fill: parent
    radius: 24
    intensity: 0.8      // 0..2
}
```

C++ and the full list of parameters are covered in the
[glass client README](../libs/glassclient/README.md). If Fluctus Glass is not running, the same
request falls back to KWin's normal blur.

## Hacking

```bash
just glass test       # unit tests for the pure logic
just glass nested     # run the fresh build in an isolated nested KWin; your session is untouched
just glass parity     # score the look against Apple's reference images
```

The effect itself is a thin adapter: it works out *which* areas get glass, and leaves the
rendering to the Rust engine in [crates/liquidglass](../crates/liquidglass/README.md). The
math, the optics and the reverse-engineering notes are in [docs/](docs/README.md).

Fluctus Glass is licensed under [GPL-3.0-or-later](LICENSE) because it links KWin. The client
library and the rendering engine are Apache-2.0.
