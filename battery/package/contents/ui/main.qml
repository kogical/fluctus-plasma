// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    readonly property bool isHorizontalPanel: Plasmoid.formFactor === PlasmaCore.Types.Horizontal
    readonly property bool isVerticalPanel: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property bool inPanel: isHorizontalPanel || isVerticalPanel
    readonly property var devicesModel: Plasmoid.devicesModel

    preferredRepresentation: inPanel ? compactRepresentation : fullRepresentation

    // On the desktop, hide Plasma's default opaque card frame when Liquid Glass is active
    // so Fluctus Glass / LiquidGlass owns the widget surface cleanly.
    Plasmoid.backgroundHints: (!inPanel && Plasmoid.configuration.useLiquidGlass)
        ? PlasmaCore.Types.NoBackground
        : PlasmaCore.Types.DefaultBackground

    Plasmoid.icon: devicesModel && devicesModel.hasAnyBattery
        ? devicesModel.primaryBatteryIcon
        : "battery-missing"

    toolTipMainText: {
        if (!devicesModel || !devicesModel.hasAnyBattery) {
            return i18n("Fluctus Battery");
        }
        return devicesModel.primaryCharging
            ? i18n("%1 — %2 (Charging)", devicesModel.primaryName, devicesModel.primaryPercentageText)
            : i18n("%1 — %2", devicesModel.primaryName, devicesModel.primaryPercentageText);
    }

    toolTipSubText: {
        if (!devicesModel || !devicesModel.hasAnyBattery) {
            return i18n("No battery devices detected");
        }
        if (devicesModel.batteryCount > 1) {
            return i18np("%1 connected battery device", "%1 connected battery devices", devicesModel.batteryCount);
        }
        return i18n("Click to view battery details");
    }

    function syncBackendConfig() {
        Plasmoid.setFilterConfig(
            Plasmoid.configuration.showStubs,
            Plasmoid.configuration.hideLinePower,
            Plasmoid.configuration.sortOrder,
            Plasmoid.configuration.preferredDeviceId,
            Plasmoid.configuration.lowThreshold,
            Plasmoid.configuration.criticalThreshold
        );
        Plasmoid.setPollIntervalSeconds(Plasmoid.configuration.pollIntervalSeconds);
    }

    Component.onCompleted: syncBackendConfig()

    Connections {
        target: Plasmoid.configuration
        function onShowStubsChanged() { root.syncBackendConfig(); }
        function onHideLinePowerChanged() { root.syncBackendConfig(); }
        function onSortOrderChanged() { root.syncBackendConfig(); }
        function onPreferredDeviceIdChanged() { root.syncBackendConfig(); }
        function onLowThresholdChanged() { root.syncBackendConfig(); }
        function onCriticalThresholdChanged() { root.syncBackendConfig(); }
        function onPollIntervalSecondsChanged() { root.syncBackendConfig(); }
    }

    compactRepresentation: CompactView {
        devicesModel: root.devicesModel
        isVertical: root.isVerticalPanel
        toggleExpanded: function() {
            root.expanded = !root.expanded;
        }
    }

    fullRepresentation: FullView {
        devicesModel: root.devicesModel
        inPanel: root.inPanel
    }
}
