// SPDX-License-Identifier: Apache-2.0
pragma Singleton

import QtQuick

// Tuned material recipes for LiquidGlass.
// material(name, dark) returns a plain object whose keys match LiquidGlass properties.
QtObject {
    readonly property var names: ["liquid", "frosted", "acrylic", "clear", "solid", "crystal"]

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
        case "crystal":
            // Crystal: fully clear, deeper lens with a steeper rim bend, strong dispersion.
            return Object.assign(material("liquid", d), {
                tintAlpha: 0.02, bezel: 40, refraction: 44, magnify: 0.04, edgeBend: 0.25,
                dispersion: 0.5, frost: 0, saturation: 1.35, highlight: 1.25, highlightWidth: 5
            });
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
            // Liquid Glass (Apple-style): a convex squircle lens refracting with Snell's law. The
            // flat centre stays crisp; all bending happens in the bevel, with dispersion and a
            // bright angular rim. Barely any tint or blur.
            return {
                pointerLight: 1.0,
                tint: d ? Qt.rgba(0.07, 0.07, 0.09, 1) : Qt.rgba(1, 1, 1, 1),
                tintAlpha: d ? 0.06 : 0.015,
                bezel: 64, refraction: 60, magnify: 0.34, edgeBend: 0.15, dispersion: 0.74,
                frost: 0.24, saturation: 1.24, brightness: d ? 1.02 : 1.02,
                highlight: 0.84, highlightWidth: 1.85,
                edgeShade: 0, sheen: 0.0,
                innerBorderColor: Qt.rgba(1, 1, 1, 0),
                outerBorderColor: Qt.rgba(0, 0, 0, 0),
                borderWidth: 1, grain: 0,
                shadowColor: Qt.rgba(0, 0, 0, d ? 0.12 : 0.09), shadowBlur: 36, shadowOffsetY: 8
            };
        }
    }
}
