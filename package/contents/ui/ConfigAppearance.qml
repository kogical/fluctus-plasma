import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: configPage

    property alias cfg_baseIconSize: baseIconSizeSpin.value
    property alias cfg_iconSpacing: iconSpacingSpin.value
    property alias cfg_maxScale: maxScaleSlider.value
    property alias cfg_influenceRadiusIcons: radiusSlider.value
    property alias cfg_magnificationCurve: curveCombo.currentIndex
    property alias cfg_magnificationEnabled: magCheck.checked
    property alias cfg_bounceEnabled: bounceCheck.checked
    property alias cfg_bounceRatio: bounceRatioSlider.value
    property alias cfg_blurEnabled: blurCheck.checked
    property alias cfg_pillCornerRadius: radiusSpin.value
    property alias cfg_pillOpacity: opacitySlider.value

    readonly property int computedRecommendedThickness: {
        const s0 = baseIconSizeSpin.value;
        const m = magCheck.checked ? maxScaleSlider.value : 1.0;
        const br = bounceCheck.checked ? bounceRatioSlider.value : 0.0;
        return Math.ceil(6 + Math.ceil(s0 * m) + Math.ceil(br * s0 * m) + 6);
    }

    Kirigami.FormLayout {
        QQC2.SpinBox {
            id: baseIconSizeSpin
            Kirigami.FormData.label: i18n("Base icon size:")
            from: 24
            to: 96
            stepSize: 2
        }

        QQC2.SpinBox {
            id: iconSpacingSpin
            Kirigami.FormData.label: i18n("Icon spacing:")
            from: 0
            to: 32
        }

        QQC2.CheckBox {
            id: magCheck
            Kirigami.FormData.label: i18n("Magnification:")
            text: i18n("Enable dock magnification wave")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Maximum zoom:")
            enabled: magCheck.checked
            QQC2.Slider {
                id: maxScaleSlider
                from: 1.0
                to: 2.5
                stepSize: 0.05
            }
            QQC2.Label {
                text: maxScaleSlider.value.toFixed(2) + "×"
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Wave radius:")
            enabled: magCheck.checked
            QQC2.Slider {
                id: radiusSlider
                from: 1.0
                to: 4.0
                stepSize: 0.1
            }
            QQC2.Label {
                text: radiusSlider.value.toFixed(1) + " icons"
            }
        }

        QQC2.ComboBox {
            id: curveCombo
            Kirigami.FormData.label: i18n("Magnification curve:")
            enabled: magCheck.checked
            model: [
                i18n("Raised Cosine (macOS Default)"),
                i18n("Quintic Smootherstep"),
                i18n("Windowed Gaussian")
            ]
        }

        QQC2.CheckBox {
            id: bounceCheck
            Kirigami.FormData.label: i18n("Bounce:")
            text: i18n("Bounce launching and urgent icons")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Bounce height:")
            enabled: bounceCheck.checked
            QQC2.Slider {
                id: bounceRatioSlider
                from: 0.2
                to: 0.9
                stepSize: 0.05
            }
            QQC2.Label {
                text: Math.round(bounceRatioSlider.value * 100) + "%"
            }
        }

        QQC2.CheckBox {
            id: blurCheck
            Kirigami.FormData.label: i18n("Dock pill:")
            text: i18n("Enable rounded blur behind pill")
        }

        QQC2.SpinBox {
            id: radiusSpin
            Kirigami.FormData.label: i18n("Pill corner radius:")
            from: 0
            to: 48
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Pill opacity:")
            QQC2.Slider {
                id: opacitySlider
                from: 0.0
                to: 1.0
                stepSize: 0.02
            }
            QQC2.Label {
                text: Math.round(opacitySlider.value * 100) + "%"
            }
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Recommended panel height:")
            text: i18n("%1 px (prevents zoom and bounce clipping on Wayland)", configPage.computedRecommendedThickness)
            opacity: 0.8
        }
    }
}
