// SPDX-License-Identifier: MIT
pragma Singleton

import QtQuick

// Tuned material recipes for LiquidGlass.
// material(name, dark) returns a plain object whose keys match LiquidGlass properties.
QtObject {
    readonly property var names: ["liquid", "frosted", "acrylic", "clear", "solid"]

    function material(name, dark) {
        const d = dark !== false;
        switch (name) {
        case "frosted":
            // Pre-Liquid-Glass vibrancy: dense frosted body, crisp two-tone hairline, no lens.
            return {
                pointerLight: 0.5,
                tint: d ? Qt.rgba(0.11, 0.11, 0.12, 1) : Qt.rgba(0.97, 0.97, 0.98, 1),
                tintAlpha: d ? 0.42 : 0.55,
                bezel: 0, refraction: 0, dispersion: 0,
                frost: 0.75, saturation: 1.45, brightness: d ? 0.92 : 1.06,
                highlight: 0.22, highlightWidth: 1.5, edgeShade: 0, sheen: 0.025,
                innerBorderColor: d ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.55),
                outerBorderColor: d ? Qt.rgba(0, 0, 0, 0.42) : Qt.rgba(0, 0, 0, 0.10),
                borderWidth: 1, grain: 0,
                shadowColor: Qt.rgba(0, 0, 0, d ? 0.30 : 0.16), shadowBlur: 16, shadowOffsetY: 3
            };
        case "acrylic":
            // Dense tinted acrylic with luminosity grain.
            return {
                pointerLight: 0.35,
                tint: d ? Qt.rgba(0.13, 0.13, 0.15, 1) : Qt.rgba(0.95, 0.95, 0.96, 1),
                tintAlpha: d ? 0.66 : 0.70,
                bezel: 0, refraction: 0, dispersion: 0,
                frost: 0.95, saturation: 1.25, brightness: 1.0,
                highlight: 0.14, highlightWidth: 1.5, edgeShade: 0, sheen: 0.02,
                innerBorderColor: d ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(1, 1, 1, 0.45),
                outerBorderColor: Qt.rgba(0, 0, 0, d ? 0.30 : 0.08),
                borderWidth: 1, grain: 0.055,
                shadowColor: Qt.rgba(0, 0, 0, d ? 0.30 : 0.15), shadowBlur: 18, shadowOffsetY: 4
            };
        case "clear":
            // Almost invisible body, backdrop blur does the work.
            return {
                pointerLight: 0.8,
                tint: d ? Qt.rgba(0.10, 0.10, 0.12, 1) : Qt.rgba(1, 1, 1, 1),
                tintAlpha: d ? 0.08 : 0.12,
                bezel: 6, refraction: 4, dispersion: 0.1,
                frost: 0.45, saturation: 1.3, brightness: 1.02,
                highlight: 0.38, highlightWidth: 2, edgeShade: 0.04, sheen: 0.03,
                innerBorderColor: Qt.rgba(1, 1, 1, d ? 0.16 : 0.40),
                outerBorderColor: Qt.rgba(0, 0, 0, d ? 0.14 : 0.06),
                borderWidth: 1, grain: 0,
                shadowColor: Qt.rgba(0, 0, 0, 0.14), shadowBlur: 14, shadowOffsetY: 3
            };
        case "solid":
            return {
                pointerLight: 0.3,
                tint: d ? Qt.rgba(0.12, 0.12, 0.14, 1) : Qt.rgba(0.96, 0.96, 0.97, 1),
                tintAlpha: 1.0,
                bezel: 0, refraction: 0, dispersion: 0,
                frost: 0, saturation: 1, brightness: 1,
                highlight: 0.10, highlightWidth: 1.5, edgeShade: 0, sheen: 0.02,
                innerBorderColor: Qt.rgba(1, 1, 1, d ? 0.07 : 0.6),
                outerBorderColor: Qt.rgba(0, 0, 0, d ? 0.35 : 0.12),
                borderWidth: 1, grain: 0,
                shadowColor: Qt.rgba(0, 0, 0, d ? 0.32 : 0.16), shadowBlur: 16, shadowOffsetY: 4
            };
        case "liquid":
        default:
            // Liquid Glass: nearly clear body, convex lens bezel, bright angular rim, light blur.
            return {
                pointerLight: 1.0,
                tint: d ? Qt.rgba(0.07, 0.07, 0.09, 1) : Qt.rgba(1, 1, 1, 1),
                tintAlpha: d ? 0.16 : 0.20,
                bezel: 16, refraction: 14, dispersion: 0.18,
                frost: 0.14, saturation: 1.65, brightness: d ? 1.03 : 1.08,
                highlight: d ? 0.85 : 0.95, highlightWidth: 3.5,
                edgeShade: d ? 0.16 : 0.07, sheen: d ? 0.05 : 0.08,
                innerBorderColor: Qt.rgba(1, 1, 1, d ? 0.10 : 0.30),
                outerBorderColor: Qt.rgba(0, 0, 0, d ? 0.26 : 0.10),
                borderWidth: 1, grain: 0,
                shadowColor: Qt.rgba(0, 0, 0, d ? 0.24 : 0.14), shadowBlur: 22, shadowOffsetY: 6
            };
        }
    }
}
