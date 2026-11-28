// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Devices page: which devices appear, their order, the one shown in the panel, and access for
// receivers that need a udev rule before they report a battery.
import QtQuick
import QtQml.Models
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.plasma.plasmoid

KCM.SimpleKCM {
    id: page

    property alias cfg_showStubs: showStubsCheck.checked
    property bool cfg_hideLinePower
    property alias cfg_sortOrder: sortCombo.currentIndex
    property string cfg_preferredDeviceId

    readonly property var devicesModel: Plasmoid.devicesModel
    readonly property var needingAccess: devicesModel ? devicesModel.permissionNeededNames : []
    readonly property var sortDescriptions: [
        i18n("Computers and phones first, then whatever is running lowest."),
        i18n("The device that needs charging soonest comes first."),
        i18n("In alphabetical order.")
    ]

    // Connected devices with a readable battery, for the panel choice.
    Instantiator {
        id: connected
        model: page.devicesModel
        delegate: QtObject {
            required property string deviceId
            required property string name
            required property bool powerAvailable
        }
    }
    readonly property var panelChoices: {
        const choices = [{ id: "", name: i18n("Automatic") }];
        let found = cfg_preferredDeviceId.length === 0;
        for (let i = 0; i < connected.count; ++i) {
            const d = connected.objectAt(i);
            if (d && d.powerAvailable) {
                choices.push({ id: d.deviceId, name: d.name });
                found = found || d.deviceId === cfg_preferredDeviceId;
            }
        }
        if (!found) {
            choices.push({ id: cfg_preferredDeviceId, name: i18n("A device that isn't connected") });
        }
        return choices;
    }

    Kirigami.FormLayout {
        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Which Devices Appear")
        }

        QQC2.CheckBox {
            id: showStubsCheck
            Kirigami.FormData.label: i18n("Also show:")
            text: i18n("Devices that don't report a battery level")
        }

        QQC2.CheckBox {
            text: i18n("Power adapters")
            checked: !page.cfg_hideLinePower
            onToggled: page.cfg_hideLinePower = !checked
        }

        ConfigHint {
            text: i18n("Devices that are asleep, or that need access below, always appear, so you know they are there.")
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Order")
        }

        QQC2.ComboBox {
            id: sortCombo
            Kirigami.FormData.label: i18n("Sort:")
            model: [i18n("Most important first"), i18n("Lowest battery first"), i18n("By name")]
        }

        ConfigHint {
            text: page.sortDescriptions[Math.max(0, sortCombo.currentIndex)]
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Panel")
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Shown in the panel:")
            model: page.panelChoices
            textRole: "name"
            currentIndex: Math.max(0, page.panelChoices.findIndex(c => c.id === page.cfg_preferredDeviceId))
            onActivated: page.cfg_preferredDeviceId = page.panelChoices[currentIndex].id
        }

        ConfigHint {
            text: i18n("Automatic shows the first device in the order above. You can also right-click a device in the widget and choose Show in Panel.")
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Access")
        }

        QQC2.Label {
            visible: page.needingAccess.length === 0
            Kirigami.FormData.label: i18n("Receivers:")
            text: i18n("Every connected device can be read.")
        }

        ConfigHint {
            visible: page.needingAccess.length === 0
            text: i18n("Receivers from Keychron, Logitech, Razer, Corsair and SteelSeries only report their battery with permission from a udev rule, which comes with the widget. If one can't be read, this page shows how to fix it.")
        }

        Kirigami.InlineMessage {
            id: accessMessage
            Layout.fillWidth: true
            visible: page.needingAccess.length > 0
            type: Kirigami.MessageType.Warning
            text: i18np("Fluctus can't read the battery of %2 yet. Linux only lets apps talk to this kind of receiver with permission, which a small udev rule grants. Run the command below once in a terminal (it asks for your password), then unplug the receiver and plug it back in.",
                        "Fluctus can't read the batteries of %2 yet. Linux only lets apps talk to these receivers with permission, which a small udev rule grants. Run the command below once in a terminal (it asks for your password), then unplug the receivers and plug them back in.",
                        page.needingAccess.length, page.needingAccess.join(", "))
            actions: [
                Kirigami.Action {
                    icon.name: "edit-copy"
                    text: i18n("Copy command")
                    onTriggered: Plasmoid.copyToClipboard(Plasmoid.udevRuleCommand)
                },
                Kirigami.Action {
                    icon.name: "view-refresh"
                    text: i18n("Check again")
                    onTriggered: Plasmoid.refresh()
                }
            ]
        }

        QQC2.TextArea {
            visible: accessMessage.visible
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 30
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.WrapAnywhere
            font.family: "monospace"
            text: Plasmoid.udevRuleCommand
            Accessible.name: i18n("Command that installs the udev rule")
        }
    }
}
