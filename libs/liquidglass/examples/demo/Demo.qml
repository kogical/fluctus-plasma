// SPDX-License-Identifier: Apache-2.0
import QtQuick
import Kogical.LiquidGlass

Rectangle {
    id: page
    color: "#101018"

    // Colourful backdrop to refract.
    Item {
        id: scene
        anchors.fill: parent
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#2b1d5c" }
                GradientStop { position: 0.5; color: "#123a6b" }
                GradientStop { position: 1.0; color: "#5a1f4a" }
            }
        }
        Repeater {
            model: 14
            Rectangle {
                required property int index
                x: (index * 137) % page.width - 40
                y: (index * 89) % page.height - 30
                width: 90 + (index % 4) * 40
                height: width
                radius: width / 2
                color: Qt.hsla((index * 0.13) % 1.0, 0.75, 0.55, 0.9)
            }
        }
        Text {
            x: 40; y: 250
            text: "Liquid Glass ◐ refraction"
            font.pixelSize: 64; font.bold: true
            color: "white"
        }
    }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 40
        spacing: 34
        Repeater {
            model: ["liquid", "liquid", "frosted", "acrylic", "clear"]
            LiquidGlass {
                required property string modelData
                required property int index
                width: 620; height: 74
                radius: modelData === "liquid" ? 28 : 18
                material: modelData
                backdrop: index === 1 ? null : scene
                Text {
                    anchors.centerIn: parent
                    text: parent.modelData + (parent.hasBackdrop ? "  (backdrop)" : "  (overlay)")
                    color: "white"; font.pixelSize: 20
                }
            }
        }
    }
}
