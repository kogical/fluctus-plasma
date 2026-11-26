prefix := "/usr"
build_dir := "build/release"
state_dir := env_var_or_default("XDG_STATE_HOME", env_var("HOME") + "/.local/state") + "/fluctus-plasma"
restart := "ask"

default:
    @just --list

# Configure and build in Release mode (never under sudo)
build:
    cmake --preset release -DCMAKE_INSTALL_PREFIX={{prefix}}
    cmake --build --preset release

# Build and run unit tests (Release)
test: build
    ctest --preset release

# Build and run unit tests with AddressSanitizer + UndefinedBehaviorSanitizer
test-san:
    cmake --preset debug-asan-ubsan -DCMAKE_INSTALL_PREFIX={{prefix}}
    cmake --build --preset debug-asan-ubsan
    ctest --preset debug-asan-ubsan

# Install compiled applet and layout template, persisting install_manifest.txt
install: build
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
update restart=restart: build
    #!/usr/bin/env bash
    set -euo pipefail
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

# Remove build artifacts (preserves persisted install manifest)
clean:
    rm -rf build
