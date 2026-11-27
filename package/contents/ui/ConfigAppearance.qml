import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: configPage

    property alias cfg_panelIconRatio: panelIconRatioSlider.value
    property alias cfg_iconSpacingRatio: iconSpacingRatioSlider.value
    property alias cfg_edgePadding: edgePaddingSpin.value
    property alias cfg_topMargin: topMarginSpin.value

    property alias cfg_backgroundMode: bgModeCombo.currentIndex
    property alias cfg_blurStyle: blurStyleCombo.currentIndex
    property alias cfg_blurEnabled: blurCheck.checked
    property alias cfg_pillColorMode: pillColorModeCombo.currentIndex
    property color cfg_pillCustomColor
    property alias cfg_pillOpacity: opacitySlider.value
    property alias cfg_pillCornerRadius: radiusSpin.value
    property alias cfg_pillBorderWidth: borderWidthSpin.value
    property alias cfg_pillBorderOpacity: borderOpacitySlider.value
    property alias cfg_pillPaddingPrimary: pillPadPrimarySpin.value
    property alias cfg_pillPaddingCross: pillPadCrossSpin.value
    property alias cfg_showReflection: reflectionCheck.checked
    property alias cfg_indicatorStyle: indicatorStyleCombo.currentIndex
    property alias cfg_indicatorPosition: indicatorPosCombo.currentIndex

    readonly property bool isCustomPill: bgModeCombo.currentIndex === 1

    ColorDialog {
        id: pillColorDialog
        title: i18n("Choose Pill Background Color")
        selectedColor: configPage.cfg_pillCustomColor
        onAccepted: configPage.cfg_pillCustomColor = selectedColor
    }

    Kirigami.FormLayout {
        // ---------------------------------------------------------------------
        // Section 1: Sizing & Panel Fit
        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Sizing & Panel Fit")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Icon size:")
            Layout.fillWidth: true

            QQC2.Slider {
                id: panelIconRatioSlider
                Layout.fillWidth: true
                from: 0.25
                to: 1.0
                stepSize: 0.05
            }
            QQC2.Label {
                Layout.minimumWidth: 46
                horizontalAlignment: Text.AlignRight
                text: Math.round(panelIconRatioSlider.value * 100) + "%"
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Icon spacing:")
            Layout.fillWidth: true

            QQC2.Slider {
                id: iconSpacingRatioSlider
                Layout.fillWidth: true
                from: 0.0
                to: 0.50
                stepSize: 0.02
            }
            QQC2.Label {
                Layout.minimumWidth: 46
                horizontalAlignment: Text.AlignRight
                text: Math.round(iconSpacingRatioSlider.value * 100) + "%"
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Information
            text: i18n("Icon size is a percentage of available panel thickness; icon spacing is a percentage of icon size. Resize your Plasma panel in Edit Mode to scale the dock.")
        }

        QQC2.SpinBox {
            id: edgePaddingSpin
            Kirigami.FormData.label: i18n("Screen-edge padding:")
            from: 0
            to: 32
            textFromValue: function(value) { return value + " px"; }
            valueFromText: function(text) { return parseInt(text, 10) || 0; }
        }

        QQC2.SpinBox {
            id: topMarginSpin
            Kirigami.FormData.label: i18n("Outer headroom margin:")
            from: 0
            to: 32
            textFromValue: function(value) { return value + " px"; }
            valueFromText: function(text) { return parseInt(text, 10) || 0; }
        }

        // ---------------------------------------------------------------------
        // Section 2: Background & Pill Surface
        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Background & Pill")
        }

        QQC2.ComboBox {
            id: bgModeCombo
            Kirigami.FormData.label: i18n("Background mode:")
            Layout.fillWidth: true
            model: [
                i18n("Default (Panel / Panel Colorizer)"),
                i18n("Custom Glass Pill"),
                i18n("Plasma Theme Surface"),
                i18n("Transparent (Icons Only)")
            ]
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: bgModeCombo.currentIndex === 0
            type: Kirigami.MessageType.Information
            text: i18n("Default mode draws no custom pill and leaves KWin blur untouched, letting your Plasma panel theme or Panel Colorizer handle styling.")
        }

        QQC2.ComboBox {
            id: blurStyleCombo
            visible: configPage.isCustomPill
            Kirigami.FormData.label: i18n("Glass / blur material:")
            Layout.fillWidth: true
            model: [
                i18n("Frosted Glass (Standard)"),
                i18n("Liquid Glass (Specular & Vibrant)"),
                i18n("Acrylic (Dense Translucent)"),
                i18n("Clear Blur (Backdrop Only)"),
                i18n("Solid (No Blur)")
            ]
        }

        QQC2.CheckBox {
            id: blurCheck
            visible: configPage.isCustomPill
            enabled: blurStyleCombo.currentIndex !== 4
            Kirigami.FormData.label: i18n("Compositor blur:")
            text: i18n("Enable KWin rounded backdrop blur behind pill")
        }

        QQC2.ComboBox {
            id: pillColorModeCombo
            visible: configPage.isCustomPill
            Kirigami.FormData.label: i18n("Pill tint:")
            Layout.fillWidth: true
            model: [
                i18n("Follow Plasma Theme"),
                i18n("Custom Color")
            ]
        }

        RowLayout {
            visible: configPage.isCustomPill && pillColorModeCombo.currentIndex === 1
            Kirigami.FormData.label: i18n("Custom color:")
            spacing: Kirigami.Units.smallSpacing

            Rectangle {
                width: 28
                height: 22
                radius: 4
                color: configPage.cfg_pillCustomColor
                border.width: 1
                border.color: Kirigami.Theme.disabledTextColor
            }

            QQC2.Button {
                text: i18n("Choose Color…")
                onClicked: pillColorDialog.open()
            }
        }

        RowLayout {
            visible: configPage.isCustomPill
            Kirigami.FormData.label: i18n("Pill opacity:")
            Layout.fillWidth: true

            QQC2.Slider {
                id: opacitySlider
                Layout.fillWidth: true
                from: 0.0
                to: 1.0
                stepSize: 0.02
            }
            QQC2.Label {
                Layout.minimumWidth: 42
                horizontalAlignment: Text.AlignRight
                text: Math.round(opacitySlider.value * 100) + "%"
            }
        }

        QQC2.SpinBox {
            id: radiusSpin
            visible: configPage.isCustomPill
            Kirigami.FormData.label: i18n("Corner radius:")
            from: 0
            to: 48
            textFromValue: function(value) { return value + " px"; }
            valueFromText: function(text) { return parseInt(text, 10) || 0; }
        }

        QQC2.SpinBox {
            id: borderWidthSpin
            visible: configPage.isCustomPill
            Kirigami.FormData.label: i18n("Border width:")
            from: 0
            to: 6
            textFromValue: function(value) { return value + " px"; }
            valueFromText: function(text) { return parseInt(text, 10) || 0; }
        }

        RowLayout {
            visible: configPage.isCustomPill && borderWidthSpin.value > 0
            Kirigami.FormData.label: i18n("Border opacity:")
            Layout.fillWidth: true

            QQC2.Slider {
                id: borderOpacitySlider
                Layout.fillWidth: true
                from: 0.0
                to: 1.0
                stepSize: 0.02
            }
            QQC2.Label {
                Layout.minimumWidth: 42
                horizontalAlignment: Text.AlignRight
                text: Math.round(borderOpacitySlider.value * 100) + "%"
            }
        }

        QQC2.SpinBox {
            id: pillPadPrimarySpin
            visible: configPage.isCustomPill || bgModeCombo.currentIndex === 2
            Kirigami.FormData.label: i18n("Pill end padding:")
            from: 0
            to: 40
            textFromValue: function(value) { return value + " px"; }
            valueFromText: function(text) { return parseInt(text, 10) || 0; }
        }

        QQC2.SpinBox {
            id: pillPadCrossSpin
            visible: configPage.isCustomPill || bgModeCombo.currentIndex === 2
            Kirigami.FormData.label: i18n("Pill cross padding:")
            from: 0
            to: 24
            textFromValue: function(value) { return value + " px"; }
            valueFromText: function(text) { return parseInt(text, 10) || 0; }
        }

        // ---------------------------------------------------------------------
        // Section 3: Visual Effects & Indicators
        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Visual Effects & Indicators")
        }

        QQC2.ComboBox {
            id: indicatorStyleCombo
            Kirigami.FormData.label: i18n("Running indicator style:")
            Layout.fillWidth: true
            model: [
                i18n("Pill Bar (Default)"),
                i18n("Classic Dot"),
                i18n("Glowing Underline"),
                i18n("Minimal Line"),
                i18n("None")
            ]
        }

        QQC2.ComboBox {
            id: indicatorPosCombo
            visible: indicatorStyleCombo.currentIndex !== 4
            Kirigami.FormData.label: i18n("Indicator position:")
            Layout.fillWidth: true
            model: [
                i18n("Screen Edge (Outer)"),
                i18n("Inside Icon")
            ]
        }

        QQC2.CheckBox {
            id: reflectionCheck
            Kirigami.FormData.label: i18n("Reflection:")
            text: i18n("Show subtle icon reflection beneath bottom dock")
        }
    }
}
