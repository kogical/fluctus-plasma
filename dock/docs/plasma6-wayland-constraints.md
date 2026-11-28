# KDE Plasma 6.8 & Wayland Constraints for a Dock Plasmoid

Running a macOS-style magnifying dock **strictly inside a Plasma Plasmoid** on **KDE Plasma 6.8 (Wayland)** imposes concrete compositor, shell, and packaging constraints. Everything below has been verified against the local **Plasma 6.8.3 / KF6 6.32** installation.

---

## 1. Verified Local Environment & SDK

- **Plasma:** `plasmashell 6.8.3`
- **Compilers & Tools:** `g++ 16.2.1`, `clang 23.1.5`, `clang-tidy`, `cmake`, `ninja`, `just`, `kpackagetool6`
- **Installed CMake SDKs (`/usr/lib64/cmake/`, symlinked to `/usr/lib/cmake/` on Arch/CachyOS):**
  - `Plasma` (`/usr/lib64/cmake/Plasma/PlasmaMacros.cmake` providing `plasma_add_applet`)
  - `PlasmaActivities`, `PlasmaActivitiesStats`, `LibTaskManager` (`PW::LibTaskManager`), `KF6WindowSystem`, `KF6GlobalAccel`
- **Compositor Blur:** Local KWayland integration supports both `org_kde_kwin_blur` and `ext_background_effect_v1`.

---

## 2. Compiled `Plasma::Applet` via `plasma_add_applet`

Instead of installing a global QML module into `/usr/lib/qt6/qml/` (like WaveTask does), Fluctus will be built as a **compiled `Plasma::Applet`** using Plasma 6's native `plasma_add_applet` macro (matching stock `org.kde.plasma.taskmanager.so`):

- **How `plasma_add_applet` works** (verified in `/usr/lib64/cmake/Plasma/PlasmaMacros.cmake`):
  - Builds a shared plugin `${id}.so` and installs it directly to `${KDE_INSTALL_PLUGINDIR}/plasma/applets/`.
  - Calls `ecm_add_qml_module(${id} URI "plasma.applet.${id}" QT_NO_PLUGIN ...)` and embeds `QML_SOURCES` / `RESOURCES` cleanly into the plugin — **zero global QML namespace pollution** and no `QML2_IMPORT_PATH` / `KDE_INSTALL_QMLDIR` headaches.
  - Allows a custom C++ `FluctusApplet : public Plasma::Applet` subclass (`K_PLUGIN_CLASS_WITH_JSON`), giving C++ direct access to `containment()`, `location()`, `formFactor()`, `status()`, and `configuration()` without WaveTask's fragile `ContainmentItem_QML` string-matching parent walk.
- **Hyphen-in-ID verification item (Stage 2 Step 1):** `PlasmaMacros.cmake` replaces `-` with `_` for generated class names, but passes `URI "plasma.applet.${id}"` verbatim to `ecm_add_qml_module`. Since QML sources in the same module do not need an explicit `import` of their own URI, we will verify at Stage 2 scaffold compile time whether `qt_add_qml_module` accepts `com.kogical.fluctus-plasma` cleanly; if Qt6's tooling rejects a hyphen in `URI`, we will use `com.kogical.fluctus` as the applet ID.
- **Required metadata:** `package/metadata.json` must declare `"X-Plasma-Provides": ["org.kde.plasma.multitasking"]`, and `main.qml` must expose `activateTaskAtIndex(index)` so Plasma shell components can interoperate with Fluctus as a task manager.

---

## 3. Wayland Panel Clipping, Headroom, Input Masking & `dodgewindows`

### Panel Surface Clipping & Exact Thickness Formula
On Wayland, a Plasma Panel is a single `QQuickWindow` (`PanelView`). Child items cannot draw outside it.
- **Required thickness:**
  $$T_{\text{required}} = \text{edgePadding} + \lceil s_0 \cdot M \rceil + \lceil \text{bounceRatio} \cdot s_0 \cdot M \rceil + \text{topMargin}$$
  For `48 px` icons at `1.8×` zoom (`86 px`) plus `0.55×` bounce (`48 px`), the panel needs **`~134 px + padding`** (WaveTask's hardcoded `86 px` clips bounces).
- Fluctus exposes this formula from C++ to both the settings UI (showing recommended vs. current panel thickness) and clamps live bounce amplitude `H` to whatever headroom is actually available.
- **Primary-axis pre-allocation:** Reserve $\Delta_{\max} = (M - 1) \cdot R \cdot W(1)$ on each side of the resting dock span inside `DockLayoutItem`'s `implicitWidth` / `Layout.preferredWidth` (updated only on task count or config changes, never per frame) so the Wayland panel surface never resizes during hover.
- **Screen-edge hit area:** Set `Plasmoid.constraintHints: Plasmoid.CanFillArea` so click/hover hits reach the exact screen edge (infinite Fitts's Law depth).

### `dodgewindows` & Transparent Headroom Click-Through
- **Known Plasma limitation:** `dodgewindows` uses the panel's full surface rectangle, so windows near the screen edge will hide the dock when they enter the transparent headroom, and without an input mask, clicks in empty headroom hit the panel instead of windows behind it.
- **Hover engagement region:** Hover engagement (`α → 1`) must trigger **only** when the pointer is inside the **visible dock pill / magnified icon hull**, never in empty headroom above resting icons.
- **`QWindow::setMask` policy:** Optional input-masking to pass clicks through empty headroom is **only** permitted when:
  1. Fluctus is the sole applet in its panel containment, AND
  2. `Plasmoid.containment.corona.editMode` is `false`.
  We will test in Stage 2 whether `PanelView` overwrites `QWindow::setMask` on resize/state changes before enabling it by default.

---

## 4. Rounded Blur-Behind & `PanelView` Contention

`KWindowEffects::enableBlurBehind(QWindow*, bool, const QRegion&)` works on Plasma 6.8 Wayland with rounded `QRegion`s, **with three critical caveats**:
1. **`PanelView` overwrites blur:** Plasma's panel sets its own blur-behind on resize, theme/opacity change, and edit-mode toggle — last call wins. `BlurRegionController` must re-apply blur after window resize, screen change, containment/edit-mode change, and pill geometry change.
2. **Window lifecycle:** On `ItemSceneChange` / `windowChanged`, clear blur on the **old** `QWindow` before applying it to the **new** one (using `QPointer<QWindow>`).
3. **Fractional scaling & antialiased corners:** `QRegion` uses integer logical pixels, which can step slightly past antialiased rounded pill corners under fractional scaling (`125%`, `150%`). Inset the blur region by **`~1 px`** inside the rendered pill rect so blur never bleeds outside the border.

---

## 5. KWin Minimize Geometry (`requestPublishDelegateGeometry`)

To keep KWin's minimize/unminimize animations targeting the dock icons accurately, `DockLayoutItem` must call `tasksModel.requestPublishDelegateGeometry(modelIndex, globalRect, item)` using each task's **static resting slot rect in global screen coordinates** (never the magnified rect), republishing whenever tasks settle, reorder, or the panel window moves.

---

## 6. Global Shortcuts (Meta+1..9) & No `/dev/input`

Verified against Plasma 6.8.3 and `~/.config/kglobalshortcutsrc`:
- Stock Plasma 6.8's `org.kde.plasma.taskmanager.so` registers `"activate task manager entry 1"`..`10`, `"activate next/previous task manager entry"`, and `"move active task to left/right"` under the `plasmashell` component. Creating a separate component (like WaveTask's `[org.vicko.wavetask]`) still conflicts over Meta+1 because only one global shortcut owner receives the key.
- **Fluctus strategy:**
  1. **Reuse the stock action names** so existing user Meta+1..9 bindings carry over seamlessly without duplicate System Settings entries. *(Stage 2 verification: test coexistence when both stock Task Manager and Fluctus are loaded simultaneously.)*
  2. **Single process-wide `ShortcutManager` router** (owned as a clean singleton with a `QPointer<FluctusApplet>` registry, avoiding WaveTask's `static std::once_flag` dangling-pointer bug). Routes shortcut activations to the Fluctus instance on the active window's screen → cursor's screen → primary screen.
  3. **No `/dev/input/event*` evdev reading:** Plasma has no supported modifier-hold API without evdev. Instead, show numbered index badges briefly (`~1.5 s`) after any Meta+N activation, plus an optional toggle shortcut (e.g., `Meta+` `).

---

## 7. Panel Layout Template & Packaging Paths

- **Layout template:** `layout-templates/metadata.json` must declare `"KPackageStructure": "Plasma/LayoutTemplate"` and install to `${KDE_INSTALL_DATADIR}/plasma/layout-templates/com.kogical.fluctus-plasma.panel/`.
- **System install paths:** Configure CMake with `-DKDE_INSTALL_USE_QT_SYS_PATHS=ON` so plugins install to `/usr/lib/qt6/plugins/plasma/applets/` on Arch/CachyOS (`/usr/lib64` is a symlink to `/usr/lib`).
- **Distro packaging note:** Direct `/usr` installs bypass `pacman` on Arch/CachyOS. While `just install` / `uninstall` / `update` will support direct CMake installs cleanly via a persisted manifest, providing a minimal `PKGBUILD` in Stage 2+ is a clean option for tracked package management; for rootless dev installs, `~/.local` works when `QT_PLUGIN_PATH` is set via `~/.config/environment.d/`.
