// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Picks the widget's surface:
//   - Liquid Glass when it was chosen and Fluctus Glass is running (GlassSurface.qml);
//   - the Frosted pre-Liquid Glass look otherwise, including when the Kogical.FluctusGlass
//     QML module is not installed at all (GlassSurface.qml then fails to load);
//   - nothing for "Plasma theme", where Plasma draws its own background.
// GlassSurface.qml is only ever loaded here, so a missing Fluctus Glass never stops the widget.
import QtQuick
import org.kde.plasma.plasmoid

Item {
    id: surface

    /// 0 = Liquid Glass (Frosted until Fluctus Glass runs), 1 = Frosted, 2 = Plasma theme.
    property int material: 0
    property string glassProfile: '{"preset":"global"}'
    property real radius: 20
    property string windowType: "desktop"

    readonly property bool liquidRequested: material === 0 && Plasmoid.glassEffectState === 2
    readonly property bool liquidShown: liquidRequested && liquidLoader.status === Loader.Ready
    /// What the user actually sees: "liquid", "frosted" or "plasma".
    readonly property string effective: material === 2 ? "plasma" : (liquidShown ? "liquid" : "frosted")

    Loader {
        id: liquidLoader
        anchors.fill: parent
        active: surface.liquidRequested
        source: "GlassSurface.qml"
        onLoaded: {
            item.glassProfile = Qt.binding(() => surface.glassProfile);
            item.radius = Qt.binding(() => surface.radius);
            item.windowType = Qt.binding(() => surface.windowType);
        }
    }

    Loader {
        anchors.fill: parent
        active: surface.effective === "frosted"
        sourceComponent: FrostedSurface {
            radius: surface.radius
            windowType: surface.windowType
        }
    }
}
