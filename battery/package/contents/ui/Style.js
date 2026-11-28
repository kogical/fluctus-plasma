// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Shared colors and helpers for every battery view. Levels use system green, yellow and red
// (as on macOS) instead of the Plasma accent: a battery level reads as health, not selection.
.pragma library

function luminance(c) {
    return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
}

/// True when content is drawn as light text on a dark surface.
function isDark(textColor) {
    return luminance(textColor) > 0.5;
}

function levelColor(severity, dark) {
    switch (severity) {
    case "critical":
        return dark ? "#ff453a" : "#ff3b30";
    case "warning":
        return dark ? "#ffd60a" : "#f5b800";
    default:
        return dark ? "#30d158" : "#28b14c";
    }
}

/// `color` at `alpha`, for tracks, hairlines and platters derived from the text color.
function alpha(color, a) {
    return Qt.rgba(color.r, color.g, color.b, a);
}

/// Severity for a sample or raw level, mirroring the backend's thresholds.
function severityFor(percentage, charging, low, critical) {
    if (charging) {
        return "charging";
    }
    if (percentage <= critical) {
        return "critical";
    }
    if (percentage <= low) {
        return "warning";
    }
    return "normal";
}
