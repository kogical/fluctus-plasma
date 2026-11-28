// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: page

    property alias cfg_showStubs: showStubsCheck.checked
    property alias cfg_hideLinePower: hideLinePowerCheck.checked
    property alias cfg_sortOrder: sortOrderCombo.currentIndex
    property alias cfg_lowThreshold: lowSpin.value
    property alias cfg_criticalThreshold: criticalSpin.value
    property alias cfg_pollIntervalSeconds: pollSpin.value
    property string cfg_preferredDeviceId

    Kirigami.FormLayout {
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Devices")
        }

        QQC2.ComboBox {
            id: sortOrderCombo
            Kirigami.FormData.label: i18n("Sort order:")
            model: [
                i18n("Primary device, then lowest battery"),
                i18n("Lowest battery first"),
                i18n("Alphabetical")
            ]
        }

        QQC2.CheckBox {
            id: hideLinePowerCheck
            Kirigami.FormData.label: i18n("Filtering:")
            text: i18n("Hide AC power adapters")
        }

        QQC2.CheckBox {
            id: showStubsCheck
            text: i18n("Show connected devices without battery telemetry")
        }

        QQC2.Button {
            visible: page.cfg_preferredDeviceId.length > 0
            Kirigami.FormData.label: i18n("Pinned device:")
            text: i18n("Clear pinned panel device")
            icon.name: "edit-clear"
            onClicked: page.cfg_preferredDeviceId = ""
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Thresholds & Polling")
        }

        QQC2.SpinBox {
            id: lowSpin
            Kirigami.FormData.label: i18n("Low battery warning:")
            from: 5
            to: 50
            textFromValue: function(value) { return i18n("%1%", value); }
            valueFromText: function(text) { return parseInt(text, 10) || 20; }
            onValueModified: {
                if (criticalSpin.value > value) {
                    criticalSpin.value = value;
                }
            }
        }

        QQC2.SpinBox {
            id: criticalSpin
            Kirigami.FormData.label: i18n("Critical battery alert:")
            from: 1
            to: lowSpin.value
            textFromValue: function(value) { return i18n("%1%", value); }
            valueFromText: function(text) { return parseInt(text, 10) || 10; }
        }

        QQC2.SpinBox {
            id: pollSpin
            Kirigami.FormData.label: i18n("Refresh interval:")
            from: 10
            to: 600
            stepSize: 5
            textFromValue: function(value) { return i18n("%1 s", value); }
            valueFromText: function(text) { return parseInt(text, 10) || 30; }
        }
    }
}
