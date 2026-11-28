import QtQuick

import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg
import Kogical.LiquidGlass

// Shared dock surface renderer used by the live dock (main.qml) and by the configuration
// previews (DockPreview.qml), so what the settings show is exactly what the panel draws.
Item {
    id: bg

    // 0: Default (panel draws), 1: Custom Glass Pill, 2: Plasma Theme Surface, 3: Transparent
    property int backgroundMode: 1
    // 0: Frosted, 1: Liquid Glass, 2: Acrylic, 3: Clear, 4: Solid, 5: Crystal
    property int materialIndex: 1
    // 0: Plasma theme, 1: Custom, 2: Accent
    property int colorMode: 0
    property color customColor: "#1e1e2e"
    property real tintOpacity: 0.22
    property real cornerRadius: 18
    // 0: Material default, 1: Single, 2: Dual hairline, 3: Accent glow, 4: None
    property int borderStyle: 0
    property int borderWidth: 1
    property real borderOpacity: 0.22
    property bool shadowEnabled: true
    property real shadowStrength: 1.0
    property real highlightStrength: 1.0
    property bool vertical: false
    // 0: bottom, 1: top, 2: left, 3: right
    property int dockEdge: 0
    /// Optional item behind the dock to refract (previews). null = compositor blur overlay.
    property Item backdrop: null
    property int backdropRevision: 0
    /// Pointer position in the parent's coordinates and its 0..1 presence (drives the light).
    property point pointer: Qt.point(0, 0)
    property real pointerStrength: 0
    property bool interactive: true
    /// Previews only: backdrop blur amount (0..1) imitating the compositor, or -1 for the
    /// material's own recipe.
    property real previewFrost: -1
    /// Previews only: imitate the Glass KWin effect's edge refraction.
    property bool previewRefraction: false

    readonly property var materialNames: ["frosted", "liquid", "acrylic", "clear", "solid", "crystal"]
    readonly property string materialName: materialNames[Math.max(0, Math.min(5, materialIndex))]

    // ---- Fluctus Glass ----------------------------------------------------
    /// The Fluctus Glass KWin effect renders this surface: the in-app overlay steps aside.
    property bool compositorGlass: false
    /// User tuning, multipliers on the material (1 = natural look).
    property real glassIntensity: 1.0
    property real glassThickness: 1.0
    property real glassFrost: 1.0
    property real glassDispersion: 1.0

    function argb(c, alpha) {
        return Qt.rgba(c.r, c.g, c.b, alpha === undefined ? c.a : alpha).toString();
    }
    /// The material for the effect, keyed as in FluctusGlass/GlassProtocol.hpp. Built from the
    /// same values the in-app overlay uses, so previews and the compositor agree.
    readonly property var glassParameters: ({
        "intensity": bg.glassIntensity,
        "radius": glass.radius,
        "tint": argb(glass.tint, Math.max(0, Math.min(1, glass.tintAlpha))),
        "bezel": glass.bezel,
        "refraction": glass.recipe.refraction,
        "magnify": glass.magnify,
        "edgeBend": glass.edgeBend,
        "dispersion": glass.dispersion,
        "frost": glass.recipe.frost * bg.glassFrost,
        "saturation": glass.saturation,
        "brightness": glass.brightness,
        "edgeShade": glass.recipe.edgeShade,
        "grain": glass.grain,
        "highlight": glass.highlight,
        "highlightWidth": glass.highlightWidth,
        "sheen": glass.sheen,
        "lightAngle": glass.lightAngle,
        "innerBorder": argb(glass.innerBorderColor),
        "outerBorder": argb(glass.outerBorderColor),
        "borderWidth": glass.borderWidth,
        "shadowColor": argb(glass.shadowColor),
        "shadowBlur": glass.shadowBlur,
        "shadowOffsetX": glass.shadowOffsetX,
        "shadowOffsetY": glass.shadowOffsetY,
        "pointerLight": bg.interactive ? glass.pointerLight : 0,
        "pointerBulge": bg.interactive ? 6 : 0
    })

    readonly property color baseColor: {
        switch (colorMode) {
        case 1: return customColor;
        case 2: return Kirigami.Theme.highlightColor;
        default: return Kirigami.Theme.backgroundColor;
        }
    }
    function luminance(c) {
        return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
    }
    readonly property bool dark: colorMode === 2
        ? luminance(Kirigami.Theme.backgroundColor) < 0.5
        : luminance(baseColor) < 0.5

    readonly property bool showGlass: backgroundMode === 1 && width > 0 && height > 0
    readonly property bool showTheme: backgroundMode === 2 && width > 0 && height > 0

    /// KWin background-contrast for the selected material (contrast, intensity, saturation).
    readonly property var compositorContrast: {
        if (backgroundMode === 2) {
            return { contrast: 1.0, intensity: 1.0, saturation: 1.0 };
        }
        switch (materialIndex) {
        case 0: return dark ? { contrast: 1.12, intensity: 0.96, saturation: 1.5 }
                            : { contrast: 1.04, intensity: 1.10, saturation: 1.4 };
        case 1: return dark ? { contrast: 1.0, intensity: 1.05, saturation: 1.8 }
                            : { contrast: 0.96, intensity: 1.12, saturation: 1.65 };
        case 2: return { contrast: 1.05, intensity: 1.0, saturation: 1.25 };
        case 3: return { contrast: 1.0, intensity: 1.0, saturation: 1.3 };
        default: return { contrast: 1.0, intensity: 1.0, saturation: 1.0 };
        }
    }
    /// Whether compositor blur makes sense for the current surface.
    readonly property bool wantsBlur: (backgroundMode === 1 && materialIndex !== 4) || backgroundMode === 2
    /// Plasma FrameSvg mask for blur in theme-surface mode (QRegion, item-local).
    readonly property var themeMask: themeSurface.mask

    // Light comes from above the dock for bottom docks; from the screen side for others.
    readonly property real lightAngle: {
        switch (dockEdge) {
        case 1: return -128;
        case 2: return -150;
        case 3: return -30;
        default: return -128;
        }
    }

    KSvg.FrameSvgItem {
        id: themeSurface
        anchors.fill: parent
        imagePath: "widgets/panel-background"
        visible: bg.showTheme
    }

    LiquidGlass {
        id: glass
        anchors.fill: parent
        visible: bg.showGlass && !bg.compositorGlass
        material: bg.materialName
        intensity: bg.glassIntensity
        magnify: recipe.magnify ?? 0
        dark: bg.dark
        radius: Math.min(bg.cornerRadius, Math.min(width, height) * 0.5)
        lightAngle: bg.lightAngle
        backdrop: bg.backdrop
        backdropRevision: bg.backdropRevision
        pointer: Qt.point(bg.pointer.x - bg.x, bg.pointer.y - bg.y)
        pointerStrength: bg.interactive ? bg.pointerStrength : 0
        // The real dock sits on compositor blur. Stock KWin blur cannot bend what is behind it;
        // the Glass KWin effect refracts at the edges, so previews imitate whichever is active.
        frost: bg.previewFrost >= 0 ? bg.previewFrost : Math.min(1, recipe.frost * bg.glassFrost)
        refraction: bg.previewFrost < 0 ? recipe.refraction : (bg.previewRefraction ? 12 : 0)
        dispersion: Math.min(1, (bg.previewFrost < 0 ? recipe.dispersion : (bg.previewRefraction ? 0.15 : 0)) * bg.glassDispersion)
        // Over compositor blur the lens darkening band only adds murk on dark wallpapers.
        edgeShade: recipe.edgeShade * 0.5

        // Vertical docks: lens bezel follows the shorter side.
        bezel: Math.min(recipe.bezel * bg.glassThickness, Math.min(width, height) * 0.45)
        tint: bg.baseColor
        tintAlpha: bg.tintOpacity
        highlight: recipe.highlight * bg.highlightStrength
        sheen: recipe.sheen * Math.min(1.5, bg.highlightStrength)

        readonly property color accent: Kirigami.Theme.highlightColor
        borderWidth: bg.borderStyle === 0 ? recipe.borderWidth : bg.borderWidth
        innerBorderColor: {
            if (bg.borderWidth <= 0 && bg.borderStyle !== 0) {
                return "transparent";
            }
            switch (bg.borderStyle) {
            case 1: return Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, bg.borderOpacity);
            case 2: return Qt.rgba(1, 1, 1, bg.borderOpacity);
            case 3: return Qt.rgba(accent.r, accent.g, accent.b, Math.min(1, bg.borderOpacity + 0.35));
            case 4: return "transparent";
            default: return recipe.innerBorderColor;
            }
        }
        outerBorderColor: {
            if (bg.borderWidth <= 0 && bg.borderStyle !== 0) {
                return "transparent";
            }
            switch (bg.borderStyle) {
            case 2: return Qt.rgba(0, 0, 0, Math.min(1, bg.borderOpacity * 1.8));
            case 3: return Qt.rgba(accent.r, accent.g, accent.b, Math.min(1, bg.borderOpacity + 0.1));
            case 1:
            case 4: return "transparent";
            default: return recipe.outerBorderColor;
            }
        }
        shadowColor: {
            if (!bg.shadowEnabled) {
                return "transparent";
            }
            if (bg.borderStyle === 3) {
                return Qt.rgba(accent.r, accent.g, accent.b, Math.min(1, 0.55 * bg.shadowStrength));
            }
            const c = recipe.shadowColor;
            return Qt.rgba(c.r, c.g, c.b, Math.min(1, c.a * bg.shadowStrength));
        }
        shadowBlur: bg.borderStyle === 3 ? 16 : recipe.shadowBlur
        shadowOffsetY: bg.borderStyle === 3 ? 0 : (bg.dockEdge === 1 ? recipe.shadowOffsetY : recipe.shadowOffsetY * 0.5)
    }
}
