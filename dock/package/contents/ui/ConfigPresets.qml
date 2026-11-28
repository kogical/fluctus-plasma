import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.plasma.plasmoid

import "Presets.js" as Presets

KCM.SimpleKCM {
    id: page

    // Every key a preset can write must be declared here so Plasma loads and saves it.
    property string cfg_activePreset
    property int cfg_backgroundMode
    property int cfg_blurStyle
    property bool cfg_blurEnabled
    property int cfg_pillColorMode
    property color cfg_pillCustomColor
    property real cfg_pillOpacity
    property int cfg_pillCornerRadius
    property int cfg_pillBorderStyle
    property int cfg_pillBorderWidth
    property real cfg_pillBorderOpacity
    property bool cfg_pillShadow
    property real cfg_pillShadowStrength
    property real cfg_glassHighlight
    property int cfg_pillPaddingPrimary
    property int cfg_pillPaddingCross
    property bool cfg_showReflection
    property int cfg_indicatorStyle
    property int cfg_indicatorColorMode
    property int cfg_indicatorPosition
    property real cfg_iconSpacingRatio
    property int cfg_edgePadding
    property int cfg_topMargin
    property bool cfg_hidePanelBackground
    property bool cfg_magnificationEnabled
    property real cfg_maxZoomPanelRatio
    property real cfg_influenceRadiusIcons
    property int cfg_magnificationCurve
    property bool cfg_bounceEnabled
    property real cfg_bounceRatio

    property var enabledGroups: {
        const g = {};
        for (const group of Presets.groups) {
            g[group.id] = group.defaultOn;
        }
        return g;
    }
    property bool lightWallpaper: false

    /// Preset picked on this page; nothing is written until Apply/OK.
    property var selectedPreset: null
    /// Read by Plasma's settings window: enables Apply while the selection would change something.
    property bool unsavedChanges: false

    function sameValue(a, b) {
        if (typeof a === "number" && typeof b === "number") {
            return Math.abs(a - b) < 1e-6;
        }
        return String(a) === String(b);
    }

    /// Values the selected preset would write with the ticked parts.
    function pendingValues() {
        if (!selectedPreset) {
            return {};
        }
        const values = Presets.valuesFor(selectedPreset, enabledGroups);
        if (enabledGroups.appearance) {
            // The dock only "is" a preset when its look comes from it.
            values.activePreset = selectedPreset.id;
        }
        return values;
    }

    function refreshPending() {
        const values = pendingValues();
        let changed = false;
        for (const key in values) {
            if (!sameValue(values[key], Plasmoid.configuration[key])) {
                changed = true;
                break;
            }
        }
        unsavedChanges = changed;
    }

    function selectPreset(preset) {
        selectedPreset = preset;
        refreshPending();
    }

    // Called by Plasma's settings window when Apply or OK is pressed, before it saves cfg_ values.
    function saveConfig() {
        const values = pendingValues();
        for (const key in values) {
            const prop = "cfg_" + key;
            if (prop in page) {
                page[prop] = values[key];
            }
        }
        unsavedChanges = false;
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("Pick a style to start from. You can fine-tune everything afterwards on the other pages.")
        }

        GlassEffectNotice {
            visible: effectState !== 2 && Presets.presets.some(p => Presets.requiresGlassEffect(p))
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                text: i18n("Apply:")
            }
            Flow {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing
                Repeater {
                    model: Presets.groups
                    QQC2.CheckBox {
                        required property var modelData
                        text: i18n(modelData.label)
                        checked: page.enabledGroups[modelData.id]
                        onToggled: {
                            const g = Object.assign({}, page.enabledGroups);
                            g[modelData.id] = checked;
                            page.enabledGroups = g;
                            page.refreshPending();
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            ConfigHint {
                Layout.alignment: Qt.AlignVCenter
                text: i18n("Magnification and bounce stays unchecked by default, so a preset keeps the way your dock moves.")
            }
            Item { Layout.fillWidth: true }
            QQC2.Switch {
                text: i18n("Light wallpaper")
                checked: page.lightWallpaper
                onToggled: page.lightWallpaper = checked
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.selectedPreset !== null
            type: page.unsavedChanges ? Kirigami.MessageType.Positive : Kirigami.MessageType.Information
            text: {
                if (!page.selectedPreset) {
                    return "";
                }
                const name = i18n(page.selectedPreset.name);
                if (page.unsavedChanges) {
                    return i18n("%1 is selected. Press Apply to use it.", name);
                }
                return Presets.groups.every(g => page.enabledGroups[g.id])
                    ? i18n("Your dock already looks like %1, so there is nothing to apply.", name)
                    : i18n("Your dock already matches the ticked parts of %1. Tick more parts above to bring in the rest.", name);
            }
        }

        GridLayout {
            id: grid
            Layout.fillWidth: true
            columns: Math.max(1, Math.floor(width / (Kirigami.Units.gridUnit * 17)))
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.largeSpacing

            Repeater {
                model: Presets.presets

                QQC2.ItemDelegate {
                    id: card
                    required property var modelData
                    readonly property var v: Presets.allValues(modelData)
                    readonly property bool current: page.selectedPreset
                        ? page.selectedPreset.id === modelData.id
                        : Plasmoid.configuration.activePreset === modelData.id
                    readonly property bool inUse: Plasmoid.configuration.activePreset === modelData.id

                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 17
                    padding: Kirigami.Units.smallSpacing
                    highlighted: current
                    // Liquid Glass presets need the Glass KWin effect (see the notice above).
                    readonly property bool locked: Presets.requiresGlassEffect(modelData) && Plasmoid.glassEffectState !== 2
                    enabled: !locked
                    onClicked: page.selectPreset(modelData)

                    Accessible.name: modelData.name
                    Accessible.description: modelData.description

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 6
                            radius: Kirigami.Units.cornerRadius
                            color: "transparent"
                            border.width: card.current ? 2 : 1
                            border.color: card.current ? Kirigami.Theme.highlightColor
                                : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.15)
                            clip: true

                            DockPreview {
                                anchors.fill: parent
                                anchors.margins: parent.border.width
                                iconCount: 6
                                wallpaper: page.lightWallpaper ? 1 : 0
                                backgroundMode: card.v.backgroundMode
                                blurStyle: card.v.blurStyle
                                pillColorMode: card.v.pillColorMode
                                pillCustomColor: card.v.pillCustomColor
                                pillOpacity: card.v.pillOpacity
                                pillCornerRadius: card.v.pillCornerRadius
                                pillBorderStyle: card.v.pillBorderStyle
                                pillBorderWidth: card.v.pillBorderWidth
                                pillBorderOpacity: card.v.pillBorderOpacity
                                pillShadow: card.v.pillShadow
                                pillShadowStrength: card.v.pillShadowStrength
                                glassHighlight: card.v.glassHighlight
                                pillPaddingPrimary: card.v.pillPaddingPrimary
                                pillPaddingCross: card.v.pillPaddingCross
                                showReflection: card.v.showReflection
                                indicatorStyle: card.v.indicatorStyle
                                indicatorColorMode: card.v.indicatorColorMode
                                indicatorPosition: card.v.indicatorPosition
                                iconSpacingRatio: card.v.iconSpacingRatio
                                edgePadding: card.v.edgePadding
                                hidePanelBackground: card.v.hidePanelBackground
                                blurEnabled: card.v.blurEnabled
                                blurStrength: Plasmoid.systemBlurStrength || 10
                                compositorRefraction: Plasmoid.glassEffectState === 2
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Kirigami.Heading {
                                Layout.fillWidth: true
                                level: 4
                                text: i18n(card.modelData.name)
                                elide: Text.ElideRight
                            }
                            Kirigami.Icon {
                                visible: card.inUse
                                source: "checkmark"
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                            }
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font: Kirigami.Theme.smallFont
                            opacity: 0.75
                            text: i18n(card.modelData.description)
                        }
                        QQC2.Label {
                            visible: card.locked
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.neutralTextColor
                            text: i18n("Needs the Glass effect. See the note at the top of this page.")
                        }
                    }
                }
            }
        }
    }
}
