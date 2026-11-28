// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Unified adaptive FullView for both Desktop Widget (tiny tile / compact card / expanded grid)
// and Panel Popup, backed by Fluctus Liquid Glass.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

Item {
    id: fullView

    required property var devicesModel
    required property bool inPanel

    readonly property var metrics: Plasmoid.layoutMetrics(
        Math.max(1, width),
        Math.max(1, height),
        false,
        false
    )
    readonly property string formMode: inPanel ? "desktopCompact" : metrics.mode
    readonly property bool isTinyDesktop: !inPanel && formMode === "desktopTiny"
    readonly property bool isExpandedGrid: !inPanel && formMode === "desktopExpanded" && devicesModel.count > 1

    Layout.minimumWidth: inPanel ? Kirigami.Units.gridUnit * 16 : Kirigami.Units.gridUnit * 7
    Layout.minimumHeight: inPanel ? Kirigami.Units.gridUnit * 12 : Kirigami.Units.gridUnit * 7
    Layout.preferredWidth: inPanel ? Kirigami.Units.gridUnit * 20 : Kirigami.Units.gridUnit * 18
    Layout.preferredHeight: inPanel
        ? Math.min(Kirigami.Units.gridUnit * 26, Math.max(Kirigami.Units.gridUnit * 12, Kirigami.Units.gridUnit * 5 + devicesModel.count * Kirigami.Units.gridUnit * 4.4))
        : Kirigami.Units.gridUnit * 14

    // Liquid Glass surface (desktop widget and panel popup)
    GlassSurface {
        anchors.fill: parent
        glassEnabled: Plasmoid.configuration.useLiquidGlass
        glassProfile: Plasmoid.configuration.glassProfile
        radius: Math.min(Plasmoid.configuration.cornerRadius, fullView.metrics.cornerRadius)
        windowType: fullView.inPanel ? "popup" : "desktop"
    }

    // ---- Tiny Desktop Tile (hero radial ring) ------------------------------
    ColumnLayout {
        visible: fullView.isTinyDesktop
        anchors.fill: parent
        anchors.margins: fullView.metrics.padding
        spacing: fullView.metrics.spacing

        BatteryRing {
            Layout.alignment: Qt.AlignCenter
            Layout.preferredWidth: fullView.metrics.ringSize
            Layout.preferredHeight: fullView.metrics.ringSize
            percentage: fullView.devicesModel.primaryPercentage
            hasPercentage: fullView.devicesModel.hasPrimaryPercentage
            charging: fullView.devicesModel.primaryCharging
            severity: fullView.devicesModel.primarySeverity
            centerText: fullView.devicesModel.hasAnyBattery ? fullView.devicesModel.primaryPercentageText : ""
            iconName: fullView.devicesModel.hasAnyBattery ? "" : "battery-missing-symbolic"
        }

        Text {
            visible: fullView.metrics.showSecondaryLabel
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: fullView.devicesModel.hasAnyBattery
                ? fullView.devicesModel.primaryName
                : i18n("No batteries")
            color: Kirigami.Theme.textColor
            font.pixelSize: fullView.metrics.subFontSizePx
            font.weight: Font.Medium
            elide: Text.ElideRight
        }
    }

    // ---- Standard / Expanded Widget & Panel Popup --------------------------
    ColumnLayout {
        visible: !fullView.isTinyDesktop
        anchors.fill: parent
        anchors.margins: fullView.metrics.padding
        spacing: fullView.metrics.spacing

        // Header bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Kirigami.Icon {
                source: fullView.devicesModel.hasAnyBattery
                    ? fullView.devicesModel.primaryBatteryIcon
                    : "battery-missing-symbolic"
                Layout.preferredWidth: fullView.metrics.iconSize
                Layout.preferredHeight: fullView.metrics.iconSize
                isMask: true
                color: fullView.devicesModel.primaryCharging
                    ? Kirigami.Theme.positiveTextColor
                    : Kirigami.Theme.textColor
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Text {
                    Layout.fillWidth: true
                    text: i18n("Batteries")
                    color: Kirigami.Theme.textColor
                    font.pixelSize: fullView.metrics.fontSizePx + 1
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: {
                        if (!fullView.devicesModel.hasAnyBattery) {
                            return i18n("No battery devices detected");
                        }
                        if (fullView.devicesModel.chargingCount > 0) {
                            return i18np("%1 device · %2 charging", "%1 devices · %2 charging",
                                         fullView.devicesModel.batteryCount,
                                         fullView.devicesModel.chargingCount);
                        }
                        return i18np("%1 device", "%1 devices", fullView.devicesModel.batteryCount);
                    }
                    color: Kirigami.Theme.disabledTextColor
                    font.pixelSize: fullView.metrics.subFontSizePx
                    elide: Text.ElideRight
                }
            }

            QQC2.ToolButton {
                icon.name: "view-refresh-symbolic"
                display: QQC2.AbstractButton.IconOnly
                enabled: !fullView.devicesModel.refreshing
                onClicked: Plasmoid.refresh()
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: i18n("Refresh devices")
            }
        }

        // Empty state
        Item {
            visible: fullView.devicesModel.count === 0
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 6

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignHCenter
                    source: "battery-missing-symbolic"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.large
                    Layout.preferredHeight: Kirigami.Units.iconSizes.large
                    opacity: 0.55
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: fullView.devicesModel.refreshing
                        ? i18n("Scanning devices…")
                        : i18n("No batteries found")
                    color: Kirigami.Theme.disabledTextColor
                    font.pixelSize: fullView.metrics.fontSizePx
                }
            }
        }

        // Compact list view (popup & narrow/medium desktop widget)
        ListView {
            visible: fullView.devicesModel.count > 0 && !fullView.isExpandedGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Math.max(6, Math.round(fullView.metrics.spacing * 0.8))
            model: fullView.devicesModel

            delegate: DeviceCard {
                width: ListView.view.width
                tileMode: false
                fontSizePx: fullView.metrics.fontSizePx
                subFontSizePx: fullView.metrics.subFontSizePx
                iconSize: fullView.metrics.iconSize
                onPinRequested: function(id) {
                    Plasmoid.configuration.preferredDeviceId = id;
                }
            }
        }

        // Expanded multi-column grid (wide desktop widget)
        GridView {
            visible: fullView.devicesModel.count > 0 && fullView.isExpandedGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: fullView.devicesModel

            readonly property int cols: Math.max(2, fullView.metrics.columns)
            readonly property real gap: fullView.metrics.spacing
            cellWidth: Math.floor((width + gap) / cols)
            cellHeight: Math.max(106, Math.round(fullView.metrics.ringSize + 48))

            delegate: Item {
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

                width: GridView.view.cellWidth
                height: GridView.view.cellHeight

                DeviceCard {
                    anchors.fill: parent
                    anchors.rightMargin: GridView.view.gap
                    anchors.bottomMargin: GridView.view.gap
                    tileMode: true
                    deviceId: parent.deviceId
                    name: parent.name
                    categoryIcon: parent.categoryIcon
                    transportLabel: parent.transportLabel
                    powerAvailable: parent.powerAvailable
                    percentage: parent.percentage
                    hasPercentage: parent.hasPercentage
                    percentageText: parent.percentageText
                    isCharging: parent.isCharging
                    chargingLabel: parent.chargingLabel
                    severity: parent.severity
                    unavailableLabel: parent.unavailableLabel
                    cells: parent.cells
                    cellCount: parent.cellCount
                    isPrimary: parent.isPrimary
                    fontSizePx: fullView.metrics.fontSizePx
                    subFontSizePx: fullView.metrics.subFontSizePx
                    iconSize: fullView.metrics.iconSize
                    ringSize: fullView.metrics.ringSize
                    onPinRequested: function(id) {
                        Plasmoid.configuration.preferredDeviceId = id;
                    }
                }
            }
        }
    }
}
