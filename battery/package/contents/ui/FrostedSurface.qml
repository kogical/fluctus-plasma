// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// The pre-Liquid Glass macOS look (through Sequoia): dense, softly blurred frosted glass with a
// crisp hairline. Needs nothing beyond Kogical.LiquidGlass, which is built into the widget, so it
// is the fallback whenever Fluctus Glass is missing.
//   - Desktop: frosts the live wallpaper in the same scene.
//   - Panel popup: Plasma's dialog is already blurred, translucent glass; nothing is drawn.
import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import Kogical.LiquidGlass

import "Style.js" as Style

Item {
    id: surface

    property real radius: 20
    property string windowType: "desktop"

    readonly property bool dark: !Style.isDark(Kirigami.Theme.backgroundColor)
    readonly property Item wallpaperBackdrop: {
        if (windowType !== "desktop") {
            return null;
        }
        const containment = Plasmoid.containment;
        const wp = containment ? containment.wallpaperGraphicsObject : null;
        return wp && wp.Window.window === surface.Window.window ? wp : null;
    }
    readonly property bool inEditMode: Boolean(
        Plasmoid.containment && Plasmoid.containment.corona && Plasmoid.containment.corona.editMode
    )

    property int _backdropRevision: 0
    onWidthChanged: _backdropRevision++
    onHeightChanged: _backdropRevision++
    onXChanged: _backdropRevision++
    onYChanged: _backdropRevision++
    Window.onWindowChanged: _backdropRevision++

    // Follow the widget while it is dragged in Edit Mode; nothing runs otherwise.
    Timer {
        interval: 33
        repeat: true
        running: surface.wallpaperBackdrop !== null && surface.inEditMode && surface.visible
        onTriggered: surface._backdropRevision++
    }

    LiquidGlass {
        anchors.fill: parent
        visible: surface.windowType === "desktop" && surface.wallpaperBackdrop !== null
        material: "frosted"
        dark: surface.dark
        radius: Math.min(surface.radius, Math.min(width, height) / 2)
        backdrop: surface.wallpaperBackdrop
        backdropRevision: surface._backdropRevision
    }

    // Desktop without a wallpaper in the same scene: an opaque-enough card that still reads well.
    Rectangle {
        anchors.fill: parent
        visible: surface.windowType === "desktop" && surface.wallpaperBackdrop === null
        radius: Math.min(surface.radius, Math.min(width, height) / 2)
        color: Style.alpha(Kirigami.Theme.backgroundColor, 0.9)
        border.width: 1
        border.color: Style.alpha(Kirigami.Theme.textColor, 0.14)
    }
}
