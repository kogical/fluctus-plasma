// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A Plasma dialog with the theme's translucent, blurred background: an ordinary blur request,
// used to test Fluctus Glass global mode (blur requests become Liquid Glass).
import QtQuick
import QtQuick.Window
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents

PlasmaCore.Dialog {
    visible: true
    location: PlasmaCore.Types.Floating
    x: 120
    y: 120
    mainItem: Column {
        width: 320
        spacing: 8
        PlasmaComponents.Label {
            text: "Plasma popup (blur request)"
            font.bold: true
        }
        Repeater {
            model: ["Wi-Fi", "Bluetooth", "Night Light", "Do Not Disturb"]
            PlasmaComponents.Label {
                required property string modelData
                text: "• " + modelData
            }
        }
    }
}
