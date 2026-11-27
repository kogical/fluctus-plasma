prefix := "/usr"
build_dir := "build/release"
state_dir := env_var_or_default("XDG_STATE_HOME", env_var("HOME") + "/.local/state") + "/fluctus-plasma"
restart := "ask"

default:
    @just --list

# Configure (only when build.ninja is missing) and build in Release mode (never under sudo)
build:
    @if [ ! -f "{{build_dir}}/build.ninja" ]; then cmake --preset release -DCMAKE_INSTALL_PREFIX={{prefix}}; fi
    cmake --build --preset release

# Build only the Plasma applet plugin (skips unit test binaries for faster iteration)
build-applet:
    @if [ ! -f "{{build_dir}}/build.ninja" ]; then cmake --preset release -DCMAKE_INSTALL_PREFIX={{prefix}}; fi
    cmake --build --preset release --target com.kogical.fluctus

# Force a fresh CMake reconfigure
reconfigure:
    cmake --preset release -DCMAKE_INSTALL_PREFIX={{prefix}}

# Build and run unit tests (Release)
test: build
    ctest --preset release --output-on-failure

# Build and run unit tests with AddressSanitizer + UndefinedBehaviorSanitizer
test-san:
    @if [ ! -f "build/debug-asan-ubsan/build.ninja" ]; then cmake --preset debug-asan-ubsan -DCMAKE_INSTALL_PREFIX={{prefix}}; fi
    cmake --build --preset debug-asan-ubsan
    ctest --preset debug-asan-ubsan --output-on-failure

# Install compiled applet and layout template, persisting install_manifest.txt
install: build-applet
    #!/usr/bin/env bash
    set -euo pipefail
    mkdir -p "{{state_dir}}"
    if [ -w "{{prefix}}" ]; then
        cmake --install "{{build_dir}}"
    else
        sudo cmake --install "{{build_dir}}"
        sudo chown "$(id -u):$(id -g)" "{{build_dir}}/install_manifest.txt"
    fi
    cp "{{build_dir}}/install_manifest.txt" "{{state_dir}}/install_manifest.txt"
    echo "Installed Fluctus Plasma. Manifest saved to {{state_dir}}/install_manifest.txt"

# Remove all installed files recorded in the persisted manifest
uninstall:
    #!/usr/bin/env bash
    set -euo pipefail
    manifest="{{state_dir}}/install_manifest.txt"
    if [ ! -f "$manifest" ]; then
        echo "No install manifest found at $manifest"
        exit 1
    fi
    while IFS= read -r file; do
        [ -z "$file" ] && continue
        if [ -e "$file" ] || [ -L "$file" ]; then
            if [ -w "$(dirname "$file")" ]; then
                rm -f "$file"
            else
                sudo rm -f "$file"
            fi
        fi
    done < "$manifest"
    # Prune empty directories strictly under com.kogical.* paths
    while IFS= read -r file; do
        [ -z "$file" ] && continue
        dir="$(dirname "$file")"
        while [[ "$dir" == *"/com.kogical."* ]]; do
            if [ -d "$dir" ] && [ -z "$(ls -A "$dir" 2>/dev/null)" ]; then
                if [ -w "$(dirname "$dir")" ]; then
                    rmdir "$dir" || true
                else
                    sudo rmdir "$dir" || true
                fi
            else
                break
            fi
            dir="$(dirname "$dir")"
        done
    done < "$manifest"
    rm -f "$manifest"
    echo "Uninstalled Fluctus Plasma."

# Rebuild, prune stale installed files, reinstall, and optionally restart plasmashell
update restart=restart:
    #!/usr/bin/env bash
    set -euo pipefail
    just build-applet
    manifest="{{state_dir}}/install_manifest.txt"
    if [ -f "$manifest" ]; then
        just uninstall
    fi
    just install
    case "{{restart}}" in
        yes)
            do_restart=true
            ;;
        no)
            do_restart=false
            ;;
        ask)
            read -r -p "Restart plasmashell now? [y/N] " reply
            [[ "$reply" =~ ^[Yy]$ ]] && do_restart=true || do_restart=false
            ;;
        *)
            echo "Invalid restart mode: {{restart}} (expected ask|yes|no)"
            exit 1
            ;;
    esac
    if [ "$do_restart" = true ]; then
        if systemctl --user is-active --quiet plasma-plasmashell.service 2>/dev/null; then
            systemctl --user restart plasma-plasmashell.service
        else
            kquitapp6 plasmashell || true
            kstart plasmashell >/dev/null 2>&1 &
        fi
    fi

# Export PNG icons (64, 128, 256, 512 px) from docs/branding/logo.svg
branding:
    #!/usr/bin/env bash
    set -euo pipefail
    src="docs/branding/logo.svg"
    out_dir="docs/branding/png"
    mkdir -p "$out_dir"
    for size in 64 128 256 512; do
        out="${out_dir}/logo-${size}.png"
        if command -v rsvg-convert >/dev/null 2>&1; then
            rsvg-convert -w "$size" -h "$size" "$src" -o "$out"
        elif command -v resvg >/dev/null 2>&1; then
            resvg -w "$size" -h "$size" "$src" "$out"
        elif command -v magick >/dev/null 2>&1; then
            magick -background none -density 384 "$src" -resize "${size}x${size}" "$out"
        elif command -v inkscape >/dev/null 2>&1; then
            inkscape "$src" --export-type=png --export-width="$size" --export-height="$size" --export-filename="$out" >/dev/null 2>&1
        else
            echo "Error: install rsvg-convert (librsvg), resvg, ImageMagick (magick), or inkscape to render PNGs." >&2
            exit 1
        fi
        echo "Generated $out"
    done

# Remove build artifacts (preserves persisted install manifest)
clean:
    rm -rf build
