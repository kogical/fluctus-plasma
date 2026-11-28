// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Presets page: whole looks to start from, each with a live preview. Selecting only selects;
// Apply writes it (see docs/preferences-guidelines.md).
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.plasma.plasmoid

import "Presets.js" as Presets

KCM.SimpleKCM {
    id: page

    // Every key a preset writes must be declared so Plasma loads and saves it.
    property string cfg_activePreset
    property int cfg_widgetStyle
    property int cfg_surfaceMaterial
    property int cfg_cornerRadius

    property bool lightWallpaper: false
    readonly property bool liquidAvailable: Plasmoid.glassEffectState === 2

    /// Preset picked on this page; nothing is written until Apply/OK.
    property var selectedPreset: null
    /// Read by Plasma's settings window: enables Apply while the selection would change something.
    property bool unsavedChanges: false

    function selectPreset(preset) {
        selectedPreset = preset;
        let changed = preset.id !== Plasmoid.configuration.activePreset;
        for (const key in preset.values) {
            if (preset.values[key] !== Plasmoid.configuration[key]) {
                changed = true;
            }
        }
        unsavedChanges = changed;
    }

    // Called by Plasma's settings window on Apply/OK, before it saves the cfg_ values.
    function saveConfig() {
        if (!selectedPreset) {
            return;
        }
        for (const key in selectedPreset.values) {
            page["cfg_" + key] = selectedPreset.values[key];
        }
        cfg_activePreset = selectedPreset.id;
        unsavedChanges = false;
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("Pick a style to start from. You can fine-tune it afterwards on the Appearance page.")
        }

        GlassEffectNotice {}

        RowLayout {
            Layout.fillWidth: true
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
            text: !page.selectedPreset ? ""
                : page.unsavedChanges
                    ? i18n("%1 is selected. Press Apply to use it.", i18n(page.selectedPreset.name))
                    : i18n("The widget already looks like %1, so there is nothing to apply.", i18n(page.selectedPreset.name))
        }

        GridLayout {
            Layout.fillWidth: true
            columns: Math.max(1, Math.floor(width / (Kirigami.Units.gridUnit * 17)))
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.largeSpacing

            Repeater {
                model: Presets.presets

                QQC2.ItemDelegate {
                    id: card
                    required property var modelData
                    readonly property bool current: page.selectedPreset
                        ? page.selectedPreset.id === modelData.id
                        : Plasmoid.configuration.activePreset === modelData.id
                    readonly property bool inUse: Plasmoid.configuration.activePreset === modelData.id
                    readonly property bool fallsBack: Presets.usesLiquidGlass(modelData) && !page.liquidAvailable

                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 17
                    padding: Kirigami.Units.smallSpacing
                    highlighted: current
                    onClicked: page.selectPreset(modelData)

                    Accessible.name: modelData.name
                    Accessible.description: modelData.description

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 7
                            radius: Kirigami.Units.cornerRadius
                            color: "transparent"
                            border.width: card.current ? 2 : 1
                            border.color: card.current ? Kirigami.Theme.highlightColor
                                : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.15)
                            clip: true

                            BatteryPreview {
                                anchors.fill: parent
                                anchors.margins: parent.border.width
                                widgetStyle: card.modelData.values.widgetStyle
                                material: card.modelData.values.surfaceMaterial
                                cornerRadius: card.modelData.values.cornerRadius
                                wallpaper: page.lightWallpaper ? 1 : 0
                                liquidAvailable: page.liquidAvailable
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
                            visible: card.fallsBack
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.neutralTextColor
                            text: i18n("Shows as Frosted until Fluctus Glass is running.")
                        }
                    }
                }
            }
        }
    }
}
