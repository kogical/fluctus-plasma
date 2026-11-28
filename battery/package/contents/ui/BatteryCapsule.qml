// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Crisp vector horizontal battery capsule that scales cleanly down to tiny 20px panels
// and up to 4K HiDPI displays without blurry raster scaling.
import QtQuick
import org.kde.kirigami as Kirigami

Item {
    id: capsule

    property real percentage: 0
    property bool hasPercentage: true
    property bool charging: false
    property string severity: "normal"

    readonly property real clampedPct: hasPercentage ? Math.max(0, Math.min(100, percentage)) : 0
    readonly property color fillColor: {
        switch (severity) {
        case "critical":
            return Kirigami.Theme.negativeTextColor;
        case "warning":
            return Kirigami.Theme.neutralTextColor;
        case "charging":
        case "full":
            return Kirigami.Theme.positiveTextColor;
        default:
            return Kirigami.Theme.textColor;
        }
    }

    readonly property real bodyHeight: Math.max(10, Math.round(height * 0.72))
    readonly property real nubWidth: Math.max(2, Math.round(bodyHeight * 0.14))
    readonly property real nubGap: Math.max(1, Math.round(bodyHeight * 0.08))
    readonly property real bodyWidth: Math.max(14, width - nubWidth - nubGap)
    readonly property real stroke: Math.max(1, Math.round(bodyHeight * 0.11))

    implicitWidth: Math.round(height * 1.65)
    implicitHeight: 16

    Rectangle {
        id: body
        x: 0
        anchors.verticalCenter: parent.verticalCenter
        width: capsule.bodyWidth
        height: capsule.bodyHeight
        radius: Math.max(2, Math.round(height * 0.28))
        color: "transparent"
        border.width: capsule.stroke
        border.color: Qt.rgba(
            Kirigami.Theme.textColor.r,
            Kirigami.Theme.textColor.g,
            Kirigami.Theme.textColor.b,
            capsule.hasPercentage ? 0.85 : 0.40
        )

        Rectangle {
            id: fillBar
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: capsule.stroke + Math.max(1, Math.round(body.height * 0.08))
            width: Math.max(
                capsule.clampedPct > 0 ? 2 : 0,
                Math.round((parent.width - anchors.margins * 2) * (capsule.clampedPct / 100.0))
            )
            radius: Math.max(1, parent.radius - capsule.stroke)
            color: capsule.fillColor
            visible: capsule.hasPercentage

            Behavior on width {
                NumberAnimation {
                    duration: Kirigami.Units.shortDuration
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    // Positive terminal nub
    Rectangle {
        x: body.width + capsule.nubGap
        anchors.verticalCenter: parent.verticalCenter
        width: capsule.nubWidth
        height: Math.max(4, Math.round(body.height * 0.42))
        radius: Math.max(1, width * 0.5)
        color: Qt.rgba(
            Kirigami.Theme.textColor.r,
            Kirigami.Theme.textColor.g,
            Kirigami.Theme.textColor.b,
            capsule.hasPercentage ? 0.72 : 0.35
        )
    }
}
