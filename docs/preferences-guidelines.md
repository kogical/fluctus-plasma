# Preferences guidelines

How Fluctus settings pages look, read and behave. These rules come from Fluctus Dock's settings
([dock/package/contents/ui/Config*.qml](../dock/package/contents/ui/)), which every other
component's settings follow. When a page and this document disagree, fix one of them.

## Principles

1. **Show, don't describe.** A visual setting is judged by looking at it. Every page that changes
   how something looks carries a live preview that reacts before Apply.
2. **Start from a good whole, then tune.** The first page offers complete looks (presets) to pick
   from. Individual knobs come after, on their own pages.
3. **Plain words, real consequences.** Labels name the thing, control text says what happens, and
   hints explain the effect in numbers or in terms of what the user will see.
4. **Only what applies.** Settings that do not apply to the current choice are hidden; settings
   that apply but are switched off are disabled.
5. **Never a dead end.** When something the widget needs is missing (Fluctus Glass, a udev rule,
   a panel), the page says so where it matters, offers the fix, and the widget keeps working with a
   fallback in the meantime.

## Organization

- **Pages** are `ConfigCategory` entries in `config/config.qml`, named with one short noun or noun
  pair and a Breeze icon: *Presets*, *Appearance*, *Size & Spacing*, *Magnification*, *Behavior*.
  Order: presets first, then how it looks, then how it moves or sizes, then how it behaves.
- **Sections** inside a page are `Kirigami.Separator { Kirigami.FormData.isSection: true }` with a
  Title Case label (*Dock Background*, *Shape and Edges*, *Running Apps*). Three to six sections a
  page; split the page when it grows beyond that.
- **One idea per row.** A row is a `Kirigami.FormLayout` item: a label, one control, and at most
  one hint below it. Related checkboxes share one label and stack without labels of their own
  (*Only show windows from:* then *This screen*, *The current virtual desktop*…).
- **Layout** is `KCM.SimpleKCM` with a `Kirigami.FormLayout`. Sliders and hints are capped at
  22 grid units wide so lines stay readable on wide windows.

## Presets page

- One sentence of introduction: *"Pick a style to start from. You can fine-tune everything
  afterwards on the other pages."*
- A responsive grid of cards (`QQC2.ItemDelegate`, about 17 grid units wide). Each card shows a
  **live preview** rendered by the same component as the real widget, a level-4 heading, and one
  short description sentence in the small font at 75% opacity. The preset in use carries a
  checkmark; the selected one is highlighted.
- **Selecting does not save.** Picking a card only selects it; the page sets `unsavedChanges` so
  Plasma enables Apply, and writes the values in `saveConfig()`. A `Kirigami.InlineMessage` says
  what Apply will do (*"macOS Tahoe is selected. Press Apply to use it."*) or why nothing will
  change.
- When presets cover independent aspects, let the user choose which parts to apply with
  checkboxes, and default the disruptive parts (how things move) to off.
- Editing any preset-controlled value by hand on another page clears the active preset
  (`markCustom()`), so the checkmark never lies.
- A *Light wallpaper* switch lets the user judge glass and contrast on both kinds of wallpaper.

## Live previews

- Visual pages put the preview in the page `header`: the preview (about 8 grid units tall), then a
  caption row with a short hint in the small, disabled font (*"Preview on a sample wallpaper.
  Point at it to try the glass."*) and the *Light wallpaper* switch, then a `Kirigami.Separator`.
- The preview binds to the page's controls, not to saved config, so it shows unsaved changes.
  Settings edited on other pages are previewed as saved.
- Previews use sample content (sample icons, sample devices) and a sample wallpaper, never the
  user's live data, so every look is judged on the same scene.
- Previews render with the real components and materials. When Fluctus Glass is running, the
  preview uses its exact material; otherwise it shows the fallback the user will actually get.

## Controls

| Use | For | Example |
| --- | --- | --- |
| `QQC2.CheckBox` | An optional behavior | *Bounce:* "Bounce icons while apps open or need attention" |
| `QQC2.Switch` | Something visible that turns on or off immediately in the preview | *Light wallpaper* |
| `QQC2.ComboBox` | Three to six named, mutually exclusive choices | *Background:* None / Glass / Plasma theme / Icons only |
| `ConfigSlider` | A continuous amount, with a fixed-width read-out | *Corner roundness:* "18 px" … "Pill" |
| `QQC2.SpinBox` | A small exact number with a unit | *Outline width:* "1 px" |
| Button with `…` | Opens a dialog | "Choose…" |

- **Read-outs speak human units**: percentages, `px`, `×`, or words when numbers mean little
  (*Light / Medium / Strong*, *Pill* at the maximum radius). A space goes between number and unit
  (`%1 px`), none before `%`.
- **Combo boxes explain their current choice** with a hint below that changes with the selection
  (one description per option, kept in an array next to the model).
- Shared building blocks: `ConfigHint.qml` (hint text) and `ConfigSlider.qml` (slider with
  read-out). Copy them into a component's package rather than restyling ad hoc.

## Progressive disclosure

- **Hide** controls that make no sense for the current choice (material settings when the
  background is not glass, outline width when there is no outline).
- **Disable** controls whose parent toggle is off but would apply once it is on (wave width while
  magnification is unchecked), so users can see what turning it on would offer.
- Hints follow the same `visible` condition as the control they explain.

## Language

- **Labels** (`Kirigami.FormData.label`) are sentence case and end with a colon: *Corner
  roundness:*, *Clicking the active app:*.
- **Control text** describes the outcome, starting with a verb, sentence case, no final period:
  *"Hide the panel behind the dock"*, *"Grow icons as the pointer moves over them"*. Avoid
  *Enable*/*Disable*/*Toggle*; the checkbox already says that.
- **Hints** are complete sentences with a period. Explain what the user will see or what it
  costs, with real numbers where possible: *"Icons grow from 32 px to 48 px."* Mention side
  effects outside the widget (*"Blur strength is shared by every blurred surface on your
  desktop"*).
- Name things the way the user sees them: *Liquid Glass*, *Frosted*, *panel*, *receiver*. No
  internal names (config keys, D-Bus names, enum values) and no jargon (*telemetry*, *stub*,
  *HID*) in labels; a technical term may appear in a hint when the user needs it to act.
- No exclamation marks, no "please", no blame (*"isn't running"*, not *"you didn't enable"*).
- US English. Typographic quotes (“ ”) and ellipsis (…). Every string through `i18n()`.

## Problems and fallbacks

- Report a problem with a `Kirigami.InlineMessage` placed **where it matters**: the Liquid Glass
  notice sits with the glass settings and on the presets that need it, not at the top of every
  page.
  - `Information` when something optional is missing (Fluctus Glass not installed).
  - `Warning` when the user chose something that cannot work right now (*"This material needs a
    glass effect in KWin, which isn't running, so the dock shows Frosted for now."*).
- Every message offers the next step as actions: fix it directly when we can (*Turn on Fluctus
  Glass*), otherwise *Copy command*, *Installation guide* or *Open Desktop Effects*, and always a
  way to re-check (*Check again*). Messages disappear once the problem is solved.
- **Always have a fallback when Liquid Glass is missing.** Liquid Glass needs the Fluctus Glass
  KWin effect (and, for QML, the `Kogical.FluctusGlass` module, which is installed separately).
  Without it the widget must still load and look right, using the **pre-Liquid Glass macOS look**:
  dense, softly blurred *Frosted* glass with a crisp hairline outline, the way macOS looked through
  Sequoia. Concretely:
  - Never import `Kogical.FluctusGlass` from a file the widget needs to start; load glass surfaces
    and glass settings through a `Loader` and fall back to the Frosted surface when it fails.
  - Options that need Liquid Glass stay listed. Either lock them, marked *(needs Fluctus Glass)*
    (the Dock, whose materials have no automatic fallback), or accept them and render the
    fallback with a `Warning` that says so (Fluctus Battery: *"…so the widget shows Frosted for
    now"*). Never let the user believe they got Liquid Glass when they did not. Preset cards that
    use Liquid Glass say what happens without it.
  - The preview shows the fallback the user will really get.
- Do not fight other tools that own the same thing: when Panel Colorizer styles the panel, the
  dock says so and leaves the panel background alone.
- Settings that belong to the system (KWin's blur strength) are labeled as shared, and saved through
  the page's `saveConfig()` rather than the widget's config.

## Implementation checklist

- [ ] New key in `config/main.xml` with a sensible default; `cfg_<name>` on its page; read in QML
      through `Plasmoid.configuration.<name>`.
- [ ] New QML file listed in the component's QML sources in `CMakeLists.txt`.
- [ ] Visual setting: visible in the page's live preview and in the preset cards.
- [ ] Hint written in plain words; read-out in human units.
- [ ] Works, and looks right, without Fluctus Glass.
