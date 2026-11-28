# Developing Fluctus

## Layout

```text
.
├── justfile                 entry point: `just` lists every recipe
├── tools/common.just        settings and helpers every justfile imports
├── CMakeLists.txt           the one C++ superbuild; finds shared dependencies once
├── CMakePresets.json        release, debug, debug-asan-ubsan
├── cmake/                   shared CMake settings (C++23, warnings, sanitizers, ccache, mold)
├── Cargo.toml, Cargo.lock   the Cargo workspace for every Rust crate
│
├── dock/                    Fluctus Dock (Plasma applet)          justfile, README, docs/
├── glass/                   Fluctus Glass (KWin effect, GPL-3)    justfile, README, docs/
├── libs/glassclient/        Glass client library + QML module     README
├── libs/liquidglass/        Kogical.LiquidGlass Qt Quick module   README
├── crates/liquidglass/      Rust rendering engine                 justfile, README
│
├── docs/                    repository-wide docs and branding
├── packaging/arch/          PKGBUILD (also: flake.nix at the root)
└── .github/workflows/       CI, which runs `just ci`
```

Each component owns its sources, tests, docs and (when it has its own workflow) a justfile.
Shared things live at the root and nowhere else.

## One cache for everything

- **C++.** There is a single CMake project. `build/<preset>/` holds every component, so Qt,
  KDE Frameworks, the shared libraries and the precompiled headers are found and built once.
  `ccache`/`sccache` and `mold`/`lld` are picked up automatically
  (`cmake/FluctusSettings.cmake`).
- **Rust.** The root `Cargo.toml` is a workspace with one `Cargo.lock` and one `target/`.
  `cargo test`, CI and the CMake build of the KWin effect all share it. CMake asks for
  `target/release/libliquidglass.a`, so the release and sanitizer presets link the same Rust build.
  Packagers can move it with `-DFLUCTUS_CARGO_TARGET_DIR=…`.
- **CI** caches `ccache`, the CMake trees, Cargo's registry and `target/`.

## Justfiles

`just` at the root lists everything, grouped by component. Component recipes run either way:

```bash
just dock test      # from the root
cd dock && just test
```

The conventions:

- **Same verbs everywhere:** `build`, `test`, `test-san`, `install`, `uninstall`, `update`.
  Root recipes run the verb for every component.
- **Shared, not duplicated.** `tools/common.just` defines `root`, `prefix`, `preset` and
  `build_dir`, plus private helpers to configure, compile, run CTest by label, and
  install/uninstall a CMake component. Every justfile imports it.
- **Settings come from the environment:** `FLUCTUS_PREFIX` (default `/usr`) and
  `FLUCTUS_PRESET` (default `release`).
- **Never build as root.** Install steps use `sudo` only when the prefix is not writable.
- **Safe reinstalls.** Each component's installed files are recorded in
  `$XDG_STATE_HOME/fluctus/<component>.manifest`. A reinstall first *unlinks* the previous
  files, because plasmashell and KWin keep their plugins memory-mapped and overwriting one in
  place can crash the session. Uninstall prunes only directories named `com.kogical.*` or
  `Kogical`.
- **Tests are labelled** by component in CTest (`dock`, `glass`), so `just dock test` runs only
  the dock's tests.

## Adding a component

1. Create `<name>/` with a `CMakeLists.txt`. Use the shared targets (`fluctus_warnings`,
   `Fluctus::GlassClient`, …) and call `fluctus_allow_sanitizer_undefs()` on loadable plugins.
2. Add it at the root: an `option(FLUCTUS_BUILD_<NAME> …)` and an `add_subdirectory` with its
   own `CMAKE_INSTALL_DEFAULT_COMPONENT_NAME`.
3. Label its tests: `set_tests_properties(… PROPERTIES LABELS <name>)`.
4. Add `<name>/justfile` importing `../tools/common.just`, then `mod <name>` in the root justfile
   and the component in the root `install`/`uninstall`.
5. Write a `README.md` for users and put deep material in `<name>/docs/`, linked from
   [docs/README.md](README.md).
6. Rust crates go in `crates/<name>/` and join the workspace automatically.

## Licensing

Apache-2.0 everywhere, except `glass/`, which is GPL-3.0-or-later because it links KWin. Code
that must stay usable by any app (`libs/`, `crates/`) never links GPL code: it talks to the
effect over D-Bus only. Every source file starts with SPDX headers.

## CI

`.github/workflows/ci.yml` installs the toolchain on Arch Linux and runs `just ci`: Rust tests,
the C++ tests in Release and under ASan + UBSan, then clang-tidy and clippy. Run the same
locally before pushing.
