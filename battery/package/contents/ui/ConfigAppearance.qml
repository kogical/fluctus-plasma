// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Appearance page: style, glass and the panel icon, with a live preview in the header.
// Kogical.FluctusGlass is only reached through GlassProfileSettings.qml (a Loader), so the page
// works without Fluctus Glass installed.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.plasma.plasmoid

KCM.SimpleKCM {
    id: page

    property alias cfg_widgetStyle: styleCombo.currentIndex
    property alias cfg_surfaceMaterial: materialCombo.currentIndex
    property string cfg_glassProfile: '{"preset":"global"}'
    property alias cfg_cornerRadius: radiusSlider.value
    property alias cfg_showPercentageInPanel: showPctCheck.checked
    property bool cfg_showDeviceIconInPanel
    property alias cfg_showAllBatteriesInPanel: showAllCheck.checked
    property string cfg_activePreset

    readonly property bool liquidAvailable: Plasmoid.glassEffectState === 2
    readonly property var styleDescriptions: [
        i18n("A title and a list: every device with its charge on a slim bar."),
        i18n("A grid of charge rings with the device in the middle, like the Batteries widget on macOS.")
    ]
    readonly property var materialDescriptions: [
        i18n("Clear glass whose curved edge bends what is behind it, like macOS Tahoe."),
        i18n("Dense, softly blurred glass with a crisp outline, the look of macOS before Liquid Glass. Works without Fluctus Glass."),
        i18n("Your Plasma theme's widget background.")
    ]

    // Editing the look by hand means the widget no longer matches a named preset.
    function markCustom() {
        cfg_activePreset = "";
    }

    header: ColumnLayout {
        spacing: 0

        BatteryPreview {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 10
            widgetStyle: styleCombo.currentIndex
            material: materialCombo.currentIndex
            cornerRadius: radiusSlider.value
            wallpaper: lightWallpaperSwitch.checked ? 1 : 0
            liquidAvailable: page.liquidAvailable
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing

            QQC2.Label {
                Layout.fillWidth: true
                text: i18n("Preview with sample devices on a sample wallpaper.")
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                elide: Text.ElideRight
            }
            QQC2.Switch {
                id: lightWallpaperSwitch
                text: i18n("Light wallpaper")
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }
    }

    Kirigami.FormLayout {
        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Style")
        }

        QQC2.ComboBox {
            id: styleCombo
            Kirigami.FormData.label: i18n("Layout:")
            model: [i18n("List"), i18n("Rings")]
            onActivated: page.markCustom()
        }

        ConfigHint {
            text: page.styleDescriptions[Math.max(0, styleCombo.currentIndex)]
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Glass")
        }

        QQC2.ComboBox {
            id: materialCombo
            Kirigami.FormData.label: i18n("Material:")
            model: [i18n("Liquid Glass"), i18n("Frosted"), i18n("Plasma theme")]
            onActivated: page.markCustom()
        }

        ConfigHint {
            text: page.materialDescriptions[Math.max(0, materialCombo.currentIndex)]
        }

        GlassEffectNotice {
            visible: materialCombo.currentIndex === 0 && effectState !== 2
            liquidChosen: true
        }

        Loader {
            id: glassLook
            visible: materialCombo.currentIndex === 0 && status !== Loader.Error
            Kirigami.FormData.label: i18n("Liquid Glass look:")
            source: "GlassProfileSettings.qml"
            onLoaded: {
                item.profile = Qt.binding(() => page.cfg_glassProfile);
                item.edited.connect(json => {
                    page.cfg_glassProfile = json;
                    page.markCustom();
                });
            }
        }

        ConfigHint {
            visible: materialCombo.currentIndex === 0
            text: glassLook.status === Loader.Error
                ? i18n("Fine-tuning Liquid Glass needs Fluctus Glass installed.")
                : i18n("Strength, frost and color of the glass. “Same as everywhere else” follows Fluctus Glass's own settings.")
        }

        ConfigSlider {
            id: radiusSlider
            visible: materialCombo.currentIndex !== 2
            Kirigami.FormData.label: i18n("Corner roundness:")
            from: 0
            to: 36
            stepSize: 1
            format: function(v) { return i18n("%1 px", Math.round(v)); }
            onMoved: page.markCustom()
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("In the Panel")
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Icon:")
            model: [i18n("Battery"), i18n("The device itself")]
            currentIndex: page.cfg_showDeviceIconInPanel ? 1 : 0
            onActivated: page.cfg_showDeviceIconInPanel = currentIndex === 1
        }

        QQC2.CheckBox {
            id: showPctCheck
            Kirigami.FormData.label: i18n("Level:")
            text: i18n("Show the percentage next to the icon")
        }

        QQC2.CheckBox {
            id: showAllCheck
            Kirigami.FormData.label: i18n("Several devices:")
            text: i18n("Show up to three devices side by side")
        }

        ConfigHint {
            text: i18n("Only in horizontal panels. Otherwise the panel shows the device chosen on the Devices page.")
        }
    }
}
