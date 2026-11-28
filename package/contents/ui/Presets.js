.pragma library

// Dock style presets. Each preset is split into independently applicable groups so the
// Presets page can apply e.g. only the look while keeping the user's physics.
// Keys are Plasmoid.configuration entry names (see config/main.xml).
// Spacing rule: the pill sits (edgePadding - pillPaddingCross) px from the screen edge, so
// presets keep edgePadding = pillPaddingCross + ~5 px for a visible gap under the dock.

var groups = [
    { id: "appearance", label: "Look", defaultOn: true },
    { id: "indicators", label: "Indicators", defaultOn: true },
    { id: "layout", label: "Spacing", defaultOn: true },
    { id: "panel", label: "Panel background", defaultOn: true },
    { id: "physics", label: "Magnification and bounce", defaultOn: false }
];

var presets = [
    {
        id: "tahoe",
        name: "macOS Tahoe",
        description: "Liquid Glass with a lens-like edge, a bright rim of light and a soft shadow.",
        appearance: {
            backgroundMode: 1, blurStyle: 1, blurEnabled: true,
            pillColorMode: 0, pillCustomColor: "#1e1e2e", pillOpacity: 0.16,
            pillCornerRadius: 26, pillBorderStyle: 0, pillBorderWidth: 1, pillBorderOpacity: 0.2,
            pillShadow: true, pillShadowStrength: 1.0, glassHighlight: 1.15,
            showReflection: false
        },
        indicators: { indicatorStyle: 1, indicatorColorMode: 1, indicatorPosition: 0 },
        layout: {
            iconSpacingRatio: 0.16, edgePadding: 12, topMargin: 6,
            pillPaddingPrimary: 10, pillPaddingCross: 7
        },
        panel: { hidePanelBackground: true },
        physics: {
            magnificationEnabled: true, maxZoomPanelRatio: 1.0, influenceRadiusIcons: 2.5,
            magnificationCurve: 0, bounceEnabled: true, bounceRatio: 0.55
        }
    },
    {
        id: "sequoia",
        name: "macOS Sequoia",
        description: "Frosted glass with a crisp two-tone outline, as macOS looked before Liquid Glass.",
        appearance: {
            backgroundMode: 1, blurStyle: 0, blurEnabled: true,
            pillColorMode: 0, pillCustomColor: "#1e1e2e", pillOpacity: 0.40,
            pillCornerRadius: 18, pillBorderStyle: 2, pillBorderWidth: 1, pillBorderOpacity: 0.16,
            pillShadow: true, pillShadowStrength: 1.0, glassHighlight: 0.8,
            showReflection: false
        },
        indicators: { indicatorStyle: 1, indicatorColorMode: 1, indicatorPosition: 0 },
        layout: {
            iconSpacingRatio: 0.14, edgePadding: 11, topMargin: 6,
            pillPaddingPrimary: 9, pillPaddingCross: 6
        },
        panel: { hidePanelBackground: true },
        physics: {
            magnificationEnabled: true, maxZoomPanelRatio: 1.0, influenceRadiusIcons: 2.5,
            magnificationCurve: 0, bounceEnabled: true, bounceRatio: 0.55
        }
    },
    {
        id: "capsule",
        name: "Glass Capsule",
        description: "Fully rounded glass tinted with your accent color, with a strong rim light.",
        appearance: {
            backgroundMode: 1, blurStyle: 1, blurEnabled: true,
            pillColorMode: 2, pillCustomColor: "#1e1e2e", pillOpacity: 0.14,
            pillCornerRadius: 48, pillBorderStyle: 0, pillBorderWidth: 1, pillBorderOpacity: 0.2,
            pillShadow: true, pillShadowStrength: 1.2, glassHighlight: 1.4,
            showReflection: false
        },
        indicators: { indicatorStyle: 1, indicatorColorMode: 2, indicatorPosition: 0 },
        layout: {
            iconSpacingRatio: 0.18, edgePadding: 12, topMargin: 6,
            pillPaddingPrimary: 13, pillPaddingCross: 7
        },
        panel: { hidePanelBackground: true },
        physics: {
            magnificationEnabled: true, maxZoomPanelRatio: 1.0, influenceRadiusIcons: 2.8,
            magnificationCurve: 1, bounceEnabled: true, bounceRatio: 0.6
        }
    },
    {
        id: "plasma",
        name: "Plasma Native",
        description: "Your Plasma theme's panel look, wrapped snugly around the icons.",
        appearance: {
            backgroundMode: 2, blurStyle: 0, blurEnabled: true,
            pillColorMode: 0, pillCustomColor: "#1e1e2e", pillOpacity: 0.4,
            pillCornerRadius: 12, pillBorderStyle: 0, pillBorderWidth: 1, pillBorderOpacity: 0.2,
            pillShadow: true, pillShadowStrength: 1.0, glassHighlight: 1.0,
            showReflection: false
        },
        indicators: { indicatorStyle: 0, indicatorColorMode: 0, indicatorPosition: 0 },
        layout: {
            iconSpacingRatio: 0.16, edgePadding: 10, topMargin: 6,
            pillPaddingPrimary: 9, pillPaddingCross: 5
        },
        panel: { hidePanelBackground: true },
        physics: {
            magnificationEnabled: true, maxZoomPanelRatio: 0.85, influenceRadiusIcons: 2.5,
            magnificationCurve: 0, bounceEnabled: true, bounceRatio: 0.45
        }
    },
    {
        id: "midnight",
        name: "Midnight Solid",
        description: "A solid deep-navy bar with no blur and accent-colored lines.",
        appearance: {
            backgroundMode: 1, blurStyle: 4, blurEnabled: false,
            pillColorMode: 1, pillCustomColor: "#14141c", pillOpacity: 1.0,
            pillCornerRadius: 14, pillBorderStyle: 1, pillBorderWidth: 1, pillBorderOpacity: 0.10,
            pillShadow: true, pillShadowStrength: 1.3, glassHighlight: 0.6,
            showReflection: false
        },
        indicators: { indicatorStyle: 3, indicatorColorMode: 2, indicatorPosition: 0 },
        layout: {
            iconSpacingRatio: 0.18, edgePadding: 11, topMargin: 6,
            pillPaddingPrimary: 10, pillPaddingCross: 6
        },
        panel: { hidePanelBackground: true },
        physics: {
            magnificationEnabled: true, maxZoomPanelRatio: 0.8, influenceRadiusIcons: 2.2,
            magnificationCurve: 1, bounceEnabled: false, bounceRatio: 0.4
        }
    },
    {
        id: "neon",
        name: "Neon Aurora",
        description: "Dark smoky glass with a glowing accent outline.",
        appearance: {
            backgroundMode: 1, blurStyle: 3, blurEnabled: true,
            pillColorMode: 1, pillCustomColor: "#0b0b18", pillOpacity: 0.55,
            pillCornerRadius: 22, pillBorderStyle: 3, pillBorderWidth: 1, pillBorderOpacity: 0.35,
            pillShadow: true, pillShadowStrength: 1.0, glassHighlight: 0.6,
            showReflection: true
        },
        indicators: { indicatorStyle: 2, indicatorColorMode: 2, indicatorPosition: 0 },
        layout: {
            iconSpacingRatio: 0.2, edgePadding: 12, topMargin: 6,
            pillPaddingPrimary: 12, pillPaddingCross: 7
        },
        panel: { hidePanelBackground: true },
        physics: {
            magnificationEnabled: true, maxZoomPanelRatio: 1.0, influenceRadiusIcons: 3.0,
            magnificationCurve: 2, bounceEnabled: true, bounceRatio: 0.7
        }
    },
    {
        id: "paper",
        name: "Paper",
        description: "Warm, light acrylic with a fine grain and a soft shadow.",
        appearance: {
            backgroundMode: 1, blurStyle: 2, blurEnabled: true,
            pillColorMode: 1, pillCustomColor: "#f2efe8", pillOpacity: 0.78,
            pillCornerRadius: 16, pillBorderStyle: 2, pillBorderWidth: 1, pillBorderOpacity: 0.3,
            pillShadow: true, pillShadowStrength: 1.4, glassHighlight: 0.5,
            showReflection: false
        },
        indicators: { indicatorStyle: 0, indicatorColorMode: 1, indicatorPosition: 0 },
        layout: {
            iconSpacingRatio: 0.16, edgePadding: 11, topMargin: 6,
            pillPaddingPrimary: 10, pillPaddingCross: 6
        },
        panel: { hidePanelBackground: true },
        physics: {
            magnificationEnabled: true, maxZoomPanelRatio: 0.9, influenceRadiusIcons: 2.5,
            magnificationCurve: 0, bounceEnabled: true, bounceRatio: 0.5
        }
    },
    {
        id: "floating",
        name: "Floating Icons",
        description: "No background at all. Icons float over the wallpaper with a reflection.",
        appearance: {
            backgroundMode: 3, blurStyle: 0, blurEnabled: false,
            pillColorMode: 0, pillCustomColor: "#1e1e2e", pillOpacity: 0.2,
            pillCornerRadius: 18, pillBorderStyle: 0, pillBorderWidth: 1, pillBorderOpacity: 0.2,
            pillShadow: false, pillShadowStrength: 1.0, glassHighlight: 1.0,
            showReflection: true
        },
        indicators: { indicatorStyle: 3, indicatorColorMode: 0, indicatorPosition: 0 },
        layout: {
            iconSpacingRatio: 0.22, edgePadding: 8, topMargin: 6,
            pillPaddingPrimary: 8, pillPaddingCross: 4
        },
        panel: { hidePanelBackground: true },
        physics: {
            magnificationEnabled: true, maxZoomPanelRatio: 1.0, influenceRadiusIcons: 3.0,
            magnificationCurve: 0, bounceEnabled: true, bounceRatio: 0.6
        }
    }
];

/// Flattened values of the selected groups of a preset.
function valuesFor(preset, enabledGroups) {
    var out = {};
    for (var i = 0; i < groups.length; ++i) {
        var g = groups[i].id;
        if (!enabledGroups[g] || !preset[g]) {
            continue;
        }
        for (var key in preset[g]) {
            out[key] = preset[g][key];
        }
    }
    return out;
}

/// Presets drawing a Liquid Glass surface need the Glass KWin effect (kwin-effects-glass).
function requiresGlassEffect(preset) {
    return preset.appearance.backgroundMode === 1 && preset.appearance.blurStyle === 1;
}

/// All values of a preset (used for previews).
function allValues(preset) {
    var all = {};
    for (var i = 0; i < groups.length; ++i) {
        all[groups[i].id] = true;
    }
    return valuesFor(preset, all);
}
