# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: Apache-2.0
#
# Settings and helpers shared by every justfile in the repository. The root justfile and each
# component justfile import this file, so a recipe behaves the same whether you run
# `just dock install` at the root or `just install` inside dock/.
#
# Every component builds into the same CMake tree (build/<preset>) and the same Cargo target/
# directory, so dependencies are found and compiled once.
#
# Settings come from the environment:
#   FLUCTUS_PREFIX   install prefix                     (default /usr)
#   FLUCTUS_PRESET   CMake preset in CMakePresets.json  (default release)

root := clean(source_directory() / "..")
prefix := env("FLUCTUS_PREFIX", "/usr")
preset := env("FLUCTUS_PRESET", "release")
san_preset := "debug-asan-ubsan"
build_dir := root / "build" / preset
state_dir := env("XDG_STATE_HOME", home_directory() / ".local/state") / "fluctus"

# Configure a CMake preset once; later builds re-run CMake by themselves when needed.
[private]
configure p=preset:
    @[ -f "{{ root }}/build/{{ p }}/build.ninja" ] || \
        (cd "{{ root }}" && cmake --preset "{{ p }}" -DCMAKE_INSTALL_PREFIX="{{ prefix }}")

# Build the given targets (everything when none are given).
[private]
compile p *targets: (configure p)
    cd "{{ root }}" && cmake --build --preset "{{ p }}" {{ if targets == "" { "" } else { "--target " + targets } }}

# Run the CTest suite, optionally only the tests carrying one label (dock, glass).
[private]
ctest p label="":
    cd "{{ root }}" && ctest --preset "{{ p }}" {{ if label == "" { "" } else { "--label-regex '^" + label + "$'" } }}

# Install one CMake component and remember its files for a clean uninstall later. Files from the
# previous install are removed first: plasmashell and KWin keep their plugins mapped, and
# unlinking (rather than overwriting) them keeps the running session from crashing.
[private]
install-component component: (uninstall-component component)
    #!/usr/bin/env bash
    set -euo pipefail
    sudo=""; [ -w "{{ prefix }}" ] || sudo="sudo"
    $sudo cmake --install "{{ build_dir }}" --prefix "{{ prefix }}" --component "{{ component }}"
    mkdir -p "{{ state_dir }}"
    cp "{{ build_dir }}/install_manifest_{{ component }}.txt" "{{ state_dir }}/{{ component }}.manifest"
    $sudo rm -f "{{ build_dir }}/install_manifest_{{ component }}.txt"
    echo "Installed {{ component }} into {{ prefix }}."

# Remove what install-component installed (no-op when nothing is recorded).
[private]
uninstall-component component: (remove-manifest (state_dir / component + ".manifest"))

# Delete every file listed in an install manifest, prune the directories Fluctus owns, then
# delete the manifest itself.
[private]
remove-manifest manifest:
    #!/usr/bin/env bash
    set -euo pipefail
    manifest="{{ manifest }}"
    [ -f "$manifest" ] || exit 0
    run() { if [ -w "$(dirname "${!#}")" ]; then "$@"; else sudo "$@"; fi; }  # sudo only if needed
    while IFS= read -r file || [ -n "$file" ]; do  # CMake omits the final newline
        [ -n "$file" ] || continue
        if [ -e "$file" ] || [ -L "$file" ]; then run rm -f "$file"; fi
        # Only directories named after us are ever pruned, never shared system directories.
        dir="$(dirname "$file")"
        while [[ "$dir" == */com.kogical.* || "$dir" == */Kogical || "$dir" == */Kogical/* ]]; do
            [ -d "$dir" ] && [ -z "$(ls -A "$dir")" ] || break
            run rmdir "$dir"
            dir="$(dirname "$dir")"
        done
    done < "$manifest"
    rm -f "$manifest"
    echo "Removed the files listed in $manifest."
