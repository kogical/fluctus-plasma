// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// A charge ring in the style of the macOS Batteries widget: a heavy round-capped arc around the
// device glyph. While charging, a lightning bolt sits in a gap at the top of the ring.
// An empty slot (no device) is just the track.
import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

import "Style.js" as Style

Item {
    id: ring

    property real percentage: 0
    property bool hasPercentage: true
    property bool charging: false
    property string severity: "normal"
    /// Device glyph in the middle; empty for an empty slot.
    property string iconName: ""
    /// The device is known but its level can't be read: glyph dimmed, no arc.
    property bool dimmed: false
    /// Something needs the user (e.g. a udev rule): a yellow "!" badge on the ring.
    property bool attention: false
    property bool dark: Style.isDark(Kirigami.Theme.textColor)
    property real strokeRatio: 0.095

    readonly property real strokeWidth: Math.max(2.5, Math.round(Math.min(width, height) * strokeRatio))
    readonly property color levelColor: Style.levelColor(severity, dark)
    readonly property color trackColor: Style.alpha(Kirigami.Theme.textColor, dark ? 0.16 : 0.12)
    readonly property real clampedPct: hasPercentage ? Math.max(0, Math.min(100, percentage)) : 0
    // Gap at 12 o'clock for the bolt, in degrees on each side.
    readonly property real gapHalf: charging ? 14 : 0

    property real shownPct: clampedPct
    Behavior on shownPct {
        NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
    }

    implicitWidth: Kirigami.Units.gridUnit * 3
    implicitHeight: implicitWidth

    Shape {
        id: shape
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        readonly property real cx: width / 2
        readonly property real cy: height / 2
        readonly property real r: Math.max(2, Math.min(width, height) / 2 - ring.strokeWidth / 2)
        readonly property real start: -90 + ring.gapHalf
        readonly property real span: 360 - 2 * ring.gapHalf

        ShapePath {
            fillColor: "transparent"
            strokeColor: ring.trackColor
            strokeWidth: ring.strokeWidth
            capStyle: ring.charging ? ShapePath.RoundCap : ShapePath.FlatCap
            PathAngleArc {
                centerX: shape.cx
                centerY: shape.cy
                radiusX: shape.r
                radiusY: shape.r
                startAngle: shape.start
                sweepAngle: shape.span
            }
        }

        ShapePath {
            fillColor: "transparent"
            strokeColor: ring.hasPercentage && !ring.dimmed && ring.shownPct > 0 ? ring.levelColor : "transparent"
            strokeWidth: ring.strokeWidth
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: shape.cx
                centerY: shape.cy
                radiusX: shape.r
                radiusY: shape.r
                startAngle: shape.start
                sweepAngle: Math.max(0.5, shape.span * ring.shownPct / 100)
            }
        }
    }

    // Lightning bolt in the gap at the top of the ring.
    Shape {
        visible: ring.charging
        width: ring.strokeWidth * 1.15
        height: ring.strokeWidth * 1.7
        x: (ring.width - width) / 2
        y: ring.strokeWidth / 2 - height / 2
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: "transparent"
            fillColor: ring.levelColor
            scale: Qt.size(ring.strokeWidth * 1.15, ring.strokeWidth * 1.7)
            startX: 0.62; startY: 0.0
            PathLine { x: 0.05; y: 0.58 }
            PathLine { x: 0.46; y: 0.58 }
            PathLine { x: 0.36; y: 1.0 }
            PathLine { x: 0.95; y: 0.40 }
            PathLine { x: 0.54; y: 0.40 }
            PathLine { x: 0.62; y: 0.0 }
        }
    }

    Kirigami.Icon {
        visible: ring.iconName.length > 0
        anchors.centerIn: parent
        width: Math.round(Math.min(ring.width, ring.height) * 0.40)
        height: width
        source: ring.iconName
        isMask: true
        color: Kirigami.Theme.textColor
        opacity: ring.dimmed ? 0.45 : 0.95
    }

    // Attention badge, top-right, like a notification badge.
    Rectangle {
        visible: ring.attention
        readonly property real size: Math.max(12, Math.round(Math.min(ring.width, ring.height) * 0.3))
        width: size
        height: size
        radius: size / 2
        x: ring.width - size * 0.85
        y: -size * 0.15
        color: ring.dark ? "#ffd60a" : "#f5b800"
        border.width: Math.max(1, Math.round(size * 0.08))
        border.color: Style.alpha(Kirigami.Theme.backgroundColor, 0.9)

        Text {
            anchors.centerIn: parent
            text: "!"
            color: "#1d1d1f"
            font.pixelSize: Math.round(parent.size * 0.72)
            font.weight: Font.Black
        }
    }
}
