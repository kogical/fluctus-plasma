#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs a nested, fully isolated KWin (own D-Bus session, own config dir) with the freshly built
# Fluctus Glass effect loaded, plus the example backdrop and glass windows. The live session is
# never touched.
#
#   glass/tools/run-nested.sh <build-dir> [--screenshot out.png] [--global] [--seconds N]
set -euo pipefail

build_dir="$(realpath "${1:?build dir}")"
shift
shot=""
global=false
seconds=0
scale=1
backend=--virtual
pointer=""
while [ $# -gt 0 ]; do
    case "$1" in
        --screenshot) shot="$(realpath -m "$2")"; shift 2 ;;
        --global) global=true; shift ;;
        --seconds) seconds="$2"; shift 2 ;;
        --scale) scale="$2"; shift 2 ;;
        --windowed) backend=""; shift ;;   # show the nested session as a window instead
        --pointer) pointer="$2"; shift 2 ;; # "x y": simulated pointer before the screenshot
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done

here="$(cd "$(dirname "$0")/.." && pwd)"
if [ -n "${FLUCTUS_NESTED_STAGE:-}" ]; then
    stage="$FLUCTUS_NESTED_STAGE"   # kept afterwards for inspecting logs
    rm -rf "$stage"; mkdir -p "$stage"
else
    stage="$(mktemp -d -t fluctus-glass-nested.XXXXXX)"
    trap 'rm -rf "$stage"' EXIT
fi

mkdir -p "$stage/plugins/kwin/effects/plugins" "$stage/plugins/kwin/effects/configs" "$stage/qml" "$stage/config"
ln -s "$build_dir/bin/fluctusglass.so" "$stage/plugins/kwin/effects/plugins/fluctusglass.so"
ln -s "$build_dir/libs/glassclient/qml/Kogical" "$stage/qml/Kogical"
cat > "$stage/config/kwinrc" <<CFG
[Plugins]
blurEnabled=true
fluctusglassEnabled=true

[Effect-fluctusglass]
GlobalMode=$global
CFG

wallpaper="${BACKDROP_IMAGE:-$(ls -S /usr/share/wallpapers/*/contents/images/*.png 2>/dev/null | head -1 || true)}"
cat > "$stage/BackdropRun.qml" <<QML
import "file://$here/examples"
Backdrop { imagePath: "$wallpaper"; animate: ${ANIMATE:-true}; showBlocks: ${BLOCKS:-true}; showText: ${SHOW_TEXT:-true}; frameless: ${FRAMELESS:-false} }
QML

export XDG_CONFIG_HOME="$stage/config"
export QT_PLUGIN_PATH="$stage/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QML_IMPORT_PATH="$stage/qml"
export QT_LOGGING_RULES="fluctus.glass*.debug=true;kwin_effect*.warning=true"
export POINTER="$pointer" BACKEND="$backend" STAGE="$stage" HERE="$here" WALLPAPER="$wallpaper" SHOT="$shot" SECONDS_TO_RUN="$seconds" SCALE="$scale"

dbus-run-session -- bash -c '
    set -u
    socket="wayland-fluctus-glass-$$"
    # shellcheck disable=SC2086
    kwin_wayland $BACKEND --no-lockscreen --no-global-shortcuts --width 1280 --height 800 --scale "$SCALE" --socket "$socket" \
        > "$STAGE/kwin.log" 2>&1 &
    kwin=$!
    for _ in $(seq 50); do [ -S "${XDG_RUNTIME_DIR}/$socket" ] && break; sleep 0.1; done
    export WAYLAND_DISPLAY="$socket" QT_QPA_PLATFORM=wayland
    /usr/lib/qt6/bin/qmlscene "$STAGE/BackdropRun.qml" > "$STAGE/backdrop.log" 2>&1 &
    sleep 1.5
    /usr/lib/qt6/bin/qmlscene --transparent "${DEMO_QML:-$HERE/examples/GlassDemo.qml}" > "$STAGE/demo.log" 2>&1 &
    if [ "${EXTRA_POPUP:-false}" = true ]; then
        /usr/lib/qt6/bin/qmlscene "$HERE/examples/PlasmaPopup.qml" > "$STAGE/popup.log" 2>&1 &
    fi
    sleep 2.5
    echo "kwin alive: $(kill -0 $kwin 2>/dev/null && echo yes || echo no)"
    echo "loaded effects: $(timeout 5 qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadedEffects | tr "\n" " ")"
    echo "glass info: $(timeout 5 qdbus6 com.kogical.Fluctus.Glass /com/kogical/Fluctus/Glass com.kogical.Fluctus.Glass1.GetInfo 2>&1 | tr "\n" " ")"
    if [ -n "$POINTER" ]; then
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.debug fluctusglass "pointer $POINTER" > /dev/null
        sleep 0.5
    fi
    echo "glass state:"; timeout 5 qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.debug fluctusglass state
    if [ -n "$SHOT" ]; then
        timeout 15 spectacle -b -n -f -o "$SHOT" > "$STAGE/spectacle.log" 2>&1 || echo "screenshot failed"
        echo "screenshot: $SHOT"
    fi
    if [ "$SECONDS_TO_RUN" -gt 0 ]; then
        # KWin CPU time over the idle window (ticks of 1/100 s): ~0 means nothing repaints.
        t0=$(awk "{print \$14+\$15}" /proc/$kwin/stat)
        sleep "$SECONDS_TO_RUN"
        t1=$(awk "{print \$14+\$15}" /proc/$kwin/stat)
        echo "kwin cpu over ${SECONDS_TO_RUN}s: $((t1 - t0)) ticks"
    fi
    pkill -P $$ qmlscene 2>/dev/null
    kill $kwin 2>/dev/null; wait $kwin 2>/dev/null
    echo "--- kwin log (glass) ---"; grep -i "glass\|shader\|error" "$STAGE/kwin.log" | head -40
    echo "--- demo log ---"; head -20 "$STAGE/demo.log"
'
