import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg

// Live, self-contained dock preview for the settings pages.
// Renders a sample wallpaper, an optional simulated Plasma panel, the real DockBackground
// (imitating the compositor's blur, and the Glass effect's refraction when it is active) and
// sample icons + running indicators.
Item {
    id: preview

    // Style inputs (same names as the config keys).
    property int backgroundMode: 1
    property int blurStyle: 1
    property int pillColorMode: 0
    property color pillCustomColor: "#1e1e2e"
    property real pillOpacity: 0.22
    property real pillCornerRadius: 18
    property int pillBorderStyle: 0
    property int pillBorderWidth: 1
    property real pillBorderOpacity: 0.22
    property bool pillShadow: true
    property real pillShadowStrength: 1.0
    property real glassHighlight: 1.0
    property real glassIntensity: 1.0
    property real glassThickness: 1.0
    property real glassFrost: 1.0
    property real glassDispersion: 1.0
    /// Fluctus Glass renders the real dock: preview its exact material instead of imitating blur.
    property bool nativeGlass: false
    property real pillPaddingPrimary: 10
    property real pillPaddingCross: 6
    property bool showReflection: false
    property int indicatorStyle: 0
    property int indicatorColorMode: 0
    property int indicatorPosition: 0
    property real iconSpacingRatio: 0.15
    property real edgePadding: 6
    property bool hidePanelBackground: false
    property bool blurEnabled: true
    /// KWin's blur strength (1..15); the preview imitates it so it matches the real panel.
    property int blurStrength: 10
    /// The Glass KWin effect is active, so the compositor refracts behind the dock.
    property bool compositorRefraction: false
    property bool interactive: true

    // Hovering the preview drives the same pointer light / lens as the real dock.
    HoverHandler {
        id: previewHover
    }
    property point lastPointer: Qt.point(width / 2, height / 2)
    Connections {
        target: previewHover
        function onPointChanged() {
            if (previewHover.hovered) {
                preview.lastPointer = previewHover.point.position;
            }
        }
    }
    property real hoverStrength: previewHover.hovered ? 1 : 0
    Behavior on hoverStrength {
        NumberAnimation { duration: 320; easing.type: Easing.OutCubic }
    }

    /// Sample wallpaper: 0 = dusk (dark), 1 = daylight (light)
    property int wallpaper: 0
    property real iconSize: Math.round(Math.min(48, Math.max(22, height * 0.30)))
    property int iconCount: 7
    property bool showPanelFrame: true

    implicitWidth: 520
    implicitHeight: 150
    clip: true

    readonly property var sampleIcons: [
        "systemsettings", "system-file-manager", "internet-web-browser",
        "utilities-terminal", "org.kde.kate", "multimedia-player", "mail-client",
        "org.kde.discover", "accessories-calculator"
    ]
    readonly property var runningMask: [true, true, true, false, true, false, false, true, false]
    readonly property int activeIndex: 2

    readonly property real spacing: Math.round(iconSize * iconSpacingRatio)
    readonly property bool usesPill: backgroundMode === 1 || backgroundMode === 2
    readonly property real padP: backgroundMode === 0 ? 4 : pillPaddingPrimary
    readonly property real padC: backgroundMode === 0 ? 0 : pillPaddingCross
    readonly property real rowWidth: iconCount * iconSize + (iconCount - 1) * spacing
    readonly property real pillWidth: rowWidth + 2 * padP
    readonly property real pillHeight: iconSize + 2 * padC
    // Mirrors DockPhysicsEngine: the pill starts (edgePadding - pillPaddingCross) from the edge.
    readonly property real dockBottom: height - Math.max(0, edgePadding - padC)

    // ---- Sample wallpaper ------------------------------------------------
    Item {
        id: wall
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: preview.wallpaper === 0 ? "#1d1838" : "#9fc6ee" }
                GradientStop { position: 0.6; color: preview.wallpaper === 0 ? "#3a2457" : "#e7d3e8" }
                GradientStop { position: 1.0; color: preview.wallpaper === 0 ? "#12233f" : "#f6e2c7" }
            }
        }
        Repeater {
            model: 9
            Rectangle {
                required property int index
                readonly property real s: preview.height * (0.35 + (index % 3) * 0.22)
                x: (index * 0.137 % 1.0) * preview.width - s * 0.3
                y: preview.height * ((index * 0.31) % 1.0) - s * 0.25
                width: s
                height: s
                radius: s / 2
                opacity: preview.wallpaper === 0 ? 0.85 : 0.75
                color: preview.wallpaper === 0
                    ? Qt.hsla((0.62 + index * 0.09) % 1.0, 0.7, 0.55, 1)
                    : Qt.hsla((0.95 + index * 0.11) % 1.0, 0.65, 0.68, 1)
            }
        }
        // A sharp shape so refraction is visible through the glass.
        Rectangle {
            x: preview.width * 0.58
            y: preview.dockBottom - preview.pillHeight * 0.75
            width: preview.width * 0.08
            height: preview.pillHeight * 1.6
            rotation: 18
            color: preview.wallpaper === 0 ? "#ffb347" : "#ff6f61"
        }
    }

    // ---- Simulated Plasma panel background --------------------------------
    KSvg.FrameSvgItem {
        id: panelFrame
        visible: preview.showPanelFrame && !preview.hidePanelBackground
        imagePath: "widgets/panel-background"
        prefix: ["floating", ""]
        x: (preview.width - width) / 2
        width: Math.min(preview.width - 16, preview.pillWidth + preview.iconSize * 1.6)
        height: Math.min(preview.height - 8, preview.pillHeight + preview.iconSize * 0.9)
        y: preview.height - height - 4
        opacity: 0.95
    }

    // ---- Dock surface -----------------------------------------------------
    DockBackground {
        id: surface
        visible: preview.usesPill
        width: preview.pillWidth
        height: preview.pillHeight
        x: Math.round((preview.width - width) / 2)
        y: Math.round(preview.dockBottom - height)
        backgroundMode: preview.backgroundMode
        materialIndex: preview.blurStyle
        colorMode: preview.pillColorMode
        customColor: preview.pillCustomColor
        tintOpacity: preview.pillOpacity
        cornerRadius: preview.pillCornerRadius
        borderStyle: preview.pillBorderStyle
        borderWidth: preview.pillBorderWidth
        borderOpacity: preview.pillBorderOpacity
        shadowEnabled: preview.pillShadow
        shadowStrength: preview.pillShadowStrength
        highlightStrength: preview.glassHighlight
        glassIntensity: preview.glassIntensity
        glassThickness: preview.glassThickness
        glassFrost: preview.glassFrost
        glassDispersion: preview.glassDispersion
        backdrop: wall
        interactive: preview.interactive
        pointer: preview.lastPointer
        pointerStrength: preview.hoverStrength
        previewRefraction: preview.compositorRefraction
        previewFrost: preview.nativeGlass ? -1
            : (preview.blurEnabled && preview.blurStyle !== 4
                ? 0.12 + Math.max(1, Math.min(15, preview.blurStrength)) / 15 * 0.8
                : 0)
        backdropRevision: Math.round(preview.width) + Math.round(preview.height) * 10000
    }

    // ---- Sample icons ------------------------------------------------------
    Row {
        id: iconRow
        spacing: preview.spacing
        x: Math.round((preview.width - preview.rowWidth) / 2)
        y: Math.round(preview.dockBottom - preview.padC - preview.iconSize)

        Repeater {
            model: preview.iconCount
            Item {
                id: slot
                required property int index
                width: preview.iconSize
                height: preview.iconSize
                readonly property bool running: preview.runningMask[index % preview.runningMask.length]
                readonly property bool active: index === preview.activeIndex

                Kirigami.Icon {
                    anchors.fill: parent
                    anchors.margins: 1
                    source: preview.sampleIcons[slot.index % preview.sampleIcons.length]
                }

                Kirigami.Icon {
                    visible: preview.showReflection
                    width: parent.width
                    height: parent.height
                    y: parent.height + 1
                    opacity: 0.22
                    source: preview.sampleIcons[slot.index % preview.sampleIcons.length]
                    transform: Scale { origin.y: preview.iconSize * 0.5; yScale: -1 }
                }

                Rectangle {
                    id: ind
                    visible: preview.indicatorStyle !== 4 && slot.running
                    readonly property real b: preview.iconSize
                    readonly property real len: {
                        switch (preview.indicatorStyle) {
                        case 1: return Math.max(4, b * 0.10);
                        case 2: return slot.active ? b * 0.42 : b * 0.22;
                        case 3: return slot.active ? b * 0.30 : b * 0.16;
                        default: return slot.active ? b * 0.24 : b * 0.11;
                        }
                    }
                    readonly property real thick: {
                        switch (preview.indicatorStyle) {
                        case 1: return len;
                        case 2: return Math.max(3, b * 0.065);
                        case 3: return 2;
                        default: return Math.max(3, b * 0.08);
                        }
                    }
                    width: Math.round(len)
                    height: Math.round(thick)
                    radius: height / 2
                    x: Math.round((preview.iconSize - width) / 2)
                    y: preview.indicatorPosition === 1
                        ? preview.iconSize - height - 3
                        : preview.iconSize + Math.max(1, Math.round((preview.padC - height) / 2))
                    color: {
                        switch (preview.indicatorColorMode) {
                        case 1: return preview.wallpaper === 0 || surface.dark ? "#e6e6e6" : "#3a3a3a";
                        case 2: return Kirigami.Theme.highlightColor;
                        default: return slot.active ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor;
                        }
                    }
                    opacity: 0.95

                    Rectangle {
                        visible: preview.indicatorStyle === 2
                        anchors.centerIn: parent
                        width: parent.width + 6
                        height: parent.height + 4
                        radius: height / 2
                        color: parent.color
                        opacity: slot.active ? 0.35 : 0.18
                        z: -1
                    }
                }
            }
        }
    }
}
