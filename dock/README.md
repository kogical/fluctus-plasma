<div align="center">

<img src="../docs/branding/logo.svg" alt="Fluctus Dock" width="112" />

# Fluctus Dock

**A smooth, macOS-style animated task manager for KDE Plasma 6.8+**

</div>

Fluctus Dock replaces the task manager in your Plasma panel. Icons swell as your pointer glides
over them, bounce when an app launches or wants attention, and settle back without using any
CPU at rest. Under the hood it is a compiled Plasma applet, so it behaves like the stock task
manager: pinning, grouping, jump lists and Meta+1…9 shortcuts all work the way you expect.

## Features

- **Magnification and bounce.** A choice of wave shapes (raised cosine, quintic, Gaussian) and
  a launch or attention bounce. It stops animating the moment it settles.
- **Fits your panel.** It scales icons and zoom headroom to the panel on any screen edge,
  so nothing clips on Wayland.
- **Plays nicely with themes.** It starts in **Default (Panel)** mode, so your Plasma theme or
  Panel Colorizer keeps styling the background and blur.
- **Glass backgrounds.** Liquid Glass, Frosted, Acrylic, Clear or Solid, each with its own tint,
  opacity, corner radius, outlines, shadow and rim light. With
  [Fluctus Glass](../glass/README.md) running, Liquid Glass genuinely refracts what is behind
  the dock.
- **Presets.** One-click looks (macOS Tahoe, macOS Sequoia, Glass Capsule, Plasma Native,
  Midnight Solid, Neon Aurora, Paper, Floating Icons) with live previews. You pick which
  parts each preset applies.
- **Hide the panel behind it.** Optionally remove the panel's own frame, blur and shadow,
  so only the dock remains. They come back when you turn this off or remove the widget.
- **Everything a task manager should do:**
  - Pin and unpin from the context menu (per activity, too), by dragging apps on, or by
    dragging pinned icons off.
  - Spring-animated drag reordering, dropping files onto apps, and hovering to raise windows.
  - Audio and mute badges, unread counts and progress bars.
  - Jump lists, virtual desktops and window actions in the context menu.
  - Live window previews and media controls in tooltips, plus a picker for apps with
    several windows.
  - Meta+1…9 to activate (and cycle through) the first nine apps.

## Install

From the repository root, `just install` installs everything; to install only the dock:

```bash
just dock install
```

Then right-click your panel → **Add Widgets…** → **Fluctus Dock**, or add the ready-made
**Fluctus Dock Panel** through *Add Panel*.

To update after pulling new code, run `just dock update`. It rebuilds, reinstalls and asks
before restarting plasmashell (`restart=yes` or `restart=no` skip the question). Coming from a
version before 0.2? `update` also removes the old `com.kogical.fluctus` applet and moves your
panels over to the new one, keeping their settings.

## Settings

Right-click the dock → **Configure Fluctus Dock…**:

- **Presets.** Ready-made styles. Tick which parts to apply.
- **Appearance.** The live preview, *Hide the panel behind the dock*, background, material,
  color, shape and running indicators.
- **Size & Spacing.** A panel height calculator, plus icon size, spacing and padding.
- **Magnification.** Zoomed size, wave width and shape, and the launch bounce.
- **Behavior.** Which windows show, click actions, pinned apps, tooltips and previews, and
  badges.

> **Using Panel Colorizer?** Keep the background on **Default (Panel)**. Fluctus then draws no
> pill of its own and leaves KWin's panel blur alone.

## For developers

```bash
just dock test        # unit tests
just dock test-san    # the same under ASan + UBSan
just dock preview     # open the installed dock in its own window
```

How it works, and why, is in [docs/](docs/README.md). Before changing anything, read the hard
rules in [AGENTS.md](../AGENTS.md); they keep the animation smooth.

Licensed under [Apache-2.0](../LICENSE).
