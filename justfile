# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: Apache-2.0
#
# Fluctus. Run `just` to see every recipe. The usual ones:
#
#   just install      build and install everything
#   just test         run every test
#   just dock update  rebuild and reinstall only the dock, then restart plasmashell
#
# Each component has its own justfile (dock/, glass/, crates/liquidglass/); all of them share one
# CMake build tree and one Cargo target/ directory. Settings: see tools/common.just.

import 'tools/common.just'

# Fluctus Dock, the Plasma task manager applet
mod dock
# Fluctus Glass, the KWin Liquid Glass effect
mod glass
# liquidglass, the Rust rendering engine behind Fluctus Glass
mod liquidglass 'crates/liquidglass'
# scintilla, the standalone async device discovery and battery crate
mod scintilla 'crates/scintilla'

# List every recipe
[private]
default:
    @just --list --list-submodules

# Check that the build tools are installed
doctor:
    #!/usr/bin/env bash
    missing=0
    for tool in cmake ninja c++ cargo; do
        if command -v "$tool" >/dev/null; then printf '  ok       %s\n' "$tool"
        else printf '  missing  %s\n' "$tool"; missing=1; fi
    done
    for tool in ccache sccache mold lld run-clang-tidy rsvg-convert; do
        command -v "$tool" >/dev/null && printf '  ok       %s (optional)\n' "$tool"
    done
    [ $missing -eq 0 ] || { echo "Install the missing tools; README.md lists the libraries too."; exit 1; }
    echo "All set. CMake reports missing libraries when you run 'just build'."

# Build everything
build: (compile preset)

# Force CMake to configure again (after installing new libraries or changing options)
reconfigure:
    cd "{{ root }}" && cmake --preset "{{ preset }}" -DCMAKE_INSTALL_PREFIX="{{ prefix }}"

# Run every test: the Rust crates, then all C++ tests
test: liquidglass::test scintilla::test build (ctest preset)

# Run the C++ tests under AddressSanitizer + UndefinedBehaviorSanitizer
test-san: (compile san_preset) (ctest san_preset)

# Run clang-tidy and clippy
lint: build liquidglass::lint scintilla::lint
    # Qt adds this GCC-only flag to GCC builds; clang-tidy (always clang) rejects it.
    run-clang-tidy -quiet -removed-arg=-mno-direct-extern-access -p "{{ build_dir }}" '^{{ root }}/(dock/src|libs/glassclient|glass/(src|kcm))/.*\.cpp$'

# Everything CI checks
ci: test test-san lint

# Install every component that can be built here (asks for sudo when needed)
install: dock::install
    #!/usr/bin/env bash
    if grep -q '^FLUCTUS_GLASS_KWIN_VERSION:' "{{ build_dir }}/CMakeCache.txt"; then
        just glass install
    else
        echo "Skipped Fluctus Glass: CMake did not find KWin's development files."
    fi

# Remove every installed component
uninstall: dock::uninstall glass::uninstall

# Reinstall everything, reload the glass effect and restart plasmashell (restart=ask|yes|no)
update restart="ask": (dock::update restart)
    #!/usr/bin/env bash
    if grep -q '^FLUCTUS_GLASS_KWIN_VERSION:' "{{ build_dir }}/CMakeCache.txt"; then
        just glass update
    fi

# Export the PNG logos (64 to 512 px) from docs/branding/logo.svg
branding:
    #!/usr/bin/env bash
    set -euo pipefail
    cd "{{ root }}/docs/branding"
    mkdir -p png
    for size in 64 128 256 512; do
        out="png/logo-${size}.png"
        if command -v rsvg-convert >/dev/null; then rsvg-convert -w "$size" -h "$size" logo.svg -o "$out"
        elif command -v resvg >/dev/null; then resvg -w "$size" -h "$size" logo.svg "$out"
        elif command -v magick >/dev/null; then magick -background none -density 384 logo.svg -resize "${size}x${size}" "$out"
        else echo "Install rsvg-convert (librsvg), resvg or ImageMagick to render PNGs." >&2; exit 1
        fi
        echo "Wrote docs/branding/$out"
    done

# Delete build output: the CMake trees in build/ and Cargo's target/ (install records are kept)
clean:
    rm -rf "{{ root }}/build" "{{ root }}/target"
