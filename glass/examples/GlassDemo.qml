// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Fluctus Glass client example: a transparent window with a Liquid Glass card, a dock-like pill
// and a plain blur request. Needs the Kogical.FluctusGlass QML module on the import path.
//   qmlscene --transparent GlassDemo.qml
import QtQuick
import QtQuick.Window
import Kogical.FluctusGlass

Window {
    id: win
    width: 980
    height: 470
    visible: true
    title: "Glass demo"
    color: "transparent"
    flags: Qt.FramelessWindowHint

    // A card, like a media player widget: thick, clear glass.
    GlassArea {
        id: card
        x: 40
        y: 40
        width: 460
        height: 300
        radius: 68
    }
    Column {
        x: card.x + 44
        y: card.y + 44
        spacing: 10
        Row {
            spacing: 22
            Rectangle {
                width: 104
                height: 104
                radius: 26
                gradient: Gradient {
                    GradientStop { position: 0; color: "#7fb2ff" }
                    GradientStop { position: 1; color: "#1b2e8a" }
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                Text { text: "Old Scratch Blues"; color: "white"; font.pixelSize: 26; font.weight: Font.DemiBold }
                Text { text: "Jack White"; color: Qt.rgba(1, 1, 1, 0.7); font.pixelSize: 20 }
            }
        }
        Item { width: 1; height: 26 }
        Rectangle {
            width: 372
            height: 6
            radius: 3
            color: Qt.rgba(1, 1, 1, 0.3)
            Rectangle { width: 120; height: 6; radius: 3; color: "white" }
        }
        Row {
            spacing: 60
            leftPadding: 60
            topPadding: 14
            Repeater {
                model: ["⏮", "⏸", "⏭"]
                Text { required property string modelData; text: modelData; color: "white"; font.pixelSize: 34 }
            }
        }
    }

    // A dock-like pill.
    GlassArea {
        id: pill
        x: 540
        y: 60
        width: 400
        height: 84
        radius: 26
        intensity: 0.8
    }
    Row {
        anchors.centerIn: pill
        spacing: 14
        Repeater {
            model: 6
            Rectangle {
                required property int index
                width: 50
                height: 50
                radius: 12
                color: Qt.hsla(index / 6, 0.6, 0.6, 0.95)
            }
        }
    }

    // A plain blur request: frosted blur normally, Liquid Glass in Fluctus Glass global mode.
    GlassArea {
        id: plain
        blurOnly: true
        x: 540
        y: 190
        width: 400
        height: 70
        radius: 18
    }
    Text {
        anchors.centerIn: plain
        text: "plain blur request"
        color: "white"
        font.pixelSize: 16
    }

    Text {
        x: 40
        y: card.y + card.height + 20
        text: card.active ? "glass active" : (card.available ? "effect found, attaching…" : "no Fluctus Glass: plain blur")
        color: "white"
        font.pixelSize: 13
    }
}
