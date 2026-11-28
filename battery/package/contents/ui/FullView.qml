// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// The widget body, on the desktop and in the panel popup: a surface (Liquid Glass, Frosted or
// the Plasma theme) and one of the two styles (list or rings).
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

Item {
    id: fullView

    required property var devicesModel
    required property bool inPanel

    readonly property int widgetStyle: Plasmoid.configuration.widgetStyle
    readonly property bool rings: widgetStyle === 1
    readonly property real padding: inPanel ? Kirigami.Units.largeSpacing * 1.5
        : Math.round(Math.max(Kirigami.Units.largeSpacing * 1.5, Math.min(width, height) * 0.08))
    readonly property bool tiny: !inPanel && (width < Kirigami.Units.gridUnit * 11 || height < Kirigami.Units.gridUnit * 8)
    readonly property int ringColumns: {
        if (inPanel) {
            return 4;
        }
        const inner = width - padding * 2;
        if (tiny || inner < height * 1.4) {
            return 2;
        }
        return Math.max(2, Math.min(6, Math.floor(inner / (Kirigami.Units.gridUnit * 4.5))));
    }

    // Popup: as tall as the content needs, within reason.
    readonly property real popupWidth: Kirigami.Units.gridUnit * 20
    Layout.minimumWidth: inPanel ? popupWidth : Kirigami.Units.gridUnit * 6
    Layout.minimumHeight: inPanel ? Kirigami.Units.gridUnit * 8 : Kirigami.Units.gridUnit * 6
    Layout.preferredWidth: inPanel ? popupWidth : Kirigami.Units.gridUnit * 18
    Layout.preferredHeight: inPanel
        ? Math.min(Kirigami.Units.gridUnit * 28, Math.max(Kirigami.Units.gridUnit * 8,
            (contentLoader.item ? contentLoader.item.contentHeight : 0) + padding * 2))
        : Kirigami.Units.gridUnit * 12

    WidgetSurface {
        anchors.fill: parent
        material: Plasmoid.configuration.surfaceMaterial
        glassProfile: Plasmoid.configuration.glassProfile
        radius: Plasmoid.configuration.cornerRadius
        windowType: fullView.inPanel ? "popup" : "desktop"
    }

    Loader {
        id: contentLoader
        anchors.fill: parent
        anchors.margins: fullView.padding
        sourceComponent: fullView.rings ? ringsComponent : listComponent
    }

    function openSettings() {
        const action = Plasmoid.internalAction("configure");
        if (action) {
            action.trigger();
        }
    }

    Component {
        id: listComponent
        ListStyleView {
            model: fullView.devicesModel
            summary: fullView.devicesModel
            heroMode: fullView.tiny
            onRefreshRequested: Plasmoid.refresh()
            onPinRequested: function(id) { Plasmoid.configuration.preferredDeviceId = id; }
            onSettingsRequested: fullView.openSettings()
        }
    }

    Component {
        id: ringsComponent
        RingsStyleView {
            model: fullView.devicesModel
            summary: fullView.devicesModel
            columns: fullView.ringColumns
            minimumSlots: fullView.ringColumns === 2 ? 4 : fullView.ringColumns
            showLabels: !fullView.tiny
            fitHeight: !fullView.inPanel
            onSettingsRequested: fullView.openSettings()
        }
    }
}
