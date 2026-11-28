// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Modal Kirigami dialog letting any QML app or Plasma applet customize Fluctus Liquid Glass for
// its own surfaces: Global Default / Light / Medium / Full / Crystal / Plain Blur / Off presets,
// Reset to Global Defaults, and full material + pointer sliders with live preview.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Kogical.FluctusGlass

Kirigami.Dialog {
    id: dialog

    /// JSON profile string (bind directly to Plasmoid.configuration.glassProfile or app settings).
    property string profile: '{"preset":"global"}'

    /// Emitted when the user accepts the dialog.
    signal profileApplied(string profile)

    title: qsTr("Liquid Glass Settings")
    standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
    preferredWidth: Kirigami.Units.gridUnit * 28
    preferredHeight: Kirigami.Units.gridUnit * 32
    padding: Kirigami.Units.largeSpacing

    property string _initialProfile: '{"preset":"global"}'
    property bool _syncing: false

    readonly property var _presets: [
        { id: "global",  label: qsTr("Global Default") },
        { id: "light",   label: qsTr("Light") },
        { id: "medium",  label: qsTr("Medium") },
        { id: "full",    label: qsTr("Full") },
        { id: "crystal", label: qsTr("Crystal") },
        { id: "plain",   label: qsTr("Plain Blur") },
        { id: "none",    label: qsTr("Off") }
    ]

    GlassProfile {
        id: model
        onJsonChanged: {
            if (!dialog._syncing) {
                dialog._syncing = true;
                dialog.profile = json;
                dialog._syncing = false;
            }
        }
    }

    onProfileChanged: {
        if (!_syncing) {
            _syncing = true;
            model.loadJson(profile);
            _syncing = false;
        }
    }

    onAboutToShow: {
        _syncing = true;
        model.loadJson(profile);
        _initialProfile = model.json;
        _syncing = false;
    }

    onAccepted: {
        _initialProfile = model.json;
        profileApplied(model.json);
    }

    onRejected: {
        if (model.json !== _initialProfile) {
            _syncing = true;
            model.loadJson(_initialProfile);
            dialog.profile = _initialProfile;
            _syncing = false;
        }
    }

    ColorDialog {
        id: tintDialog
        title: qsTr("Glass Tint Color")
        selectedColor: model.tintColor
        onAccepted: model.tintColor = selectedColor
    }

    customFooterActions: [
        Kirigami.Action {
            text: qsTr("Reset to Global Defaults")
            icon.name: "edit-undo"
            enabled: !model.followGlobal
            onTriggered: model.resetToGlobal()
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            Layout.fillWidth: true
            text: qsTr("Customize Liquid Glass for this app only. Global Default tracks your Desktop Effects settings automatically.")
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }

        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: dialog._presets
                delegate: QQC2.Button {
                    required property var modelData
                    text: modelData.label
                    checkable: true
                    checked: model.preset === modelData.id
                    onClicked: model.applyPreset(modelData.id)
                }
            }
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true
            enabled: model.mode === "glass"

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: qsTr("Material")
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Overall intensity:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 2; stepSize: 0.01
                    value: model.intensity
                    onMoved: model.intensity = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.intensity * 100) + "%"
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Refraction:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 128; stepSize: 1
                    value: model.refraction
                    onMoved: model.refraction = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.refraction) + " px"
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Glass thickness:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 128; stepSize: 1
                    value: model.bezel
                    onMoved: model.bezel = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.bezel) + " px"
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Magnification:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 0.30; stepSize: 0.01
                    value: model.magnify
                    onMoved: model.magnify = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.magnify * 100) + "%"
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Color fringing:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 1; stepSize: 0.01
                    value: model.dispersion
                    onMoved: model.dispersion = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.dispersion * 100) + "%"
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Frost:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 1; stepSize: 0.01
                    value: model.frost
                    onMoved: model.frost = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.frost * 100) + "%"
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Saturation:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 3; stepSize: 0.01
                    value: model.saturation
                    onMoved: model.saturation = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.saturation * 100) + "%"
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Brightness:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0.5; to: 2; stepSize: 0.01
                    value: model.brightness
                    onMoved: model.brightness = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.brightness * 100) + "%"
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Tint:")
                spacing: Kirigami.Units.smallSpacing
                Rectangle {
                    implicitWidth: Kirigami.Units.gridUnit * 2
                    implicitHeight: Kirigami.Units.gridUnit * 1.4
                    radius: Kirigami.Units.cornerRadius
                    color: model.tintColor
                    border.width: 1
                    border.color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Button {
                    text: qsTr("Choose…")
                    icon.name: "color-picker"
                    onClicked: tintDialog.open()
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Tint strength:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 1; stepSize: 0.01
                    value: model.tintStrength
                    onMoved: model.tintStrength = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.tintStrength * 100) + "%"
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Edge highlight:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 2; stepSize: 0.01
                    value: model.highlight
                    onMoved: model.highlight = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.highlight * 100) + "%"
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Edge shading:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 1; stepSize: 0.01
                    value: model.edgeShade
                    onMoved: model.edgeShade = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.edgeShade * 100) + "%"
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Sheen:")
                Layout.fillWidth: true
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 1; stepSize: 0.01
                    value: model.sheen
                    onMoved: model.sheen = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.sheen * 100) + "%"
                }
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: qsTr("Pointer")
            }

            QQC2.CheckBox {
                Kirigami.FormData.label: qsTr("Pointer effects:")
                text: qsTr("Light and lens follow the pointer")
                checked: model.pointerEffects
                onToggled: model.pointerEffects = checked
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Lens bulge:")
                Layout.fillWidth: true
                enabled: model.pointerEffects
                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0; to: 32; stepSize: 1
                    value: model.pointerBulge
                    onMoved: model.pointerBulge = value
                }
                QQC2.Label {
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(model.pointerBulge) + " px"
                }
            }
        }
    }
}
