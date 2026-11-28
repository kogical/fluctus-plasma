# Fluctus Battery

A battery widget (`com.kogical.fluctus.battery`) for KDE Plasma 6.8+: your laptop, phone,
earbuds, mice, keyboards and controllers in one place, on Liquid Glass. Built in C++23 on top of
[`scintilla`](../crates/scintilla/README.md).

## Styles

Pick one on the **Presets** settings page, then fine-tune on **Appearance**:

| Preset | Layout | Glass |
| --- | --- | --- |
| **Fluctus** | A title and a list: each device with its charge on a slim bar | Liquid Glass |
| **macOS Tahoe** | A grid of charge rings with the device in the middle | Liquid Glass |
| **macOS Sequoia** | The same rings | Frosted, the look macOS had before Liquid Glass |

Levels are green, then yellow below the *Low* level and red below *Critical* (both on the
**Behavior** page). A charging device shows a lightning bolt.

## Without Fluctus Glass

Real Liquid Glass comes from [Fluctus Glass](../glass/README.md), a KWin effect. Without it,
or without its `Kogical.FluctusGlass` QML module, the widget still loads and uses **Frosted**
glass. The settings say so and offer to install or turn on the effect. See
[docs/preferences-guidelines.md](../docs/preferences-guidelines.md#problems-and-fallbacks).

## Receivers that need permission

Keychron, Logitech, Razer, Corsair and SteelSeries receivers only report their battery to apps
allowed to talk to them. `just battery install` installs a udev rule
(`lib/udev/rules.d/70-scintilla.rules`) that allows the logged-in user. If it is missing,
for example with a prefix udev does not read, the affected device shows *"Needs access"*, and
the **Devices** settings page explains the fix and offers the one-line command.

A Keychron M6 on a Link-KM receiver is listed by name but its level can't be read yet: the
mouse does not answer battery requests through that receiver, not even Keychron's own Launcher's
(see [scintilla's notes](../crates/scintilla/HALL-OF-SHAME.md)).

## Build & Install

```bash
just battery build
just battery test
just battery install
just battery preview   # the installed widget in its own window
```
