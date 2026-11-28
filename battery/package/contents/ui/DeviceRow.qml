// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// One device in the list style. No card around it: a round glyph platter, the name with its
// level, a slim capsule bar and an optional caption, separated from the next row by a hairline
// inset to the text (as in Apple's lists). Hover gives a soft highlight; right-click offers
// "Show in Panel".
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import "Style.js" as Style

Item {
    id: row

    required property int index
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
    required property bool needsPermission

    /// False in previews: no hover, menus or clicks.
    property bool interactive: true
    property bool showSeparator: true
    property real fontSize: Kirigami.Theme.defaultFont.pixelSize
    readonly property bool dark: Style.isDark(Kirigami.Theme.textColor)
    readonly property color levelColor: Style.levelColor(severity, dark)
    readonly property real platterSize: Math.round(fontSize * 2.4)

    signal pinRequested(string deviceId)
    signal settingsRequested()

    readonly property string caption: {
        if (!powerAvailable) {
            return unavailableLabel;
        }
        if (cellCount > 1) {
            const parts = [];
            // A QVariantList from the backend, or a nested ListModel in previews.
            const n = cells.count !== undefined ? cells.count : cells.length;
            for (let i = 0; i < n; ++i) {
                const cell = cells.count !== undefined ? cells.get(i) : cells[i];
                parts.push(cell.roleLabel + " " + cell.percentageText);
            }
            return parts.join("  ·  ");
        }
        if (isCharging || severity === "full") {
            return chargingLabel;
        }
        return "";
    }

    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 1.5

    Accessible.role: Accessible.ListItem
    Accessible.name: powerAvailable ? i18n("%1, %2", name, percentageText) : i18n("%1, %2", name, unavailableLabel)

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: -Kirigami.Units.smallSpacing
        anchors.rightMargin: -Kirigami.Units.smallSpacing
        radius: Kirigami.Units.cornerRadius * 2
        color: Style.alpha(Kirigami.Theme.textColor, hover.hovered && row.interactive ? 0.07 : 0)
        Behavior on color {
            ColorAnimation { duration: Kirigami.Units.shortDuration }
        }
    }

    HoverHandler {
        id: hover
        enabled: row.interactive
    }

    TapHandler {
        enabled: row.interactive
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onTapped: function(eventPoint, button) {
            if (button === Qt.RightButton && row.powerAvailable) {
                menu.popup();
            } else if (row.needsPermission) {
                row.settingsRequested();
            }
        }
        onLongPressed: if (row.powerAvailable) menu.popup()
    }

    QQC2.Menu {
        id: menu
        QQC2.MenuItem {
            text: row.isPrimary ? i18n("Shown in Panel") : i18n("Show in Panel")
            icon.name: "pin"
            enabled: !row.isPrimary
            onTriggered: row.pinRequested(row.deviceId)
        }
    }

    QQC2.ToolTip.visible: row.interactive && hover.hovered && row.transportLabel.length > 0
    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    QQC2.ToolTip.text: row.transportLabel

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Kirigami.Units.largeSpacing

        // Round platter with the device glyph.
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: row.platterSize
            implicitHeight: row.platterSize
            radius: width / 2
            color: Style.alpha(Kirigami.Theme.textColor, row.dark ? 0.13 : 0.08)

            Kirigami.Icon {
                anchors.centerIn: parent
                width: Math.round(row.platterSize * 0.56)
                height: width
                source: row.categoryIcon
                isMask: true
                color: Kirigami.Theme.textColor
                opacity: row.powerAvailable ? 1 : 0.5
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Math.round(row.fontSize * 0.35)

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    text: row.name
                    font.pixelSize: row.fontSize
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    color: Kirigami.Theme.textColor
                }

                QQC2.Label {
                    visible: row.powerAvailable
                    text: row.percentageText
                    font.pixelSize: row.fontSize
                    font.features: { "tnum": 1 }
                    color: row.severity === "critical" ? row.levelColor : Kirigami.Theme.textColor
                    opacity: row.severity === "critical" ? 1 : 0.7
                }
            }

            // Slim capsule bar.
            Rectangle {
                visible: row.powerAvailable && row.hasPercentage
                Layout.fillWidth: true
                implicitHeight: Math.max(4, Math.round(row.fontSize * 0.36))
                radius: height / 2
                color: Style.alpha(Kirigami.Theme.textColor, row.dark ? 0.14 : 0.1)

                Rectangle {
                    height: parent.height
                    radius: parent.radius
                    width: Math.max(height, parent.width * Math.max(0, Math.min(100, row.percentage)) / 100)
                    color: row.levelColor
                    Behavior on width {
                        NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
                    }
                }
            }

            QQC2.Label {
                visible: row.caption.length > 0
                Layout.fillWidth: true
                text: row.caption
                font.pixelSize: Math.round(row.fontSize * 0.85)
                font.features: { "tnum": 1 }
                elide: Text.ElideRight
                color: row.needsPermission ? Kirigami.Theme.neutralTextColor
                    : (row.isCharging ? row.levelColor : Kirigami.Theme.textColor)
                opacity: row.needsPermission || row.isCharging ? 1 : 0.6
            }
        }

        Kirigami.Icon {
            visible: row.needsPermission
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: Kirigami.Units.iconSizes.small
            implicitHeight: implicitWidth
            source: "go-next-symbolic"
            opacity: 0.5
        }
    }

    // Hairline separator, inset to where the text starts.
    Rectangle {
        visible: row.showSeparator
        anchors.bottom: parent.bottom
        x: row.platterSize + Kirigami.Units.largeSpacing
        width: parent.width - x
        height: 1
        color: Style.alpha(Kirigami.Theme.textColor, row.dark ? 0.1 : 0.08)
    }
}
