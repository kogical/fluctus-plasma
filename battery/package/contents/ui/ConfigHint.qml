// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Shared with Fluctus Dock (dock/package/contents/ui/ConfigHint.qml); see docs/preferences-guidelines.md.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Short explanatory text placed under a setting.
QQC2.Label {
    Layout.fillWidth: true
    Layout.maximumWidth: Kirigami.Units.gridUnit * 22
    Layout.bottomMargin: Kirigami.Units.smallSpacing
    wrapMode: Text.WordWrap
    font: Kirigami.Theme.smallFont
    color: Kirigami.Theme.disabledTextColor
    textFormat: Text.PlainText
}
