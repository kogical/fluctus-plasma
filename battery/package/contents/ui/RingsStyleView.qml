// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// The "macOS" style: a grid of charge rings, like the Batteries widget on macOS. Free slots in
// the grid show an empty ring so the layout keeps its rhythm. Levels sit under the rings when
// there is room; otherwise the ring says it all.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import "Style.js" as Style

Item {
    id: view

    required property var model
    required property var summary
    property bool interactive: true
    /// Rings per row; the grid fills at least one full row of slots.
    property int columns: 4
    /// Minimum number of slots, so a lone device still sits in a 2×2 or 1×4 grid.
    property int minimumSlots: columns
    property bool showLabels: true
    /// Shrink rings to fit the height too (desktop). Popups size from width and grow to fit.
    property bool fitHeight: true
    property real fontSize: Kirigami.Theme.defaultFont.pixelSize

    signal settingsRequested()

    readonly property int deviceCount: view.model ? view.model.count : 0
    readonly property int slotCount: Math.max(minimumSlots, Math.ceil(deviceCount / columns) * columns)
    readonly property int rows: Math.max(1, Math.ceil(slotCount / columns))
    readonly property real gap: Math.round(Math.max(Kirigami.Units.largeSpacing, width * 0.06))
    readonly property real labelHeight: showLabels ? Math.round(fontSize * 1.9) : 0
    readonly property real ringSize: Math.max(24, Math.floor(Math.min(
        (width - gap * (columns - 1)) / columns,
        fitHeight && height > 0 ? (height - gap * (rows - 1)) / rows - labelHeight : Number.MAX_VALUE)))
    readonly property real contentHeight: rows * (ringSize + labelHeight) + (rows - 1) * gap

    Grid {
        anchors.centerIn: parent
        columns: view.columns
        columnSpacing: view.gap
        rowSpacing: view.gap

        // Devices
        Repeater {
            model: view.model

            delegate: Column {
                id: cell
                required property string name
                required property string categoryIcon
                required property bool powerAvailable
                required property real percentage
                required property bool hasPercentage
                required property string percentageText
                required property bool isCharging
                required property string severity
                required property string unavailableLabel
                required property bool needsPermission

                spacing: 0
                width: view.ringSize

                BatteryRing {
                    id: ring
                    width: view.ringSize
                    height: view.ringSize
                    percentage: cell.percentage
                    hasPercentage: cell.hasPercentage
                    charging: cell.isCharging
                    severity: cell.severity
                    iconName: cell.categoryIcon
                    dimmed: !cell.powerAvailable
                    attention: cell.needsPermission

                    HoverHandler {
                        id: hover
                        enabled: view.interactive
                    }
                    TapHandler {
                        enabled: view.interactive && cell.needsPermission
                        onTapped: view.settingsRequested()
                    }
                    QQC2.ToolTip.visible: hover.hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    QQC2.ToolTip.text: cell.powerAvailable
                        ? i18n("%1: %2", cell.name, cell.percentageText)
                        : i18n("%1: %2", cell.name, cell.unavailableLabel)
                    Accessible.role: Accessible.StaticText
                    Accessible.name: QQC2.ToolTip.text
                }

                Item {
                    visible: view.showLabels
                    width: view.ringSize
                    height: view.labelHeight

                    QQC2.Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        horizontalAlignment: Text.AlignHCenter
                        text: cell.powerAvailable ? cell.percentageText
                            : (cell.needsPermission ? i18n("Needs access") : "—")
                        font.weight: Font.Medium
                        font.features: { "tnum": 1 }
                        font.pixelSize: cell.needsPermission ? Math.round(view.fontSize * 0.8) : Math.round(view.fontSize * 1.05)
                        color: cell.needsPermission ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.textColor
                        opacity: cell.powerAvailable || cell.needsPermission ? 0.9 : 0.45
                    }
                }
            }
        }

        // Free slots
        Repeater {
            model: Math.max(0, view.slotCount - view.deviceCount)

            delegate: Column {
                width: view.ringSize
                BatteryRing {
                    width: view.ringSize
                    height: view.ringSize
                    hasPercentage: false
                }
                Item {
                    visible: view.showLabels
                    width: 1
                    height: view.labelHeight
                }
            }
        }
    }
}
