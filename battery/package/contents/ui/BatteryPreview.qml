// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Live preview for the settings pages: the real list or rings view, with sample devices, on a
// sample wallpaper, inside the material the user would actually get (Liquid Glass only when
// Fluctus Glass is running, otherwise Frosted).
import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg
import Kogical.LiquidGlass

Item {
    id: preview

    /// 0 = list, 1 = rings
    property int widgetStyle: 0
    /// 0 = Liquid Glass, 1 = Frosted, 2 = Plasma theme
    property int material: 0
    property real cornerRadius: 22
    /// 0 = dusk (dark), 1 = daylight (light)
    property int wallpaper: 0
    /// Fluctus Glass is running, so Liquid Glass can really be shown.
    property bool liquidAvailable: false

    readonly property bool plasmaTheme: material === 2
    readonly property bool showsLiquid: material === 0 && liquidAvailable
    readonly property bool darkGlass: wallpaper === 0
    readonly property color themeText: Kirigami.Theme.textColor
    readonly property color themeBackground: Kirigami.Theme.backgroundColor
    readonly property color themeNeutral: Kirigami.Theme.neutralTextColor

    implicitWidth: 420
    implicitHeight: 180
    clip: true

    // ---- Sample wallpaper ------------------------------------------------
    Item {
        id: wall
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: preview.wallpaper === 0 ? "#1d1838" : "#9fc6ee" }
                GradientStop { position: 0.6; color: preview.wallpaper === 0 ? "#3a2457" : "#e7d3e8" }
                GradientStop { position: 1.0; color: preview.wallpaper === 0 ? "#12233f" : "#f6e2c7" }
            }
        }
        Repeater {
            model: 8
            Rectangle {
                required property int index
                readonly property real s: preview.height * (0.4 + (index % 3) * 0.25)
                x: (index * 0.157 % 1.0) * preview.width - s * 0.3
                y: preview.height * ((index * 0.29) % 1.0) - s * 0.3
                width: s
                height: s
                radius: s / 2
                opacity: preview.wallpaper === 0 ? 0.85 : 0.75
                color: preview.wallpaper === 0
                    ? Qt.hsla((0.62 + index * 0.09) % 1.0, 0.7, 0.55, 1)
                    : Qt.hsla((0.95 + index * 0.11) % 1.0, 0.65, 0.68, 1)
            }
        }
        // A sharp shape so the glass's edge is visible.
        Rectangle {
            x: card.x + card.width * 0.72
            y: card.y - preview.height * 0.12
            width: preview.width * 0.06
            height: preview.height * 0.55
            rotation: 18
            color: preview.wallpaper === 0 ? "#ffb347" : "#ff6f61"
        }
    }

    // ---- Widget card -----------------------------------------------------
    readonly property real cardPadding: Math.round(Math.max(10, card.height * 0.08))

    Item {
        id: card
        readonly property real maxH: preview.height - Kirigami.Units.largeSpacing * 2
        width: preview.widgetStyle === 1
            ? Math.min(preview.width - Kirigami.Units.largeSpacing * 2, maxH * 2.1)
            : Math.min(preview.width - Kirigami.Units.largeSpacing * 2, Math.max(maxH * 1.5, Kirigami.Units.gridUnit * 15))
        height: maxH
        anchors.centerIn: parent

        LiquidGlass {
            anchors.fill: parent
            visible: !preview.plasmaTheme
            material: preview.showsLiquid ? "liquid" : "frosted"
            dark: preview.darkGlass
            radius: Math.min(preview.cornerRadius, Math.min(width, height) / 2)
            backdrop: wall
            backdropRevision: Math.round(preview.width) + Math.round(preview.height) * 10000 + preview.wallpaper
        }

        KSvg.FrameSvgItem {
            anchors.fill: parent
            visible: preview.plasmaTheme
            imagePath: "widgets/background"
        }

        Item {
            id: content
            anchors.fill: parent
            anchors.margins: preview.cardPadding

            // Glass content follows the sample wallpaper; Plasma's own card follows the theme.
            Kirigami.Theme.inherit: false
            Kirigami.Theme.textColor: preview.plasmaTheme ? preview.themeText : (preview.darkGlass ? "#f5f5f7" : "#1d1d1f")
            Kirigami.Theme.backgroundColor: preview.plasmaTheme ? preview.themeBackground : (preview.darkGlass ? "#2c2c2e" : "#f2f2f7")
            Kirigami.Theme.neutralTextColor: preview.plasmaTheme ? preview.themeNeutral : (preview.darkGlass ? "#ffd60a" : "#b25000")

            Loader {
                anchors.fill: parent
                sourceComponent: preview.widgetStyle === 1 ? ringsSample : listSample
            }
        }
    }

    Component {
        id: listSample
        ListStyleView {
            model: samples
            summary: sampleSummary
            interactive: false
            maxRows: 3
            fontSize: Math.max(8, Math.round(card.height * 0.062))
        }
    }

    Component {
        id: ringsSample
        RingsStyleView {
            model: samples
            summary: sampleSummary
            interactive: false
            columns: 4
            fontSize: Math.max(9, Math.round(card.height * 0.075))
        }
    }

    // ---- Sample devices --------------------------------------------------
    QtObject {
        id: sampleSummary
        readonly property int count: samples.count
        readonly property int batteryCount: samples.count
        readonly property bool hasAnyBattery: true
        readonly property bool refreshing: false
        readonly property string primarySeverity: "normal"
        readonly property string primaryCategoryIcon: "phone-symbolic"
        readonly property string primaryPercentageText: "80%"
        readonly property bool hasPrimaryPercentage: true
        readonly property real primaryPercentage: 80
        readonly property string primaryName: i18n("Phone")
    }

    ListModel {
        id: samples
        Component.onCompleted: {
            const rows = [
                [i18n("Phone"), "phone-symbolic", 80, false, "normal"],
                [i18n("Mouse"), "input-mouse-symbolic", 64, true, "charging"],
                [i18n("Headphones"), "audio-headphones-symbolic", 45, false, "normal"],
                [i18n("Game Controller"), "input-gaming-symbolic", 15, false, "warning"]
            ];
            for (let i = 0; i < rows.length; ++i) {
                const r = rows[i];
                append({
                    deviceId: "sample-" + i,
                    name: r[0],
                    categoryIcon: r[1],
                    transportLabel: "",
                    powerAvailable: true,
                    percentage: r[2],
                    hasPercentage: true,
                    percentageText: r[2] + "%",
                    isCharging: r[3],
                    chargingLabel: r[3] ? i18n("Charging") : "",
                    severity: r[4],
                    unavailableLabel: "",
                    cells: [],
                    cellCount: 0,
                    isPrimary: i === 0,
                    needsPermission: false
                });
            }
        }
    }
}
