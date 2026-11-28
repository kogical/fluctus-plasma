// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Appearance configuration using Kogical.FluctusGlass's shared GlassProfile and
// GlassSettingsDialog preference library (zero duplicated glass sliders or presets).
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import Kogical.FluctusGlass

KCM.SimpleKCM {
    id: page

    property alias cfg_useLiquidGlass: useGlassSwitch.checked
    property string cfg_glassProfile: '{"preset":"global"}'
    property alias cfg_cornerRadius: radiusSpin.value
    property alias cfg_showPercentageInPanel: showPctCheck.checked
    property alias cfg_showDeviceIconInPanel: showDevIconCheck.checked
    property alias cfg_showAllBatteriesInPanel: showAllCheck.checked

    GlassProfile {
        id: glassProfile
        Component.onCompleted: loadJson(page.cfg_glassProfile)
    }

    onCfg_glassProfileChanged: glassProfile.loadJson(cfg_glassProfile)

    function presetDisplayName(id) {
        switch (id) {
        case "global":  return i18n("Global Default");
        case "light":   return i18n("Light");
        case "medium":  return i18n("Medium");
        case "full":    return i18n("Full");
        case "crystal": return i18n("Crystal");
        case "plain":   return i18n("Plain Blur");
        case "none":    return i18n("Off");
        default:        return i18n("Custom");
        }
    }

    GlassSettingsDialog {
        id: glassDialog
        profile: page.cfg_glassProfile
        onProfileChanged: page.cfg_glassProfile = profile
    }

    Kirigami.FormLayout {
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Liquid Glass")
        }

        QQC2.Switch {
            id: useGlassSwitch
            Kirigami.FormData.label: i18n("Surface:")
            text: i18n("Render desktop widget and popup with Liquid Glass")
        }

        RowLayout {
            visible: useGlassSwitch.checked
            Kirigami.FormData.label: i18n("Glass profile:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: page.presetDisplayName(glassProfile.preset)
                font.weight: Font.Medium
            }

            QQC2.Button {
                text: i18n("Customize Liquid Glass…")
                icon.name: "preferences-desktop-effects"
                onClicked: glassDialog.open()
            }

            QQC2.Button {
                visible: !glassProfile.followGlobal
                text: i18n("Reset")
                icon.name: "edit-undo"
                onClicked: {
                    glassProfile.resetToGlobal();
                    page.cfg_glassProfile = glassProfile.json;
                }
            }
        }

        QQC2.SpinBox {
            id: radiusSpin
            visible: useGlassSwitch.checked
            Kirigami.FormData.label: i18n("Corner radius:")
            from: 0
            to: 36
            textFromValue: function(value) { return i18n("%1 px", value); }
            valueFromText: function(text) { return parseInt(text, 10) || 20; }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Panel Display")
        }

        QQC2.CheckBox {
            id: showPctCheck
            Kirigami.FormData.label: i18n("Compact view:")
            text: i18n("Show battery percentage")
        }

        QQC2.CheckBox {
            id: showDevIconCheck
            text: i18n("Use device category icon instead of battery capsule")
        }

        QQC2.CheckBox {
            id: showAllCheck
            text: i18n("Show up to 3 batteries side-by-side in horizontal panels")
        }
    }
}
