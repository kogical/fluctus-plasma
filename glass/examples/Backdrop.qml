// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Busy, animated backdrop for testing Fluctus Glass: an image plus moving colour blocks and text,
// so refraction, dispersion and repaint correctness are easy to judge. Set `imagePath` from a
// wrapper (glass/tools/run-nested.sh generates one) or edit it here.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window

Window {
    id: win
    width: 1280
    height: 800
    visible: true
    title: "Glass backdrop"
    color: "#202028"
    flags: frameless ? Qt.FramelessWindowHint : Qt.Window

    property string imagePath: ""
    property bool animate: true
    property bool showBlocks: true
    property bool showText: true
    property bool frameless: false

    Image {
        anchors.fill: parent
        source: win.imagePath ? "file://" + win.imagePath : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }

    Repeater {
        model: win.showBlocks ? 14 : 0
        Rectangle {
            required property int index
            width: 90
            height: 90
            radius: index % 3 === 0 ? 45 : 8
            color: Qt.hsla(index / 14, 0.85, 0.55, 1)
            y: win.height / 2 - 70 + (index % 2) * 60
            x: (index * 110 + shift) % (win.width + 120) - 60
            property real shift: 0
            NumberAnimation on shift {
                running: win.animate
                from: 0
                to: win.width + 120
                duration: 9000
                loops: Animation.Infinite
            }
        }
    }

    Text {
        visible: win.showText
        anchors.horizontalCenter: parent.horizontalCenter
        y: win.height - 190
        text: "Liquid Glass refracts whatever is behind it — 0123456789"
        font.pixelSize: 34
        font.bold: true
        color: "white"
        style: Text.Outline
        styleColor: "black"
    }
}
