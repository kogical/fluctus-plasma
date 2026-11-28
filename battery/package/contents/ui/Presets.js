// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Widget style presets for the Presets settings page. Keys are Plasmoid.configuration entry
// names (see config/main.xml). Presets on Liquid Glass fall back to Frosted by themselves when
// Fluctus Glass is not running, so none of them is ever locked.
.pragma library

var presets = [
    {
        id: "fluctus",
        name: "Fluctus",
        description: "A calm list on Liquid Glass: every device with its charge on a slim bar.",
        values: { widgetStyle: 0, surfaceMaterial: 0, cornerRadius: 22 }
    },
    {
        id: "tahoe",
        name: "macOS Tahoe",
        description: "Charge rings in a grid on Liquid Glass, like the Batteries widget on macOS Tahoe.",
        values: { widgetStyle: 1, surfaceMaterial: 0, cornerRadius: 26 }
    },
    {
        id: "sequoia",
        name: "macOS Sequoia",
        description: "The same rings on frosted glass, as macOS looked before Liquid Glass. Needs nothing extra.",
        values: { widgetStyle: 1, surfaceMaterial: 1, cornerRadius: 22 }
    }
];

function find(id) {
    for (var i = 0; i < presets.length; ++i) {
        if (presets[i].id === id) {
            return presets[i];
        }
    }
    return null;
}

/// Whether a preset's look depends on Fluctus Glass (it shows as Frosted without it).
function usesLiquidGlass(preset) {
    return preset.values.surfaceMaterial === 0;
}
