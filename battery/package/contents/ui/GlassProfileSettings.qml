// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// The Liquid Glass look (shared GlassProfile + GlassSettingsDialog from Kogical.FluctusGlass).
// Loaded through a Loader by ConfigAppearance.qml: when Fluctus Glass's QML module is not
// installed this file fails to load and the page shows a hint instead of breaking.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Kogical.FluctusGlass

RowLayout {
    id: row

    /// JSON glass profile (`cfg_glassProfile`).
    property string profile: '{"preset":"global"}'
    signal edited(string profile)

    spacing: Kirigami.Units.smallSpacing

    GlassProfile {
        id: model
        Component.onCompleted: loadJson(row.profile)
    }
    onProfileChanged: model.loadJson(profile)

    GlassSettingsDialog {
        id: dialog
        profile: row.profile
        onProfileChanged: row.edited(profile)
    }

    function presetName(id) {
        switch (id) {
        case "global":  return i18n("Same as everywhere else");
        case "light":   return i18n("Light");
        case "medium":  return i18n("Medium");
        case "full":    return i18n("Full");
        case "crystal": return i18n("Crystal");
        case "plain":   return i18n("Plain blur");
        case "none":    return i18n("Off");
        default:        return i18n("Custom");
        }
    }

    QQC2.Label {
        text: row.presetName(model.preset)
    }

    QQC2.Button {
        text: i18n("Customize…")
        icon.name: "preferences-desktop-effects"
        onClicked: dialog.open()
    }

    QQC2.Button {
        visible: !model.followGlobal
        text: i18n("Reset")
        icon.name: "edit-undo"
        onClicked: {
            model.resetToGlobal();
            row.edited(model.json);
        }
    }
}
