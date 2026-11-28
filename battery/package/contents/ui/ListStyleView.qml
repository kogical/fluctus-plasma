// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// The "Fluctus" style: a title and a plain list of devices. Hierarchy comes from type and
// spacing, not from boxes. Small desktop tiles show the headline device on its own instead.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import "Style.js" as Style

Item {
    id: view

    /// Rows with the BatteryDeviceModel roles.
    required property var model
    /// Object with count, batteryCount, hasAnyBattery, refreshing and the primary* summary.
    required property var summary
    property bool interactive: true
    /// Show only the headline device, large (tiny desktop tiles).
    property bool heroMode: false
    property real fontSize: Kirigami.Theme.defaultFont.pixelSize
    /// Show at most this many rows (previews); -1 shows all.
    property int maxRows: -1

    signal refreshRequested()
    signal pinRequested(string deviceId)
    signal settingsRequested()

    readonly property bool dark: Style.isDark(Kirigami.Theme.textColor)
    readonly property real headerHeight: header.implicitHeight + Kirigami.Units.smallSpacing
    readonly property real contentHeight: heroMode ? hero.implicitHeight
        : headerHeight + (summary.count > 0 ? list.contentHeight : emptyState.implicitHeight)

    // ---- Title -----------------------------------------------------------
    RowLayout {
        id: header
        visible: !view.heroMode
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Batteries")
            font.pixelSize: Math.round(view.fontSize * 1.3)
            font.weight: Font.DemiBold
            color: Kirigami.Theme.textColor
            elide: Text.ElideRight
        }

        QQC2.ToolButton {
            visible: view.interactive
            flat: true
            icon.name: "view-refresh-symbolic"
            display: QQC2.AbstractButton.IconOnly
            enabled: !view.summary.refreshing
            opacity: hovered ? 1 : 0.6
            onClicked: view.refreshRequested()
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            QQC2.ToolTip.text: i18n("Check for devices again")
            Accessible.name: QQC2.ToolTip.text
        }
    }

    // ---- Devices ---------------------------------------------------------
    ListView {
        id: list
        visible: !view.heroMode && view.summary.count > 0
        anchors.fill: parent
        anchors.topMargin: view.headerHeight
        clip: contentHeight > height
        interactive: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds
        model: view.model

        delegate: DeviceRow {
            width: ListView.view.width
            visible: view.maxRows < 0 || index < view.maxRows
            height: visible ? implicitHeight : 0
            interactive: view.interactive
            fontSize: view.fontSize
            showSeparator: index < (view.maxRows < 0 ? ListView.view.count : Math.min(view.maxRows, ListView.view.count)) - 1
            onPinRequested: function(id) { view.pinRequested(id); }
            onSettingsRequested: view.settingsRequested()
        }
    }

    // ---- Nothing to show -------------------------------------------------
    ColumnLayout {
        id: emptyState
        visible: !view.heroMode && view.summary.count === 0
        anchors.centerIn: parent
        anchors.verticalCenterOffset: view.headerHeight / 2
        width: parent.width
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: implicitWidth
            source: "battery-missing-symbolic"
            opacity: 0.5
        }
        QQC2.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: view.summary.refreshing ? i18n("Looking for devices…") : i18n("No Batteries")
            font.weight: Font.Medium
        }
        QQC2.Label {
            visible: !view.summary.refreshing
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: i18n("Devices with a battery show up here when they connect.")
            font.pixelSize: Math.round(view.fontSize * 0.85)
            opacity: 0.6
        }
    }

    // ---- Headline device (tiny tiles) -------------------------------------
    ColumnLayout {
        id: hero
        visible: view.heroMode
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Kirigami.Units.smallSpacing

        readonly property color levelColor: Style.levelColor(view.summary.primarySeverity, view.dark)

        Kirigami.Icon {
            implicitWidth: Math.round(view.fontSize * 1.6)
            implicitHeight: implicitWidth
            source: view.summary.hasAnyBattery ? view.summary.primaryCategoryIcon : "battery-missing-symbolic"
            isMask: true
            color: Kirigami.Theme.textColor
        }
        QQC2.Label {
            text: view.summary.hasAnyBattery ? view.summary.primaryPercentageText : "—"
            font.pixelSize: Math.round(view.fontSize * 2.2)
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
        }
        Rectangle {
            visible: view.summary.hasPrimaryPercentage
            Layout.fillWidth: true
            implicitHeight: Math.max(4, Math.round(view.fontSize * 0.36))
            radius: height / 2
            color: Style.alpha(Kirigami.Theme.textColor, view.dark ? 0.14 : 0.1)
            Rectangle {
                height: parent.height
                radius: parent.radius
                width: Math.max(height, parent.width * Math.max(0, Math.min(100, view.summary.primaryPercentage)) / 100)
                color: hero.levelColor
            }
        }
        QQC2.Label {
            Layout.fillWidth: true
            text: view.summary.hasAnyBattery ? view.summary.primaryName : i18n("No Batteries")
            elide: Text.ElideRight
            font.pixelSize: Math.round(view.fontSize * 0.9)
            opacity: 0.7
        }
    }
}
