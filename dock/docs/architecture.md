# Fluctus Dock — Architecture & Engineering Standards

## 1. Core Architectural Principle: Compiled `Plasma::Applet` + C++-Owned Layout & Physics

All math, physics, delegate layout, and system integration live in **modern C++23**, compiled as a native `Plasma::Applet` (`plasma_add_applet`). QML is strictly a thin declarative shell for Plasma UI (delegate visuals, PipeWire tooltips, context menus, group dialog, and Kirigami config pages).

```mermaid
flowchart TD
    subgraph Applet["Compiled Plasma::Applet (plasma_add_applet)"]
        Fluctus["FluctusApplet : Plasma::Applet"]
    end

    subgraph QML["Thin QML Shell (embedded QML module)"]
        Main["main.qml (Plasmoid root + TasksModel + passive HoverHandler + DropArea)"]
        Del["TaskDelegate.qml (fixed logical size, mipmapped Icon, Badge, Reflection)"]
        UI["ToolTip / ContextMenu / GroupDialog / Config*.qml"]
    end

    subgraph CPP_Quick["C++ Qt Quick Layer (src/quick/)"]
        DockItem["DockLayoutItem : QQuickItem (owns delegates + GUI-thread ticker)"]
        Blur["BlurRegionController (rounded QRegion + PanelView re-apply)"]
    end

    subgraph CPP_Core["Pure C++23 Engine (src/core/ — zero QtQuick dep)"]
        Engine["DockPhysicsEngine"]
        Integral["ScaleDensityIntegral (closed-form curves)"]
        Spring["DampedSpring (exact 3-regime solver)"]
        Bounce["BouncePhysics (latched parabola)"]
        Axis["AxisGeometry (4-edge + RTL sign)"]
    end

    subgraph CPP_Backend["C++ Plasma/KF6 Backend (src/backend/)"]
        Backend["TaskBackend (JumpLists, Places, RecentDocs)"]
        Smart["SmartLauncherBackend (process singleton DBus)"]
        Keys["ShortcutManager (process-wide QPointer router)"]
    end

    Fluctus --> Main
    Main --> DockItem
    DockItem -->|"Owns lifecycle + setPosition/setScale only"| Del
    DockItem --> Engine
    DockItem --> Blur
    Engine --> Integral
    Engine --> Spring
    Engine --> Bounce
    Engine --> Axis
    Main --> Backend
    Main --> Smart
    Fluctus --> Keys
```

---

## 2. Key Qt Quick Design Rules (Preventing Frame Stutter & Thread Bugs)

1. **Strictly GUI-Thread Animation Loop (Never `beforeSynchronizing`):**
   - `QQuickWindow::beforeSynchronizing` runs on the **render thread** in `plasmashell`'s threaded render loop; mutating `QQuickItem` properties there is thread-unsafe.
   - `DockLayoutItem` drives physics strictly on the **GUI thread** using a `QAbstractAnimation` subclass (`duration() == -1`) stepped in `updateCurrentTime(int currentTime)` (deriving `dt` from the animation clock) — started on hover/bounce/model change and stopped immediately when `DockPhysicsEngine::isSettled()` is `true` (`0%` idle CPU/GPU).
2. **`DockLayoutItem` Owns Delegate Instantiation (No `Repeater`):**
   - `Repeater` destroys delegates immediately on `rowsRemoved` (breaking removal-weight animations) and desynchronizes `childItems()` order after `rowsMoved`.
   - `DockLayoutItem` exposes `model` (`QAbstractItemModel*`) and `delegate` (`QQmlComponent*`), instantiating delegates via `beginCreate` → `setInitialProperties` → `completeCreate`, tracked by `QPersistentModelIndex`.
   - Removed rows transition into **ghost slots** whose weight springs animate to `0` before `deleteLater()`.
   - **Invariant:** Delegates must never bind `x`, `y`, `width`, `height`, or `scale` in QML; debug builds assert/warn if any geometry binding is detected.
3. **Scale-Only Per-Frame Updates (Never `setSize` Per Frame):**
   - Every delegate is sized once to fixed logical `ceil(s₀ · M)` (recomputed only when `iconSize`, `maxScale`, or `devicePixelRatio` changes).
   - Per-frame animation calls **`setPosition(...)` and `setScale(...)` only**, with `transformOrigin` set per `DockEdge` (`Bottom`, `Top`, `Left`, `Right`) so icons grow away from the screen edge.
4. **Structural-Only Implicit Sizing:**
   - `DockLayoutItem` updates `implicitWidth` / `implicitHeight` (including the exact `(M - 1) · R · W(1)` slack on each side of the primary axis) **only** on task count or config changes — never per frame — preventing Wayland panel surface resizes during hover.
5. **Unified Pointer & Drag Input:**
   - A single **passive `HoverHandler`** on the container feeds pointer coordinates to `DockLayoutItem` without being swallowed by child delegates.
   - During drag-and-drop, Wayland suppresses hover events; `DropArea.onPositionChanged` forwards drag coordinates into the same pointer entry point, while insertion-index calculation uses **resting coordinates** so A/B swaps never thrash under magnification.
6. **Multi-Instance, Edit Mode & Attention:**
   - Per-instance `screenGeometry` is fed from `Plasmoid.screenGeometry`.
   - `SmartLauncherBackend` and `ShortcutManager` are process-wide singletons shared across multi-screen dock instances (using `QPointer` registries).
   - When `Plasmoid.containment.corona.editMode` is `true`, magnification is disabled and layout length is frozen.
   - `unhideOnAttention` sets `FluctusApplet::setStatus(Plasma::Types::NeedsAttentionStatus)` so auto-hiding/dodging panels reveal the dock.

---

## 3. Axis-Agnostic 4-Edge + RTL Coordinate Model

`src/core/AxisGeometry.hpp` replaces all scattered orientation branches and WaveTask's `rotation: 180` hack:

```cpp
enum class DockEdge : std::uint8_t { Bottom, Top, Left, Right };
```

- **Primary axis (`p`):** Horizontal (`x`) for `Bottom`/`Top`; Vertical (`y`) for `Left`/`Right`.
- **Primary direction sign (`dir ∈ {+1, -1}`):** Set from `LayoutMirroring.enabled` (RTL) or `reverseMode`, natively reversing item order along `p` without rotating delegates.
- **Cross axis (`c`) & growth direction (`g ∈ {-1, +1}`):**
  - `Bottom`: grows `-Y` (`transformOrigin = Item.Bottom`)
  - `Top`: grows `+Y` (`transformOrigin = Item.Top`)
  - `Left`: grows `+X` (`transformOrigin = Item.Left`)
  - `Right`: grows `-X` (`transformOrigin = Item.Right`)

---

## 4. Source Layout

Paths are relative to `dock/`. The build is driven by the repository-wide CMake project, which
also provides the shared libraries the dock links (`libs/glassclient`, `libs/liquidglass`).

```text
dock/
├── CMakeLists.txt                   # fluctus_core, fluctus_quick, plasma_add_applet
├── justfile                         # build, test, test-san, install, uninstall, update, preview
├── docs/                            # this documentation
├── src/
│   ├── core/                        # Pure C++23 static lib (fluctus_core, zero QtQuick dep)
│   │   ├── AxisGeometry.hpp
│   │   ├── MagnificationCurves.hpp
│   │   ├── DampedSpring.hpp
│   │   ├── BouncePhysics.hpp
│   │   └── DockPhysicsEngine.{hpp,cpp}
│   ├── quick/                       # DockLayoutItem, DockAnimationDriver, BlurRegionController, DockIconItem
│   └── backend/                     # FluctusApplet, TaskBackend, SmartLauncherBackend, ShortcutManager
├── tests/                           # QtTest / CTest suite (label: dock)
├── package/
│   ├── metadata.json                # Id: com.kogical.fluctus.dock, Provides: org.kde.plasma.multitasking
│   └── contents/
│       ├── config/{config.qml,main.xml}
│       └── ui/                      # Thin QML shell (main.qml, TaskDelegate.qml, tooltips, menus, configs)
└── layout-templates/
    ├── metadata.json                # KPackageStructure: Plasma/LayoutTemplate
    └── contents/layout.js
```

---

## 5. Modern C++23 & Memory-Safety Standards

1. **Ownership & Lifetime:**
   - Non-`QObject` heap allocations use `std::unique_ptr` exclusively; no raw `new`/`delete`.
   - `QObject` trees use Qt parent-child ownership; all non-owning observers (`QQuickItem*`, `QWindow*`, `FluctusApplet*`) use `QPointer<T>` and check validity before dereference.
2. **Views & Bounds:**
   - Use `std::span`, `std::array`, `std::vector`, `std::optional`, `std::expected`, `std::clamp`; avoid C-style arrays and raw pointer arithmetic.
   - Mark pure math structs/functions `constexpr` and `[[nodiscard]]`.
3. **Compiler, Sanitizers & Static Analysis:**
   - `CMAKE_CXX_STANDARD 23`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`.
   - Strict warnings on our targets: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor` (omitting `-Wnull-dereference` due to `-O2` false positives, and including Qt/KF6 headers as `SYSTEM` so `-Wconversion` stays clean).
   - `debug-asan-ubsan` CMake preset (`-fsanitize=address,undefined -fno-omit-frame-pointer`) for running `ctest`.
   - `.clang-tidy` enabling `bugprone-*`, `modernize-*`, `performance-*`, and `cppcoreguidelines-*` (with `cppcoreguidelines-owning-memory` tuned for Qt parent-ownership factories).

---

## 6. `justfile` Command Design

The dock's recipes live in `dock/justfile` and are also reachable from the root as `just dock <recipe>`.
The shared machinery (one CMake tree per preset, install manifests, sudo only when the prefix is not
writable) is described in [the development guide](../../docs/development.md#justfiles).

| Command | Behavior |
| --- | --- |
| `just build` | Configures the `release` preset once, then builds only the `com.kogical.fluctus.dock` target, **never under `sudo`**. |
| `just test` / `just test-san` | Builds the dock's test binaries and runs the CTest tests labelled `dock` (Release, or Debug + ASan/UBSan). |
| `just install` | Removes the files of the previous install, runs `cmake --install --component dock` (with `sudo` only when needed) and keeps the manifest in `$XDG_STATE_HOME/fluctus/dock.manifest`. |
| `just uninstall` | Deletes the files in that manifest (it survives `just clean`) and prunes empty directories **only under `com.kogical.*` paths**, never parent system directories. |
| `just update [restart=ask]` | Installs, removes leftovers of the legacy `com.kogical.fluctus` applet, then restarts `plasmashell` according to `restart` (`ask`, `yes`, `no`). Panels still using the legacy id are moved to `com.kogical.fluctus.dock` while `plasmashell` is stopped (a backup of the appletsrc is kept). Uses `systemctl --user` when systemd manages `plasmashell`, else `kquitapp6` / `kstart`. |
| `just preview` | Opens the installed applet in a standalone `plasmawindowed` window. |
