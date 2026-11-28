// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Shared Liquid Glass surface driven by Kogical.FluctusGlass (GlassProfile preference library):
//   - On the desktop (inside DesktopView, where the wallpaper lives in the same QQuickWindow),
//     uses Kogical.LiquidGlass in real backdrop mode sampling the live wallpaper item.
//   - In floating windows / panel popups, requests compositor-level Fluctus Glass via GlassArea
//     (with Kogical.LiquidGlass overlay fallback when Fluctus Glass is inactive).
import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import Kogical.FluctusGlass
import Kogical.LiquidGlass

Item {
    id: surface

    property bool glassEnabled: true
    property string glassProfile: '{"preset":"global"}'
    property real radius: 20
    property string windowType: ""

    readonly property bool isDesktopSurface: windowType === "desktop"
    readonly property Item wallpaperBackdrop: {
        if (!isDesktopSurface) {
            return null;
        }
        const containment = Plasmoid.containment;
        if (!containment) {
            return null;
        }
        const wp = containment.wallpaperGraphicsObject;
        if (!wp || wp.Window.window !== surface.Window.window) {
            return null;
        }
        return wp;
    }
    // Use in-scene LiquidGlass backdrop mode when embedded in DesktopView with a live wallpaper.
    readonly property bool useInSceneBackdrop: wallpaperBackdrop !== null
    readonly property bool compositorActive: !useInSceneBackdrop && glassArea.active
    readonly property bool compositorAvailable: glassArea.available
    readonly property bool inEditMode: Boolean(
        Plasmoid.containment && Plasmoid.containment.corona && Plasmoid.containment.corona.editMode
    )
    readonly property bool isDarkTheme: {
        const c = Kirigami.Theme.backgroundColor;
        return (0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b) < 0.5;
    }

    property int _backdropRevision: 0

    GlassProfile {
        id: profileModel
        Component.onCompleted: loadJson(surface.glassProfile)
    }

    onGlassProfileChanged: profileModel.loadJson(glassProfile)
    onWidthChanged: _backdropRevision++
    onHeightChanged: _backdropRevision++
    onXChanged: _backdropRevision++
    onYChanged: _backdropRevision++
    Window.onWindowChanged: _backdropRevision++

    // While dragging/resizing the widget in Plasma Edit Mode, keep the refracted wallpaper
    // coordinates locked to the widget's live position; 0% CPU outside Edit Mode.
    Timer {
        interval: 33
        repeat: true
        running: surface.useInSceneBackdrop && surface.inEditMode && surface.visible
        onTriggered: surface._backdropRevision++
    }

    // One-shot refresh right after Edit Mode ends or geometry settles.
    onInEditModeChanged: {
        if (!inEditMode && useInSceneBackdrop) {
            settleTimer.restart();
        }
    }
    Timer {
        id: settleTimer
        interval: 120
        repeat: false
        onTriggered: surface._backdropRevision++
    }

    HoverHandler {
        id: hoverHandler
        enabled: surface.glassEnabled && profileModel.pointerEffects && !surface.compositorActive
    }

    // Compositor-level Fluctus Glass request for floating popups / standalone windows.
    GlassArea {
        id: glassArea
        anchors.fill: parent
        glassEnabled: surface.glassEnabled
            && !surface.useInSceneBackdrop
            && surface.visible
            && width > 0
            && height > 0
        radius: Math.min(surface.radius, Math.min(width, height) * 0.5)
        profile: surface.glassProfile
        windowType: surface.windowType
    }

    // In-scene Liquid Glass (real wallpaper refraction on desktop, or overlay fallback in popups).
    LiquidGlass {
        id: liquidGlass
        anchors.fill: parent
        visible: surface.glassEnabled
            && (surface.useInSceneBackdrop || !glassArea.active)
            && profileModel.mode !== "none"
            && width > 0
            && height > 0
        backdrop: surface.useInSceneBackdrop ? surface.wallpaperBackdrop : null
        backdropRevision: surface._backdropRevision
        material: profileModel.mode === "plain" ? "frosted"
            : (profileModel.preset === "crystal" ? "crystal" : "liquid")
        dark: surface.isDarkTheme
        radius: Math.min(surface.radius, Math.min(width, height) * 0.5)
        intensity: profileModel.intensity
        refraction: profileModel.mode === "plain" ? 0 : profileModel.refraction
        magnify: profileModel.mode === "plain" ? 0 : profileModel.magnify
        dispersion: profileModel.mode === "plain" ? 0 : profileModel.dispersion
        frost: profileModel.frost
        saturation: profileModel.saturation
        brightness: profileModel.brightness
        tint: profileModel.tintStrength > 0.001 ? profileModel.tintColor : Kirigami.Theme.backgroundColor
        tintAlpha: profileModel.tintStrength > 0.001 ? profileModel.tintStrength : (surface.isDarkTheme ? 0.16 : 0.24)
        highlight: profileModel.mode === "plain" ? 0.18 : profileModel.highlight
        sheen: profileModel.mode === "plain" ? 0.02 : profileModel.sheen
        edgeShade: profileModel.mode === "plain" ? 0.0 : profileModel.edgeShade
        bezel: Math.min(profileModel.bezel, Math.min(width, height) * 0.45)
        pointer: hoverHandler.point.position
        pointerStrength: (profileModel.pointerEffects && hoverHandler.hovered) ? 1.0 : 0.0

        Behavior on pointerStrength {
            NumberAnimation {
                duration: 180
                easing.type: Easing.OutCubic
            }
        }
    }
}
