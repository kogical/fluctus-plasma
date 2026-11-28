// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Behavior page: when a level counts as low or critical, and how often to ask devices that
// don't report changes by themselves.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: page

    property alias cfg_lowThreshold: lowSlider.value
    property alias cfg_criticalThreshold: criticalSlider.value
    property alias cfg_pollIntervalSeconds: pollSlider.value

    Kirigami.FormLayout {
        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Battery Levels")
        }

        ConfigSlider {
            id: lowSlider
            Kirigami.FormData.label: i18n("Low:")
            from: 5
            to: 50
            stepSize: 1
            format: function(v) { return i18n("%1%", Math.round(v)); }
            onMoved: {
                if (criticalSlider.value > value) {
                    criticalSlider.value = value;
                }
            }
        }

        ConfigHint {
            text: i18n("At or below this, a device's level turns yellow.")
        }

        ConfigSlider {
            id: criticalSlider
            Kirigami.FormData.label: i18n("Critical:")
            from: 1
            to: lowSlider.value
            stepSize: 1
            format: function(v) { return i18n("%1%", Math.round(v)); }
        }

        ConfigHint {
            text: i18n("At or below this, it turns red, in the widget and in the panel.")
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Updates")
        }

        ConfigSlider {
            id: pollSlider
            Kirigami.FormData.label: i18n("Check every:")
            from: 10
            to: 600
            stepSize: 10
            format: function(v) {
                const s = Math.round(v);
                if (s < 60) {
                    return i18n("%1 s", s);
                }
                return s % 60 === 0 ? i18n("%1 min", s / 60) : i18n("%1 min %2 s", Math.floor(s / 60), s % 60);
            }
        }

        ConfigHint {
            text: i18n("Most devices report changes by themselves. Receivers that have to be asked are checked this often; asking less often saves a little of their battery.")
        }
    }
}
