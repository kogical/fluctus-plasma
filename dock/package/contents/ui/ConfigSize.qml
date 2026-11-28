import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.plasma.plasmoid

KCM.SimpleKCM {
    id: page

    property alias cfg_panelIconRatio: iconSizeSlider.value
    property alias cfg_iconSpacingRatio: spacingSlider.value
    property alias cfg_pillPaddingPrimary: endPaddingSpin.value
    property alias cfg_pillPaddingCross: sidePaddingSpin.value
    property alias cfg_edgePadding: edgeSpin.value
    property alias cfg_topMargin: topRoomSpin.value
    property alias cfg_targetIconSize: targetIconSpin.value

    // ---- How the dock fills the panel ----------------------------------------
    // icon size = icon-size % × (space the panel gives the dock − edge distance − room above)
    readonly property bool hasSurface: Plasmoid.configuration.backgroundMode === 1 || Plasmoid.configuration.backgroundMode === 2
    readonly property int panelHeight: Plasmoid.panelThickness || 0
    readonly property real dockCross: Plasmoid.dockCrossExtent || 0
    readonly property real panelPadding: panelHeight > 0 && dockCross > 0 ? Math.max(0, panelHeight - dockCross) : 0
    readonly property real usable: Math.max(0, dockCross - edgeSpin.value - topRoomSpin.value)
    readonly property real ratio: Math.max(0.25, iconSizeSlider.value)
    readonly property real zoomRatio: Plasmoid.configuration.magnificationEnabled
        ? Math.max(ratio, Plasmoid.configuration.maxZoomPanelRatio) : ratio
    readonly property int iconPx: Math.round(ratio * usable)
    readonly property int zoomPx: Math.round(zoomRatio * usable)
    readonly property int recommendedHeight: Math.ceil(targetIconSpin.value / ratio
        + edgeSpin.value + topRoomSpin.value + panelPadding)
    readonly property int recommendedZoomPx: Math.round(targetIconSpin.value * zoomRatio / ratio)
    // The launch bounce needs free space above a resting icon: ratio ≤ 1 / (1 + bounce height).
    readonly property real bounceRatio: Plasmoid.configuration.bounceRatio
    readonly property bool bounceClipped: Plasmoid.configuration.bounceEnabled
        && iconSizeSlider.value > 1.0 / (1.0 + bounceRatio) + 0.001
    readonly property int bounceFriendlyPct: Math.floor(100 / (1.0 + bounceRatio) / 5) * 5
    readonly property int gapUnderDock: Math.max(0, edgeSpin.value - (hasSurface ? sidePaddingSpin.value : 0))

    header: ColumnLayout {
        spacing: 0

        DockPreview {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 8
            // Look settings as saved; size settings live from this page.
            backgroundMode: Plasmoid.configuration.backgroundMode
            blurStyle: Plasmoid.configuration.blurStyle
            pillColorMode: Plasmoid.configuration.pillColorMode
            pillCustomColor: Plasmoid.configuration.pillCustomColor
            pillOpacity: Plasmoid.configuration.pillOpacity
            pillCornerRadius: Plasmoid.configuration.pillCornerRadius
            pillBorderStyle: Plasmoid.configuration.pillBorderStyle
            pillBorderWidth: Plasmoid.configuration.pillBorderWidth
            pillBorderOpacity: Plasmoid.configuration.pillBorderOpacity
            pillShadow: Plasmoid.configuration.pillShadow
            pillShadowStrength: Plasmoid.configuration.pillShadowStrength
            glassHighlight: Plasmoid.configuration.glassHighlight
            showReflection: Plasmoid.configuration.showReflection
            indicatorStyle: Plasmoid.configuration.indicatorStyle
            indicatorColorMode: Plasmoid.configuration.indicatorColorMode
            indicatorPosition: Plasmoid.configuration.indicatorPosition
            hidePanelBackground: Plasmoid.configuration.hidePanelBackground
            blurEnabled: Plasmoid.configuration.blurEnabled
            blurStrength: Plasmoid.systemBlurStrength || 10
            compositorRefraction: Plasmoid.glassEffectState === 2
            pillPaddingPrimary: endPaddingSpin.value
            pillPaddingCross: sidePaddingSpin.value
            iconSpacingRatio: spacingSlider.value
            edgePadding: edgeSpin.value
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }
    }

    Kirigami.FormLayout {
        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Panel Height")
        }

        QQC2.Label {
            visible: page.panelHeight > 0
            Kirigami.FormData.label: i18n("Right now:")
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
            wrapMode: Text.WordWrap
            text: i18n("The panel is %1 px tall, so icons are %2 px and grow to %3 px.",
                page.panelHeight, page.iconPx, page.zoomPx)
        }

        RowLayout {
            visible: page.panelHeight > 0
            Kirigami.FormData.label: i18n("Icon size you want:")
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: [
                    { label: i18n("Small"), size: 32 },
                    { label: i18n("Medium"), size: 40 },
                    { label: i18n("Large"), size: 52 }
                ]
                QQC2.Button {
                    required property var modelData
                    text: modelData.label
                    checkable: true
                    checked: targetIconSpin.value === modelData.size
                    onClicked: targetIconSpin.value = modelData.size
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: i18n("%1 px icons", modelData.size)
                }
            }

            QQC2.SpinBox {
                id: targetIconSpin
                from: 24
                to: 128
                stepSize: 2
                textFromValue: function(value) { return i18n("%1 px", value); }
                valueFromText: function(text) { return parseInt(text, 10) || 40; }
            }
        }

        RowLayout {
            visible: page.panelHeight > 0
            Kirigami.FormData.label: i18n("Best panel height:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                text: i18n("%1 px", page.recommendedHeight)
                font.bold: true
            }
            QQC2.Button {
                icon.name: page.recommendedHeight === page.panelHeight ? "checkmark" : "transform-scale-symbolic"
                text: page.recommendedHeight === page.panelHeight ? i18n("Already set") : i18n("Use this height")
                enabled: page.recommendedHeight !== page.panelHeight
                onClicked: Plasmoid.setPanelThickness(page.recommendedHeight)
            }
        }

        ConfigHint {
            visible: page.panelHeight > 0
            text: i18n("At %1 px your icons are %2 px and grow to %3 px when magnified. Medium suits most 1080p screens; Large works well on bigger or high-resolution displays. Using it changes the panel's own height setting right away.",
                page.recommendedHeight, targetIconSpin.value, page.recommendedZoomPx)
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.panelHeight > 0 && page.bounceClipped
            type: Kirigami.MessageType.Information
            text: i18n("At this icon size there is not enough room above the icons for the full launch bounce. An icon size of %1% leaves enough room.", page.bounceFriendlyPct)
            actions: [
                Kirigami.Action {
                    text: i18n("Use %1%", page.bounceFriendlyPct)
                    onTriggered: iconSizeSlider.value = page.bounceFriendlyPct / 100
                }
            ]
        }

        ConfigHint {
            visible: page.panelHeight <= 0
            text: i18n("Put the dock in a panel to get a height recommendation.")
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Icons")
        }

        ConfigSlider {
            id: iconSizeSlider
            Kirigami.FormData.label: i18n("Icon size:")
            from: 0.25
            to: 1.0
            stepSize: 0.05
        }

        ConfigHint {
            text: i18n("Share of the panel height the icons use at rest. The rest is room for them to grow into when magnified.")
        }

        ConfigSlider {
            id: spacingSlider
            Kirigami.FormData.label: i18n("Space between icons:")
            from: 0
            to: 0.5
            stepSize: 0.02
        }

        ConfigHint {
            text: i18n("Measured as a share of the icon size, so it scales with the dock.")
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Padding")
        }

        QQC2.SpinBox {
            id: endPaddingSpin
            enabled: page.hasSurface
            Kirigami.FormData.label: i18n("At the ends:")
            from: 0
            to: 40
            textFromValue: function(value) { return i18n("%1 px", value); }
            valueFromText: function(text) { return parseInt(text, 10) || 0; }
        }

        QQC2.SpinBox {
            id: sidePaddingSpin
            enabled: page.hasSurface
            Kirigami.FormData.label: i18n("Above and below icons:")
            from: 0
            to: 24
            textFromValue: function(value) { return i18n("%1 px", value); }
            valueFromText: function(text) { return parseInt(text, 10) || 0; }
        }

        ConfigHint {
            text: page.hasSurface
                ? i18n("Space inside the dock background, around the icons.")
                : i18n("Only used when the dock has a background (Appearance page).")
        }

        QQC2.SpinBox {
            id: edgeSpin
            Kirigami.FormData.label: i18n("Icons to screen edge:")
            from: 0
            to: 32
            textFromValue: function(value) { return i18n("%1 px", value); }
            valueFromText: function(text) { return parseInt(text, 10) || 0; }
        }

        ConfigHint {
            text: page.hasSurface
                ? (page.gapUnderDock > 0
                    ? i18n("Leaves a %1 px gap between the dock and the screen edge.", page.gapUnderDock)
                    : i18n("The dock touches the screen edge. Make this larger than the padding above and below the icons to leave a gap."))
                : i18n("Distance from the screen edge to the icons.")
        }

        QQC2.SpinBox {
            id: topRoomSpin
            Kirigami.FormData.label: i18n("Free space at the top:")
            from: 0
            to: 32
            textFromValue: function(value) { return i18n("%1 px", value); }
            valueFromText: function(text) { return parseInt(text, 10) || 0; }
        }

        ConfigHint {
            text: i18n("Kept empty at the far side of the panel so magnified and bouncing icons never touch its edge.")
        }
    }
}
