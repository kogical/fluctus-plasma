import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.plasma.plasmoid
import Kogical.LiquidGlass

KCM.SimpleKCM {
    id: page

    property alias cfg_hidePanelBackground: hidePanelBgSwitch.checked
    property alias cfg_backgroundMode: surfaceCombo.currentIndex
    property alias cfg_blurStyle: materialCombo.currentIndex
    property alias cfg_blurEnabled: blurCheck.checked
    property alias cfg_glassInteractive: interactiveCheck.checked
    property alias cfg_pillColorMode: tintCombo.currentIndex
    property color cfg_pillCustomColor
    property alias cfg_pillOpacity: opacitySlider.value
    property alias cfg_pillCornerRadius: radiusSlider.value
    property alias cfg_pillBorderStyle: outlineCombo.currentIndex
    property alias cfg_pillBorderWidth: outlineWidthSpin.value
    property alias cfg_pillBorderOpacity: outlineOpacitySlider.value
    property alias cfg_pillShadow: shadowCheck.checked
    property alias cfg_pillShadowStrength: shadowSlider.value
    property alias cfg_glassHighlight: highlightSlider.value
    property alias cfg_glassIntensity: intensitySlider.value
    property alias cfg_glassThickness: thicknessSlider.value
    property alias cfg_glassFrost: frostSlider.value
    property alias cfg_glassDispersion: dispersionSlider.value
    property alias cfg_showReflection: reflectionCheck.checked
    property alias cfg_indicatorStyle: indicatorStyleCombo.currentIndex
    property alias cfg_indicatorColorMode: indicatorColorCombo.currentIndex
    property alias cfg_indicatorPosition: indicatorPositionCombo.currentIndex
    property string cfg_activePreset

    readonly property bool hasSurface: surfaceCombo.currentIndex === 1 || surfaceCombo.currentIndex === 2
    readonly property bool isGlass: surfaceCombo.currentIndex === 1
    readonly property bool isTranslucentGlass: isGlass && materialCombo.currentIndex !== 4
    // Liquid Glass and Crystal need a compositor glass effect (Fluctus Glass, or kwin-effects-glass);
    // without one the dock renders Frosted instead.
    readonly property bool glassEffectActive: Plasmoid.glassEffectState === 2
    readonly property bool fluctusGlassActive: glassEffectActive && Plasmoid.glassProvider === "fluctusglass"
    function needsGlassEffect(index) {
        return index === 1 || index === 5;
    }
    readonly property bool liquidSelectedButUnavailable: isGlass && needsGlassEffect(materialCombo.currentIndex) && !glassEffectActive
    property int lastValidMaterial: 0
    readonly property bool themeIsDark: {
        const c = Kirigami.Theme.backgroundColor;
        return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b < 0.5;
    }
    readonly property var materialIds: ["frosted", "liquid", "acrylic", "clear", "solid", "crystal"]
    readonly property var materialDescriptions: [
        i18n("Dense, softly blurred glass with a crisp outline. The look of macOS before Liquid Glass."),
        i18n("Clear glass whose curved edge bends what is behind it, with a bright rim of light, like macOS Tahoe."),
        i18n("Thick tinted glass with a fine grain texture."),
        i18n("A barely-there surface. The blur behind it does most of the work."),
        i18n("An opaque surface with no blur."),
        i18n("Perfectly clear, deeper glass with a sharper bend at the edge and stronger rainbow fringes.")
    ]
    readonly property var surfaceDescriptions: [
        i18n("The dock draws nothing of its own. Your Plasma panel, theme or Panel Colorizer provides the background."),
        i18n("A glass surface that hugs the icons and stretches as they magnify."),
        i18n("Your Plasma theme's panel artwork, sized to the icons and blurred."),
        i18n("Just the icons, floating over the wallpaper.")
    ]

    // Blur strength is KWin's own setting, not a widget setting, so the page tracks and saves it
    // itself. Plasma's settings window reads unsavedChanges to enable Apply and calls saveConfig().
    readonly property int savedBlurStrength: Plasmoid.systemBlurStrength || 10
    readonly property bool unsavedChanges: Math.round(blurStrengthSlider.value) !== savedBlurStrength

    function saveConfig() {
        if (unsavedChanges) {
            Plasmoid.setSystemBlurStrength(Math.round(blurStrengthSlider.value));
        }
    }

    // Editing the look by hand means the dock no longer matches a named preset.
    function markCustom() {
        cfg_activePreset = "";
    }

    ColorDialog {
        id: colorDialog
        title: i18n("Dock Color")
        selectedColor: page.cfg_pillCustomColor
        onAccepted: {
            page.cfg_pillCustomColor = selectedColor;
            page.markCustom();
        }
    }

    header: ColumnLayout {
        spacing: 0

        DockPreview {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 8
            backgroundMode: surfaceCombo.currentIndex
            blurStyle: materialCombo.currentIndex
            pillColorMode: tintCombo.currentIndex
            pillCustomColor: page.cfg_pillCustomColor
            pillOpacity: opacitySlider.value
            pillCornerRadius: radiusSlider.value
            pillBorderStyle: outlineCombo.currentIndex
            pillBorderWidth: outlineWidthSpin.value
            pillBorderOpacity: outlineOpacitySlider.value
            pillShadow: shadowCheck.checked
            pillShadowStrength: shadowSlider.value
            glassHighlight: highlightSlider.value
            glassIntensity: intensitySlider.value
            glassThickness: thicknessSlider.value
            glassFrost: frostSlider.value
            glassDispersion: dispersionSlider.value
            nativeGlass: page.fluctusGlassActive
            showReflection: reflectionCheck.checked
            indicatorStyle: indicatorStyleCombo.currentIndex
            indicatorColorMode: indicatorColorCombo.currentIndex
            indicatorPosition: indicatorPositionCombo.currentIndex
            hidePanelBackground: hidePanelBgSwitch.checked
            blurEnabled: blurCheck.checked
            blurStrength: blurStrengthSlider.value
            compositorRefraction: page.glassEffectActive
            interactive: interactiveCheck.checked
            // Size settings live on their own page; preview them as saved.
            pillPaddingPrimary: Plasmoid.configuration.pillPaddingPrimary
            pillPaddingCross: Plasmoid.configuration.pillPaddingCross
            iconSpacingRatio: Plasmoid.configuration.iconSpacingRatio
            edgePadding: Plasmoid.configuration.edgePadding
            wallpaper: lightWallpaperSwitch.checked ? 1 : 0
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing

            QQC2.Label {
                Layout.fillWidth: true
                text: i18n("Preview on a sample wallpaper. Point at it to try the glass.")
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
            Kirigami.FormData.label: i18n("Panel")
        }

        QQC2.Switch {
            id: hidePanelBgSwitch
            Kirigami.FormData.label: i18n("Panel background:")
            text: i18n("Hide the panel behind the dock")
            enabled: !Plasmoid.panelBackgroundManagedExternally
            onToggled: page.markCustom()
        }

        ConfigHint {
            text: i18n("Removes the panel's frame, blur and shadow so only the dock is visible. The panel looks normal again when you turn this off or remove the widget.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: Plasmoid.panelBackgroundManagedExternally
            type: Kirigami.MessageType.Information
            text: i18n("Panel Colorizer is styling this panel, so Fluctus leaves its background alone. Use Panel Colorizer's background options instead.")
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Dock Background")
        }

        QQC2.ComboBox {
            id: surfaceCombo
            Kirigami.FormData.label: i18n("Background:")
            model: [
                i18n("None (use the panel)"),
                i18n("Glass"),
                i18n("Plasma theme"),
                i18n("Icons only")
            ]
            onActivated: page.markCustom()
        }

        ConfigHint {
            text: page.surfaceDescriptions[Math.max(0, surfaceCombo.currentIndex)]
        }

        QQC2.ComboBox {
            id: materialCombo
            visible: page.isGlass
            Kirigami.FormData.label: i18n("Material:")
            model: [
                i18n("Frosted"),
                i18n("Liquid Glass"),
                i18n("Acrylic"),
                i18n("Clear"),
                i18n("Solid"),
                i18n("Crystal")
            ]
            delegate: QQC2.ItemDelegate {
                required property int index
                required property string modelData
                readonly property bool locked: page.needsGlassEffect(index) && !page.glassEffectActive
                width: ListView.view ? ListView.view.width : implicitWidth
                text: locked ? i18n("%1 (needs Fluctus Glass)", modelData) : modelData
                enabled: !locked
                highlighted: materialCombo.highlightedIndex === index
            }
            Component.onCompleted: page.lastValidMaterial = currentIndex
            onActivated: {
                // Keyboard or wheel can still land on the locked entry; step back from it.
                if (page.needsGlassEffect(currentIndex) && !page.glassEffectActive) {
                    currentIndex = page.needsGlassEffect(page.lastValidMaterial) ? 0 : page.lastValidMaterial;
                    return;
                }
                page.lastValidMaterial = currentIndex;
                // Each material starts from its own tuned opacity.
                const recipe = GlassMaterials.material(page.materialIds[currentIndex], page.themeIsDark);
                opacitySlider.value = Math.round(recipe.tintAlpha * 50) / 50;
                page.markCustom();
            }
        }

        ConfigHint {
            visible: page.isGlass
            text: page.materialDescriptions[Math.max(0, materialCombo.currentIndex)]
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.liquidSelectedButUnavailable
            type: Kirigami.MessageType.Warning
            text: i18n("This material needs a glass effect in KWin, which isn't running, so the dock shows Frosted for now.")
        }

        GlassEffectNotice {
            visible: page.isTranslucentGlass && effectState !== 2
        }

        QQC2.CheckBox {
            id: blurCheck
            visible: page.isTranslucentGlass || surfaceCombo.currentIndex === 2
            Kirigami.FormData.label: i18n("Blur:")
            text: i18n("Blur the wallpaper behind the dock")
            onToggled: page.markCustom()
        }

        ConfigSlider {
            id: blurStrengthSlider
            // With a glass effect, KWin's stock blur strength does not apply to the dock.
            visible: blurCheck.visible && blurCheck.checked && !page.glassEffectActive
            Kirigami.FormData.label: i18n("Blur strength:")
            from: 1
            to: 15
            stepSize: 1
            value: Plasmoid.systemBlurStrength || 10
            format: function(v) {
                return v <= 4 ? i18n("Light") : (v <= 10 ? i18n("Medium") : i18n("Strong"));
            }
        }

        ConfigHint {
            visible: blurCheck.visible
            text: !blurCheck.checked
                ? i18n("Without blur the wallpaper shows through sharply, tinted by the glass.")
                : (page.fluctusGlassActive
                    ? i18n("Fluctus Glass draws the dock itself, so its look is tuned with the Glass sliders below and affects only the dock.")
                    : page.glassEffectActive
                    ? i18n("The Glass effect is running, so blur and refraction strength are set in its own settings under Desktop Effects. They apply to every blurred surface, not just the dock.")
                    : i18n("Blur strength is shared by every blurred surface on your desktop, not just the dock. Lower it to see more of the wallpaper through the dock, or turn blur off for completely clear glass."))
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            visible: page.isGlass
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Color")
        }

        QQC2.ComboBox {
            id: tintCombo
            visible: page.isGlass
            Kirigami.FormData.label: i18n("Tint:")
            model: [
                i18n("Match Plasma theme"),
                i18n("Custom color"),
                i18n("Accent color")
            ]
            onActivated: page.markCustom()
        }

        RowLayout {
            visible: page.isGlass && tintCombo.currentIndex === 1
            Kirigami.FormData.label: i18n("Color:")
            spacing: Kirigami.Units.smallSpacing

            Rectangle {
                implicitWidth: Kirigami.Units.gridUnit * 2
                implicitHeight: Kirigami.Units.gridUnit * 1.4
                radius: Kirigami.Units.cornerRadius
                color: page.cfg_pillCustomColor
                border.width: 1
                border.color: Kirigami.Theme.disabledTextColor
            }
            QQC2.Button {
                text: i18n("Choose…")
                icon.name: "color-picker"
                onClicked: colorDialog.open()
            }
        }

        ConfigSlider {
            id: opacitySlider
            visible: page.isGlass
            Kirigami.FormData.label: i18n("Opacity:")
            from: 0
            to: 1
            stepSize: 0.02
            onMoved: page.markCustom()
        }

        ConfigHint {
            visible: page.isTranslucentGlass
            text: i18n("How much of the tint covers the blurred wallpaper. Low values look more like clear glass.")
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            visible: page.isGlass
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Shape and Edges")
        }

        ConfigSlider {
            id: radiusSlider
            visible: page.isGlass
            Kirigami.FormData.label: i18n("Corner roundness:")
            from: 0
            to: 48
            stepSize: 1
            format: function(v) { return v >= 48 ? i18n("Pill") : i18n("%1 px", Math.round(v)); }
            onMoved: page.markCustom()
        }

        QQC2.ComboBox {
            id: outlineCombo
            visible: page.isGlass
            Kirigami.FormData.label: i18n("Outline:")
            model: [
                i18n("Material default"),
                i18n("Single line"),
                i18n("Two-tone hairline"),
                i18n("Accent glow"),
                i18n("None")
            ]
            onActivated: page.markCustom()
        }

        ConfigHint {
            visible: page.isGlass && outlineCombo.currentIndex === 2
            text: i18n("A light line on the inside and a dark one on the outside, which keeps the edge crisp on any wallpaper.")
        }

        QQC2.SpinBox {
            id: outlineWidthSpin
            visible: page.isGlass && outlineCombo.currentIndex > 0 && outlineCombo.currentIndex < 4
            Kirigami.FormData.label: i18n("Outline width:")
            from: 0
            to: 6
            textFromValue: function(value) { return i18n("%1 px", value); }
            valueFromText: function(text) { return parseInt(text, 10) || 0; }
            onValueModified: page.markCustom()
        }

        ConfigSlider {
            id: outlineOpacitySlider
            visible: outlineWidthSpin.visible && outlineWidthSpin.value > 0
            Kirigami.FormData.label: i18n("Outline strength:")
            from: 0
            to: 1
            stepSize: 0.02
            onMoved: page.markCustom()
        }

        ConfigSlider {
            id: intensitySlider
            visible: page.isTranslucentGlass
            Kirigami.FormData.label: i18n("Glass strength:")
            from: 0
            to: 2
            stepSize: 0.05
            onMoved: page.markCustom()
        }

        ConfigSlider {
            id: thicknessSlider
            visible: page.isTranslucentGlass
            Kirigami.FormData.label: i18n("Glass thickness:")
            from: 0.25
            to: 2
            stepSize: 0.05
            onMoved: page.markCustom()
        }

        ConfigSlider {
            id: frostSlider
            visible: page.isTranslucentGlass
            Kirigami.FormData.label: i18n("Frost:")
            from: 0
            to: 3
            stepSize: 0.05
            onMoved: page.markCustom()
        }

        ConfigSlider {
            id: dispersionSlider
            visible: page.isTranslucentGlass
            Kirigami.FormData.label: i18n("Color fringing:")
            from: 0
            to: 2
            stepSize: 0.05
            onMoved: page.markCustom()
        }

        ConfigHint {
            visible: page.isTranslucentGlass
            text: page.fluctusGlassActive
                ? i18n("Strength scales how far the edge bends what is behind it; thickness is how wide the curved edge is. 100% is the material's natural look.")
                : i18n("These shape the refraction drawn by Fluctus Glass. Without it, only the preview and the in-dock overlay follow them.")
        }

        ConfigSlider {
            id: highlightSlider
            visible: page.isTranslucentGlass
            Kirigami.FormData.label: i18n("Edge highlight:")
            from: 0
            to: 2
            stepSize: 0.05
            onMoved: page.markCustom()
        }

        ConfigHint {
            visible: page.isTranslucentGlass
            text: i18n("The light that catches the rim of the glass. 100% is the material's natural look.")
        }

        QQC2.CheckBox {
            id: interactiveCheck
            visible: page.isTranslucentGlass
            Kirigami.FormData.label: i18n("Pointer effects:")
            text: i18n("Glass reacts to the pointer")
            onToggled: page.markCustom()
        }

        ConfigHint {
            visible: page.isTranslucentGlass
            text: i18n("Light follows the pointer along the glass and a soft spot glows under it. Nothing runs while the pointer is away from the dock.")
        }

        QQC2.CheckBox {
            id: shadowCheck
            visible: page.isGlass
            Kirigami.FormData.label: i18n("Shadow:")
            text: i18n("Drop a soft shadow under the dock")
            onToggled: page.markCustom()
        }

        ConfigSlider {
            id: shadowSlider
            visible: page.isGlass && shadowCheck.checked
            Kirigami.FormData.label: i18n("Shadow strength:")
            from: 0
            to: 2
            stepSize: 0.05
            onMoved: page.markCustom()
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Running Apps")
        }

        QQC2.ComboBox {
            id: indicatorStyleCombo
            Kirigami.FormData.label: i18n("Indicator:")
            model: [
                i18n("Pill"),
                i18n("Dot"),
                i18n("Glowing line"),
                i18n("Thin line"),
                i18n("None")
            ]
            onActivated: page.markCustom()
        }

        QQC2.ComboBox {
            id: indicatorColorCombo
            visible: indicatorStyleCombo.currentIndex !== 4
            Kirigami.FormData.label: i18n("Indicator color:")
            model: [
                i18n("Accent for the active app"),
                i18n("Neutral"),
                i18n("Accent for all apps")
            ]
            onActivated: page.markCustom()
        }

        QQC2.ComboBox {
            id: indicatorPositionCombo
            visible: indicatorStyleCombo.currentIndex !== 4
            Kirigami.FormData.label: i18n("Indicator position:")
            model: [
                i18n("Below the icon"),
                i18n("On the icon")
            ]
            onActivated: page.markCustom()
        }

        QQC2.CheckBox {
            id: reflectionCheck
            Kirigami.FormData.label: i18n("Reflection:")
            text: i18n("Show a faint mirror image under each icon")
            onToggled: page.markCustom()
        }

        ConfigHint {
            text: i18n("Only shown when the dock is at the bottom of the screen.")
        }
    }
}
