// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Effects
import QtQuick.Window

/*!
    LiquidGlass — a rounded-rect glass surface rendered by a single fragment shader.

    Two operating modes:
      * Overlay mode (default, `backdrop` is null): draws tint, lens shading, specular rim,
        borders and shadow on top of whatever the compositor blurred behind the window
        (e.g. KWin blur-behind). Use this for panels, docks and translucent windows.
      * Backdrop mode (`backdrop` set to an Item rendered *behind* this one in the same
        window): additionally samples that item with real lens refraction, chromatic
        dispersion, blur ("frost") and saturation — true in-scene liquid glass.

    Quick start:
        LiquidGlass { anchors.fill: parent; radius: 24; material: "liquid"; dark: true }

    Every property defaults to the selected `material` recipe (see GlassMaterials) and can
    be overridden individually.
*/
Item {
    id: root

    // ---- Recipe -----------------------------------------------------------
    property string material: "liquid"
    property bool dark: true
    readonly property var recipe: GlassMaterials.material(material, dark)

    // ---- Shape ------------------------------------------------------------
    property real radius: 24

    // ---- Body -------------------------------------------------------------
    property color tint: recipe.tint
    property real tintAlpha: recipe.tintAlpha
    property real bezel: recipe.bezel
    property real refraction: recipe.refraction
    /// Whole-body magnification (thick slab), backdrop mode only.
    property real magnify: recipe.magnify ?? 0
    /// Extra steep bend right at the rim ("crystal" edge), backdrop mode only.
    property real edgeBend: recipe.edgeBend ?? 0
    /// Master strength 0..2 scaling refraction, magnification, dispersion and highlights.
    property real intensity: 1
    property real dispersion: recipe.dispersion
    property real frost: recipe.frost
    property real saturation: recipe.saturation
    property real brightness: recipe.brightness
    property real edgeShade: recipe.edgeShade
    property real grain: recipe.grain

    // ---- Lighting ---------------------------------------------------------
    property real highlight: recipe.highlight
    property real highlightWidth: recipe.highlightWidth
    property real sheen: recipe.sheen
    /// Direction towards the light in degrees (screen space, -90 = straight up).
    property real lightAngle: -128

    // ---- Interaction ------------------------------------------------------
    /// Pointer position in this item's coordinates.
    property point pointer: Qt.point(width / 2, height / 2)
    /// 0..1 presence of the pointer; animate it in/out (0 = static glass, zero cost).
    property real pointerStrength: 0
    /// How strongly lighting follows the pointer (light swing + spot).
    property real pointerLight: recipe.pointerLight

    // ---- Borders ----------------------------------------------------------
    property color innerBorderColor: recipe.innerBorderColor
    property color outerBorderColor: recipe.outerBorderColor
    property real borderWidth: recipe.borderWidth

    // ---- Shadow -----------------------------------------------------------
    property color shadowColor: recipe.shadowColor
    property real shadowBlur: recipe.shadowBlur
    property real shadowOffsetX: 0
    property real shadowOffsetY: recipe.shadowOffsetY

    // ---- Backdrop mode ----------------------------------------------------
    /// Item drawn behind the glass to refract. Must live in the same window.
    property Item backdrop: null
    readonly property bool hasBackdrop: backdrop !== null
    /// Bump to force re-mapping after the glass or backdrop moved via a parent.
    property int backdropRevision: 0

    function v4(c, alphaScale) {
        return Qt.vector4d(c.r, c.g, c.b, c.a * (alphaScale === undefined ? 1.0 : alphaScale));
    }

    readonly property int _margin: Math.ceil(refraction * intensity * (1 + dispersion) + 4 + frost * 32)
    readonly property rect _backdropRect: {
        void(root.backdropRevision);
        void(root.x); void(root.y); void(root.width); void(root.height);
        if (!root.backdrop) {
            return Qt.rect(0, 0, 1, 1);
        }
        void(root.backdrop.width); void(root.backdrop.height);
        const pt = root.mapToItem(root.backdrop, 0, 0);
        return Qt.rect(pt.x - _margin, pt.y - _margin, root.width + 2 * _margin, root.height + 2 * _margin);
    }

    Rectangle {
        id: dummyItem
        width: 1
        height: 1
        color: "transparent"
        visible: false
    }

    ShaderEffectSource {
        id: dummySource
        sourceItem: dummyItem
        width: 1
        height: 1
        visible: false
        live: false
    }

    ShaderEffectSource {
        id: backdropSource
        visible: false
        sourceItem: root.backdrop
        sourceRect: root._backdropRect
        width: root._backdropRect.width
        height: root._backdropRect.height
        live: root.hasBackdrop
        smooth: true
    }

    MultiEffect {
        id: blurredBackdrop
        visible: false
        width: backdropSource.width
        height: backdropSource.height
        source: backdropSource
        autoPaddingEnabled: false
        blurEnabled: root.hasBackdrop && root.frost > 0.001
        blur: Math.min(1.0, root.frost)
        blurMax: 48
        blurMultiplier: 0.4
        layer.enabled: root.hasBackdrop
    }

    ShaderEffect {
        id: fx

        readonly property real ext: Math.ceil(Math.max(
            root.shadowColor.a > 0.001 ? root.shadowBlur + Math.max(Math.abs(root.shadowOffsetX), Math.abs(root.shadowOffsetY)) : 0,
            root.borderWidth + 2))

        x: -ext
        y: -ext
        width: root.width + 2 * ext
        height: root.height + 2 * ext
        visible: root.width > 0 && root.height > 0
        blending: true

        property size itemSize: Qt.size(width, height)
        property rect glassRect: Qt.rect(ext, ext, root.width, root.height)
        property real radius: root.radius
        property vector4d tint: Qt.vector4d(root.tint.r, root.tint.g, root.tint.b, Math.max(0, Math.min(1, root.tintAlpha)))
        property real bezel: root.bezel
        property real refraction: root.hasBackdrop ? root.refraction * root.intensity : 0
        property real dispersion: Math.min(1, root.dispersion * root.intensity)
        property real magnify: root.hasBackdrop ? Math.min(0.3, root.magnify * root.intensity) : 0
        property real edgeBend: root.edgeBend
        property real saturation: root.saturation
        property real brightness: root.brightness
        property real hasSource: root.hasBackdrop ? 1.0 : 0.0
        property real sourceMargin: root._margin
        property real rimLight: root.highlight * Math.sqrt(root.intensity)
        property real rimWidth: root.highlightWidth
        property point lightDir: Qt.point(Math.cos(root.lightAngle * Math.PI / 180), Math.sin(root.lightAngle * Math.PI / 180))
        property real edgeShade: root.edgeShade
        property real sheen: root.sheen
        property vector4d innerBorder: root.v4(root.innerBorderColor)
        property vector4d outerBorder: root.v4(root.outerBorderColor)
        property real borderWidth: root.borderWidth
        property real grain: root.grain
        property vector4d shadowColor: root.v4(root.shadowColor)
        property real shadowBlur: root.shadowBlur
        property point shadowOffset: Qt.point(root.shadowOffsetX, root.shadowOffsetY)
        property real dpr: root.Window.window ? root.Window.window.devicePixelRatio : Screen.devicePixelRatio
        property point pointerPos: root.pointer
        property real pointerStrength: root.pointerStrength
        property real pointerLight: root.pointerLight
        property var source: !root.hasBackdrop ? dummySource
            : (blurredBackdrop.blurEnabled ? blurredBackdrop : backdropSource)

        fragmentShader: Qt.resolvedUrl("shaders/liquidglass.frag.qsb")
    }
}
