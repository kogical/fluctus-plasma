import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: physicsPage

    property alias cfg_magnificationEnabled: magCheck.checked
    property alias cfg_maxZoomPanelRatio: maxZoomSlider.value
    property alias cfg_influenceRadiusIcons: radiusSlider.value
    property alias cfg_magnificationCurve: curveCombo.currentIndex
    property alias cfg_bounceEnabled: bounceCheck.checked
    property alias cfg_bounceRatio: bounceRatioSlider.value

    // Read-only context from current configuration
    property double cfg_panelIconRatio

    readonly property real effectiveZoomMultiplier: {
        const base = Math.max(0.25, cfg_panelIconRatio || 0.65);
        return Math.max(1.0, maxZoomSlider.value / base);
    }

    Kirigami.FormLayout {
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Magnification Wave")
        }

        QQC2.CheckBox {
            id: magCheck
            Kirigami.FormData.label: i18n("Magnification:")
            text: i18n("Enable smooth dock magnification wave on hover")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Maximum zoom size:")
            Layout.fillWidth: true
            enabled: magCheck.checked

            QQC2.Slider {
                id: maxZoomSlider
                Layout.fillWidth: true
                from: 0.30
                to: 1.0
                stepSize: 0.05
            }
            QQC2.Label {
                Layout.minimumWidth: 88
                horizontalAlignment: Text.AlignRight
                text: Math.round(maxZoomSlider.value * 100) + "% (" + physicsPage.effectiveZoomMultiplier.toFixed(2) + "×)"
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Wave radius:")
            Layout.fillWidth: true
            enabled: magCheck.checked

            QQC2.Slider {
                id: radiusSlider
                Layout.fillWidth: true
                from: 1.0
                to: 4.0
                stepSize: 0.1
            }
            QQC2.Label {
                Layout.minimumWidth: 64
                horizontalAlignment: Text.AlignRight
                text: radiusSlider.value.toFixed(1) + " icons"
            }
        }

        QQC2.ComboBox {
            id: curveCombo
            Kirigami.FormData.label: i18n("Falloff curve:")
            Layout.fillWidth: true
            enabled: magCheck.checked
            model: [
                i18n("Raised Cosine (macOS Default)"),
                i18n("Quintic Smootherstep"),
                i18n("Windowed Gaussian")
            ]
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Launch & Attention Bounce")
        }

        QQC2.CheckBox {
            id: bounceCheck
            Kirigami.FormData.label: i18n("Bounce:")
            text: i18n("Bounce launching and attention-demanding icons")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Bounce height:")
            Layout.fillWidth: true
            enabled: bounceCheck.checked

            QQC2.Slider {
                id: bounceRatioSlider
                Layout.fillWidth: true
                from: 0.2
                to: 0.9
                stepSize: 0.05
            }
            QQC2.Label {
                Layout.minimumWidth: 46
                horizontalAlignment: Text.AlignRight
                text: Math.round(bounceRatioSlider.value * 100) + "%"
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Information
            text: i18n("Maximum zoom size is a target percentage of available panel thickness. Both hover zoom and bounce automatically adapt to your KDE Plasma panel without clipping.")
        }
    }
}
