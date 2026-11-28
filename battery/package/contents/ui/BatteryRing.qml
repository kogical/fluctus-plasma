// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Resolution-independent GPU-rendered radial battery ring using QtQuick.Shapes.
import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

Item {
    id: ring

    property real percentage: 0
    property bool hasPercentage: true
    property bool charging: false
    property string severity: "normal"
    property string iconName: ""
    property string centerText: ""
    property real strokeWidth: Math.max(3, Math.round(Math.min(width, height) * 0.11))

    property real clampedPct: hasPercentage ? Math.max(0, Math.min(100, percentage)) : 0
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

    Behavior on clampedPct {
        NumberAnimation {
            duration: Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }
    }

    Shape {
        id: ringShape
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        layer.enabled: true
        layer.samples: 4

        readonly property real cx: width * 0.5
        readonly property real cy: height * 0.5
        readonly property real arcRadius: Math.max(4, Math.min(width, height) * 0.5 - ring.strokeWidth * 0.65)

        // Subtle track ring
        ShapePath {
            fillColor: "transparent"
            strokeColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.16)
            strokeWidth: ring.strokeWidth
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: ringShape.cx
                centerY: ringShape.cy
                radiusX: ringShape.arcRadius
                radiusY: ringShape.arcRadius
                startAngle: -90
                sweepAngle: 360
            }
        }

        // Active charge arc
        ShapePath {
            fillColor: "transparent"
            strokeColor: ring.hasPercentage ? ring.accentColor : "transparent"
            strokeWidth: ring.strokeWidth
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: ringShape.cx
                centerY: ringShape.cy
                radiusX: ringShape.arcRadius
                radiusY: ringShape.arcRadius
                startAngle: -90
                sweepAngle: Math.max(ring.clampedPct > 0 ? 4 : 0, (ring.clampedPct / 100.0) * 360.0)
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: Math.max(1, Math.round(ring.height * 0.03))

        Kirigami.Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: ring.iconName.length > 0 && ring.centerText.length === 0
            source: ring.iconName
            width: Math.max(14, Math.round(ring.height * 0.36))
            height: width
            isMask: true
            color: ring.charging ? ring.accentColor : Kirigami.Theme.textColor
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: ring.centerText.length > 0
            text: ring.centerText
            color: Kirigami.Theme.textColor
            font.pixelSize: Math.max(10, Math.round(ring.height * 0.27))
            font.weight: Font.DemiBold
            font.features: ({ "tnum": 1 })
        }

        Kirigami.Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: ring.charging && ring.centerText.length > 0 && ring.height >= 54
            source: "battery-charging-symbolic"
            width: Math.max(10, Math.round(ring.height * 0.20))
            height: width
            isMask: true
            color: ring.accentColor
        }
    }
}
