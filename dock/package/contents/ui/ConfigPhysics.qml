import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.plasma.plasmoid

KCM.SimpleKCM {
    id: page

    property alias cfg_magnificationEnabled: magnifyCheck.checked
    property alias cfg_maxZoomPanelRatio: zoomSlider.value
    property alias cfg_influenceRadiusIcons: reachSlider.value
    property alias cfg_magnificationCurve: curveCombo.currentIndex
    property alias cfg_bounceEnabled: bounceCheck.checked
    property alias cfg_bounceRatio: bounceSlider.value

    // Size context (edited on the Size & Spacing page).
    readonly property real iconRatio: Math.max(0.25, Plasmoid.configuration.panelIconRatio || 0.65)
    readonly property real usable: Math.max(0, (Plasmoid.dockCrossExtent || 0)
        - Plasmoid.configuration.edgePadding - Plasmoid.configuration.topMargin)
    readonly property real zoomFactor: Math.max(1.0, zoomSlider.value / iconRatio)
    readonly property int restPx: Math.round(iconRatio * usable)
    readonly property int zoomPx: Math.round(Math.max(iconRatio, zoomSlider.value) * usable)
    readonly property var curveDescriptions: [
        i18n("A smooth, rounded wave. This is how the macOS dock feels."),
        i18n("A broader top that keeps neighbouring icons large for longer."),
        i18n("A sharper peak that makes the icon under the pointer stand out.")
    ]

    Kirigami.FormLayout {
        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Magnification")
        }

        QQC2.CheckBox {
            id: magnifyCheck
            Kirigami.FormData.label: i18n("Magnify:")
            text: i18n("Grow icons as the pointer moves over them")
        }

        ConfigSlider {
            id: zoomSlider
            enabled: magnifyCheck.checked
            Kirigami.FormData.label: i18n("Magnified size:")
            from: 0.30
            to: 1.0
            stepSize: 0.05
            format: function(v) { return i18n("%1×", page.zoomFactor.toFixed(2)); }
        }

        ConfigHint {
            text: page.usable > 0
                ? i18n("Icons grow from %1 px to %2 px. They can never grow taller than the panel, so a taller panel or a smaller icon size allows more magnification.", page.restPx, page.zoomPx)
                : i18n("How large the icon under the pointer gets. It can never grow taller than the panel.")
        }

        ConfigSlider {
            id: reachSlider
            enabled: magnifyCheck.checked
            Kirigami.FormData.label: i18n("Wave width:")
            from: 1.0
            to: 4.0
            stepSize: 0.1
            format: function(v) { return i18np("%1 icon", "%1 icons", v.toFixed(1)); }
        }

        ConfigHint {
            text: i18n("How far the magnification spreads to the icons on either side.")
        }

        QQC2.ComboBox {
            id: curveCombo
            enabled: magnifyCheck.checked
            Kirigami.FormData.label: i18n("Wave shape:")
            model: [
                i18n("Classic"),
                i18n("Broad"),
                i18n("Peaked")
            ]
        }

        ConfigHint {
            text: page.curveDescriptions[Math.max(0, curveCombo.currentIndex)]
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Bounce")
        }

        QQC2.CheckBox {
            id: bounceCheck
            Kirigami.FormData.label: i18n("Bounce:")
            text: i18n("Bounce icons while apps open or need attention")
        }

        ConfigSlider {
            id: bounceSlider
            enabled: bounceCheck.checked
            Kirigami.FormData.label: i18n("Bounce height:")
            from: 0.2
            to: 0.9
            stepSize: 0.05
        }

        ConfigHint {
            text: i18n("Measured as a share of the icon size. If the panel is too short, bounces are lowered automatically instead of being cut off. Size & Spacing shows a panel height with room for the full bounce.")
        }
    }
}
