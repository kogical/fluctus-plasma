// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Full-viewport transparent harness for Fluctus Glass visual parity tests.
// Reads FLUCTUS_PARITY_MODE from the environment (or defaults to "tahoe_card").
import QtQuick
import QtQuick.Window
import Kogical.FluctusGlass

Window {
    id: win
    width: 1280
    height: 800
    x: 0
    y: 0
    visible: true
    title: "Glass parity harness"
    color: "transparent"
    flags: Qt.FramelessWindowHint

    // Mode selected by verify-glass-parity.py via generated wrapper or property override.
    property string mode: "tahoe_card"

    // Case 1: Exact Apple Tahoe card geometry at 2/3 scale centered in 1280x800
    // (1600x1200 -> 1067x800 at xOffset=106.5; card [275..1325, 231..968], R=152)
    GlassArea {
        visible: win.mode === "tahoe_card"
        enabled: visible
        x: 290
        y: 154
        width: 700
        height: 491
        radius: 101
    }

    // Case 2: macOS Tahoe Control Center multi-shape parity (pills, square tile, circles)
    GlassArea {
        visible: win.mode === "control_center"
        enabled: visible
        x: 520
        y: 90
        width: 240
        height: 84
        radius: 42
    }
    GlassArea {
        visible: win.mode === "control_center"
        enabled: visible
        x: 780
        y: 90
        width: 210
        height: 210
        radius: 44
    }
    GlassArea {
        visible: win.mode === "control_center"
        enabled: visible
        x: 520
        y: 216
        width: 240
        height: 84
        radius: 42
    }
    GlassArea {
        visible: win.mode === "control_center"
        enabled: visible
        x: 520
        y: 324
        width: 470
        height: 84
        radius: 42
    }
    GlassArea {
        visible: win.mode === "control_center"
        enabled: visible
        x: 520
        y: 432
        width: 470
        height: 84
        radius: 42
    }
    GlassArea {
        visible: win.mode === "control_center"
        enabled: visible
        x: 520
        y: 540
        width: 96
        height: 96
        radius: 48
    }
    GlassArea {
        visible: win.mode === "control_center"
        enabled: visible
        x: 644
        y: 540
        width: 96
        height: 96
        radius: 48
    }
    GlassArea {
        visible: win.mode === "control_center"
        enabled: visible
        x: 768
        y: 540
        width: 96
        height: 96
        radius: 48
    }
    GlassArea {
        visible: win.mode === "control_center"
        enabled: visible
        x: 892
        y: 540
        width: 96
        height: 96
        radius: 48
    }

    // Case 3: Clean optical stress test (card + dock pill + circle over geometric grid/stripes)
    GlassArea {
        visible: win.mode === "optical_grid"
        enabled: visible
        x: 100
        y: 120
        width: 520
        height: 360
        radius: 74
    }
    GlassArea {
        visible: win.mode === "optical_grid"
        enabled: visible
        x: 680
        y: 160
        width: 480
        height: 88
        radius: 44
    }
    GlassArea {
        visible: win.mode === "optical_grid"
        enabled: visible
        x: 680
        y: 310
        width: 140
        height: 140
        radius: 70
    }
    GlassArea {
        visible: win.mode === "optical_grid"
        enabled: visible
        x: 860
        y: 310
        width: 300
        height: 140
        radius: 36
    }
}
