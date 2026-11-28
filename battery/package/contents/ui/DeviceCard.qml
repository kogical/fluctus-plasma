// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Translucent device battery card supporting both compact row layout (popup / narrow widget)
// and radial tile layout (wide desktop dashboard), including multi-cell telemetry pills.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: card

    required property string deviceId
    required property string name
    required property string categoryIcon
    required property string transportLabel
    required property bool powerAvailable
    required property real percentage
    required property bool hasPercentage
    required property string percentageText
    required property bool isCharging
    required property string chargingLabel
    required property string severity
    required property string unavailableLabel
    required property var cells
    required property int cellCount
    required property bool isPrimary

    property bool tileMode: false
    property real fontSizePx: 13
    property real subFontSizePx: 11
    property real iconSize: 20
    property real ringSize: 54

    signal pinRequested(string deviceId)

    readonly property color accentColor: {
        switch (severity) {
        case "critical":
            return Kirigami.Theme.negativeTextColor;
        case "warning":
            return Kirigami.Theme.neutralTextColor;
        case "charging":
        case "full":
            return Kirigami.Theme.positiveTextColor;
        default:
            return Kirigami.Theme.highlightColor;
        }
    }

    readonly property string subtitleText: {
        const parts = [];
        if (powerAvailable) {
            if (chargingLabel.length > 0) {
                parts.push(chargingLabel);
            }
        } else if (unavailableLabel.length > 0) {
            parts.push(unavailableLabel);
        }
        if (transportLabel.length > 0) {
            parts.push(transportLabel);
        }
        return parts.join(" · ");
    }

    radius: tileMode ? 14 : 11
    color: Qt.rgba(
        Kirigami.Theme.textColor.r,
        Kirigami.Theme.textColor.g,
        Kirigami.Theme.textColor.b,
        hoverHandler.hovered ? 0.11 : 0.06
    )
    border.width: 1
    border.color: severity === "critical"
        ? Qt.rgba(Kirigami.Theme.negativeTextColor.r, Kirigami.Theme.negativeTextColor.g, Kirigami.Theme.negativeTextColor.b, 0.45)
        : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)

    implicitHeight: tileMode ? tileLayout.implicitHeight + 22 : rowLayout.implicitHeight + 18

    HoverHandler {
        id: hoverHandler
    }

    // ---- Row layout (popup & compact desktop card) -------------------------
    ColumnLayout {
        id: rowLayout
        visible: !card.tileMode
        anchors.fill: parent
        anchors.margins: 9
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                Layout.preferredWidth: Math.round(card.iconSize * 1.55)
                Layout.preferredHeight: Layout.preferredWidth
                radius: Math.round(Layout.preferredWidth * 0.32)
                color: Qt.rgba(card.accentColor.r, card.accentColor.g, card.accentColor.b, card.powerAvailable ? 0.16 : 0.08)

                Kirigami.Icon {
                    anchors.centerIn: parent
                    source: card.categoryIcon
                    width: card.iconSize
                    height: card.iconSize
                    isMask: true
                    color: card.powerAvailable ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        Layout.fillWidth: true
                        text: card.name
                        color: Kirigami.Theme.textColor
                        font.pixelSize: card.fontSizePx
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    QQC2.ToolButton {
                        visible: hoverHandler.hovered && card.powerAvailable && !card.isPrimary
                        icon.name: "pin"
                        display: QQC2.AbstractButton.IconOnly
                        implicitWidth: 22
                        implicitHeight: 22
                        onClicked: card.pinRequested(card.deviceId)
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: i18n("Show in panel")
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: card.subtitleText.length > 0
                    text: card.subtitleText
                    color: Kirigami.Theme.disabledTextColor
                    font.pixelSize: card.subFontSizePx
                    elide: Text.ElideRight
                }
            }

            RowLayout {
                spacing: 3
                visible: card.powerAvailable

                Kirigami.Icon {
                    visible: card.isCharging
                    source: "battery-charging-symbolic"
                    Layout.preferredWidth: Math.max(12, Math.round(card.fontSizePx * 0.95))
                    Layout.preferredHeight: Layout.preferredWidth
                    isMask: true
                    color: Kirigami.Theme.positiveTextColor
                }

                Text {
                    text: card.percentageText
                    color: card.severity === "critical"
                        ? Kirigami.Theme.negativeTextColor
                        : Kirigami.Theme.textColor
                    font.pixelSize: card.fontSizePx + 1
                    font.weight: Font.DemiBold
                    font.features: ({ "tnum": 1 })
                }
            }
        }

        // Slim animated charge progress bar
        Rectangle {
            visible: card.powerAvailable && card.hasPercentage
            Layout.fillWidth: true
            Layout.preferredHeight: 5
            radius: 2.5
            color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.14)

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: Math.max(4, Math.round(parent.width * Math.max(0, Math.min(100, card.percentage)) / 100.0))
                radius: parent.radius
                color: card.accentColor

                Behavior on width {
                    NumberAnimation {
                        duration: Kirigami.Units.longDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }

        // Multi-cell breakdown pills (earbuds L/R/Case, split keyboards, etc.)
        Flow {
            visible: card.powerAvailable && card.cellCount > 1
            Layout.fillWidth: true
            spacing: 5

            Repeater {
                model: card.cells
                delegate: Rectangle {
                    required property var modelData
                    radius: 6
                    color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.09)
                    implicitWidth: cellPillRow.implicitWidth + 10
                    implicitHeight: cellPillRow.implicitHeight + 4

                    RowLayout {
                        id: cellPillRow
                        anchors.centerIn: parent
                        spacing: 3

                        Text {
                            text: modelData.roleLabel
                            color: Kirigami.Theme.disabledTextColor
                            font.pixelSize: Math.max(9.5, card.subFontSizePx - 0.5)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: modelData.percentageText
                            color: Kirigami.Theme.textColor
                            font.pixelSize: Math.max(9.5, card.subFontSizePx - 0.5)
                            font.weight: Font.Medium
                            font.features: ({ "tnum": 1 })
                        }
                    }
                }
            }
        }
    }

    // ---- Tile layout (wide desktop dashboard grid) -------------------------
    ColumnLayout {
        id: tileLayout
        visible: card.tileMode
        anchors.fill: parent
        anchors.margins: 11
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            BatteryRing {
                Layout.preferredWidth: card.ringSize
                Layout.preferredHeight: card.ringSize
                percentage: card.percentage
                hasPercentage: card.hasPercentage
                charging: card.isCharging
                severity: card.severity
                centerText: card.powerAvailable ? card.percentageText : ""
                iconName: card.powerAvailable ? "" : card.categoryIcon
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Kirigami.Icon {
                    source: card.categoryIcon
                    Layout.preferredWidth: Math.max(16, Math.round(card.iconSize * 0.9))
                    Layout.preferredHeight: Layout.preferredWidth
                    isMask: true
                    color: Kirigami.Theme.textColor
                }

                Text {
                    Layout.fillWidth: true
                    text: card.name
                    color: Kirigami.Theme.textColor
                    font.pixelSize: card.fontSizePx
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }

                Text {
                    Layout.fillWidth: true
                    visible: card.subtitleText.length > 0
                    text: card.subtitleText
                    color: Kirigami.Theme.disabledTextColor
                    font.pixelSize: card.subFontSizePx
                    elide: Text.ElideRight
                }
            }
        }

        Flow {
            visible: card.powerAvailable && card.cellCount > 1
            Layout.fillWidth: true
            spacing: 5

            Repeater {
                model: card.cells
                delegate: Rectangle {
                    required property var modelData
                    radius: 6
                    color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.10)
                    implicitWidth: tileCellRow.implicitWidth + 10
                    implicitHeight: tileCellRow.implicitHeight + 4

                    RowLayout {
                        id: tileCellRow
                        anchors.centerIn: parent
                        spacing: 3

                        Text {
                            text: modelData.roleLabel
                            color: Kirigami.Theme.disabledTextColor
                            font.pixelSize: Math.max(9.5, card.subFontSizePx - 0.5)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: modelData.percentageText
                            color: Kirigami.Theme.textColor
                            font.pixelSize: Math.max(9.5, card.subFontSizePx - 0.5)
                            font.weight: Font.Medium
                            font.features: ({ "tnum": 1 })
                        }
                    }
                }
            }
        }
    }
}
