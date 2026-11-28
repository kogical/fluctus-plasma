import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

// Invariant: NEVER bind x, y, width, height, or scale here.
// DockLayoutItem owns delegate geometry and drives position + scale directly from C++.
Item {
    id: delegateRoot

    // Properties populated and kept in sync by C++ DockLayoutItem
    property int taskIndex: -1
    property var modelIndex
    property string displayTitle: ""
    property string appName: ""
    property int appPid: 0
    property var decorationIcon
    property bool isActive: false
    property bool isLauncher: false
    property bool isWindow: false
    property bool isStartup: false
    property bool isDemandingAttention: false
    property bool isMinimized: false
    property bool isMaximized: false
    property bool isFullScreen: false
    property bool isKeepAbove: false
    property bool isKeepBelow: false
    property bool isClosable: true
    property bool isMinimizable: true
    property bool isMaximizable: true
    property bool isMovable: true
    property bool isResizable: true
    property bool isFullScreenable: true
    property bool isOnAllVirtualDesktops: false
    property var virtualDesktops: []
    property var winIdList: []
    property bool isGroupParent: false
    property int childCount: 0
    property string appId: ""
    property url launcherUrl
    property url launcherUrlFull
    property int dockEdge: 0
    property real delegateLogicalSize: 87
    property real slotScale: 1.0
    property real bounceOffset: 0.0

    // Properties passed from main.qml
    property var rootRef
    property var tasksModelRef
    property var dockLayoutRef
    property var contextMenuRef
    property var groupDialogRef
    property var audioTrackerRef
    property bool showIndexBadge: false
    property bool minimizeActiveOnClick: true
    property bool enableMipmap: false
    property bool showReflection: false
    property bool showTooltips: true
    property bool showWindowPreviews: true
    property bool indicateAudioStreams: true
    property bool dragOffToUnpin: true
    property int indicatorStyle: 0
    property int indicatorPosition: 0
    // 0: Theme (accent when active), 1: Monochrome, 2: Accent
    property int indicatorColorMode: 0
    // Logical px of free space between the resting icon and the dock's outer edge
    property real indicatorOutset: 0
    property int smartLauncherRevision: 0

    readonly property bool isHorizontalDock: dockEdge === 0 || dockEdge === 1
    readonly property int iconMargin: delegateRoot.delegateLogicalSize >= 48 ? 2 : 1
    readonly property real effectiveBaseSize: dockLayoutRef
        ? Math.max(16, dockLayoutRef.effectiveBaseIconSize)
        : Math.max(16, delegateRoot.delegateLogicalSize)
    readonly property real restingScale: Math.max(0.01, effectiveBaseSize / Math.max(1, delegateRoot.delegateLogicalSize))
    readonly property int restingIconSize: Math.max(16, Math.round(effectiveBaseSize) - 2 * iconMargin)
    readonly property bool isMagnifying: delegateRoot.slotScale > 1.001
    readonly property bool isPinned: launcherUrl.toString().length > 0 && tasksModelRef
        ? tasksModelRef.launcherPosition(launcherUrl) >= 0
        : false

    // SmartLauncher live badge / progress state from C++ singleton
    readonly property var smartState: {
        void(smartLauncherRevision);
        if (!Plasmoid || typeof Plasmoid.smartLauncherState !== "function") {
            return {};
        }
        return Plasmoid.smartLauncherState(delegateRoot.appId, delegateRoot.launcherUrl);
    }
    readonly property bool smartCountVisible: Boolean(smartState && smartState.countVisible && smartState.count > 0)
    readonly property int smartCount: smartState && smartState.count ? smartState.count : 0
    readonly property bool smartProgressVisible: Boolean(smartState && smartState.progressVisible)
    readonly property real smartProgress: smartState && smartState.progress ? smartState.progress : 0.0

    // PulseAudio stream state from AudioStreamTracker
    readonly property var audioState: {
        if (!indicateAudioStreams || !audioTrackerRef || (!isWindow && !isGroupParent)) {
            return { hasAudio: false, playing: false, muted: false };
        }
        void(audioTrackerRef.revision);
        return audioTrackerRef.audioStateForTask(delegateRoot.appPid, delegateRoot.appName, delegateRoot.appId);
    }
    readonly property bool hasAudioStream: Boolean(audioState && audioState.hasAudio)
    readonly property bool isAudioMuted: Boolean(audioState && audioState.muted)

    // Optional macOS-style subtle icon reflection (bottom edge only)
    Item {
        id: reflectionHolder
        visible: delegateRoot.showReflection && delegateRoot.dockEdge === 0
        anchors.left: iconHolder.left
        anchors.right: iconHolder.right
        anchors.top: iconHolder.bottom
        height: iconHolder.height * 0.38
        clip: true
        opacity: 0.24

        Kirigami.Icon {
            width: iconHolder.width
            height: iconHolder.height
            source: delegateRoot.decorationIcon
            roundToIconSize: false
            smooth: true
            transform: Scale {
                origin.y: iconHolder.height * 0.5
                yScale: -1
            }
        }
    }

    Item {
        id: iconHolder
        anchors.fill: parent
        anchors.margins: delegateRoot.iconMargin
        opacity: mouseArea.draggedOutsideToUnpin ? 0.32 : (mouseArea.isDragging ? 0.72 : 1.0)

        Behavior on opacity { NumberAnimation { duration: 110 } }

        // Crisp 1:1 resting icon: Lanczos-3 downsampled from high-resolution QIcon/SVG artwork
        // directly to the exact physical resting size, counter-scaling delegateRoot's restingScale
        // so the combined GPU transform is identically 1.0 with nearest-texel 1:1 sampling.
        DockIconItem {
            id: restingIcon
            visible: !delegateRoot.isMagnifying
            width: delegateRoot.restingIconSize
            height: delegateRoot.restingIconSize
            transformOrigin: Item.TopLeft
            scale: 1.0 / delegateRoot.restingScale
            x: delegateRoot.dockEdge === 3
                ? (delegateRoot.width - (delegateRoot.iconMargin + width) / delegateRoot.restingScale) - delegateRoot.iconMargin
                : (delegateRoot.iconMargin / delegateRoot.restingScale) - delegateRoot.iconMargin
            y: delegateRoot.dockEdge === 0
                ? (delegateRoot.height - (delegateRoot.iconMargin + height) / delegateRoot.restingScale) - delegateRoot.iconMargin
                : (delegateRoot.iconMargin / delegateRoot.restingScale) - delegateRoot.iconMargin
            source: delegateRoot.decorationIcon
            active: mouseArea.containsMouse
            exactPixelSampling: true
        }

        // Full-resolution magnified icon shown while hovering/zooming (Lanczos-3 at peak zoom).
        DockIconItem {
            id: zoomIcon
            visible: delegateRoot.isMagnifying
            anchors.fill: parent
            source: delegateRoot.decorationIcon
            active: mouseArea.containsMouse
            exactPixelSampling: false
        }
    }

    // Drag-off-to-unpin visual badge cue
    Rectangle {
        visible: mouseArea.draggedOutsideToUnpin
        anchors.centerIn: parent
        width: unpinLabel.implicitWidth + 14
        height: 22
        radius: 11
        color: Kirigami.Theme.negativeTextColor
        z: 20

        Text {
            id: unpinLabel
            anchors.centerIn: parent
            text: i18n("Unpin")
            color: "#ffffff"
            font.pixelSize: 11
            font.bold: true
        }
    }

    // Native Plasma tooltip with PipeWire live window preview + MPRIS controls
    PlasmaCore.ToolTipArea {
        id: toolTipArea
        anchors.fill: parent
        active: delegateRoot.showTooltips && !mouseArea.isDragging
        interactive: delegateRoot.isWindow || delegateRoot.isGroupParent
        location: Plasmoid.location
        mainItem: ToolTipDelegate {
            tasksModelRef: delegateRoot.tasksModelRef
            modelIndex: delegateRoot.modelIndex
            displayTitle: delegateRoot.displayTitle
            appName: delegateRoot.appName
            decorationIcon: delegateRoot.decorationIcon
            isWindow: delegateRoot.isWindow
            isGroupParent: delegateRoot.isGroupParent
            childCount: delegateRoot.childCount
            winIdList: delegateRoot.winIdList
            appId: delegateRoot.appId
            showPreview: delegateRoot.showWindowPreviews
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton

            property point pressPos: Qt.point(0, 0)
            property bool isDragging: false
            property bool movedDuringDrag: false
            property bool draggedOutsideToUnpin: false
            property bool suppressNextClick: false

            function freshModelIndex() {
                if (!delegateRoot.tasksModelRef || delegateRoot.taskIndex < 0
                        || delegateRoot.taskIndex >= delegateRoot.tasksModelRef.count) {
                    return null;
                }
                return delegateRoot.tasksModelRef.makeModelIndex(delegateRoot.taskIndex);
            }

            function checkDraggedFarOutside(ptInDock) {
                if (!delegateRoot.dragOffToUnpin || !delegateRoot.isPinned || !delegateRoot.dockLayoutRef) {
                    return false;
                }
                const pill = delegateRoot.dockLayoutRef.pillRect;
                const threshold = Math.max(76, (delegateRoot.isHorizontalDock ? pill.height : pill.width) * 1.35);
                switch (delegateRoot.dockEdge) {
                case 0: // Bottom
                    return ptInDock.y < (pill.y - threshold);
                case 1: // Top
                    return ptInDock.y > (pill.y + pill.height + threshold);
                case 2: // Left
                    return ptInDock.x > (pill.x + pill.width + threshold);
                case 3: // Right
                    return ptInDock.x < (pill.x - threshold);
                default:
                    return false;
                }
            }

            onPressed: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    pressPos = Qt.point(mouse.x, mouse.y);
                    isDragging = false;
                    movedDuringDrag = false;
                    draggedOutsideToUnpin = false;
                    suppressNextClick = false;
                } else if (mouse.button === Qt.RightButton) {
                    mouse.accepted = true;
                    if (delegateRoot.contextMenuRef) {
                        delegateRoot.contextMenuRef.openForDelegate(delegateRoot);
                    }
                }
            }

            onPositionChanged: function(mouse) {
                if (!(mouse.buttons & Qt.LeftButton) || !delegateRoot.dockLayoutRef || !delegateRoot.tasksModelRef) {
                    return;
                }
                const dx = mouse.x - pressPos.x;
                const dy = mouse.y - pressPos.y;
                if (!isDragging && Math.hypot(dx, dy) >= Qt.styleHints.startDragDistance) {
                    isDragging = true;
                    toolTipArea.hideToolTip();
                }
                if (isDragging) {
                    const pt = delegateRoot.mapToItem(delegateRoot.dockLayoutRef, mouse.x, mouse.y);
                    draggedOutsideToUnpin = checkDraggedFarOutside(pt);

                    if (draggedOutsideToUnpin) {
                        if (delegateRoot.rootRef) {
                            delegateRoot.rootRef.activeDropIndex = -1;
                        }
                        return;
                    }

                    delegateRoot.dockLayoutRef.updatePointer(pt.x, pt.y);
                    const targetIdx = delegateRoot.dockLayoutRef.restingDragTargetIndex(
                        pt.x,
                        pt.y,
                        delegateRoot.taskIndex
                    );
                    if (delegateRoot.rootRef) {
                        delegateRoot.rootRef.activeDropIndex = targetIdx;
                    }
                    if (delegateRoot.taskIndex >= 0 && targetIdx !== delegateRoot.taskIndex) {
                        delegateRoot.tasksModelRef.move(delegateRoot.taskIndex, targetIdx);
                        movedDuringDrag = true;
                    }
                }
            }

            onReleased: function(mouse) {
                if (mouse.button === Qt.LeftButton && isDragging) {
                    suppressNextClick = true;
                    if (delegateRoot.rootRef) {
                        delegateRoot.rootRef.activeDropIndex = -1;
                    }
                    if (draggedOutsideToUnpin && delegateRoot.isPinned && delegateRoot.tasksModelRef) {
                        delegateRoot.tasksModelRef.requestRemoveLauncher(delegateRoot.launcherUrl);
                    } else if (movedDuringDrag && delegateRoot.tasksModelRef) {
                        delegateRoot.tasksModelRef.syncLaunchers();
                    }
                    isDragging = false;
                    movedDuringDrag = false;
                    draggedOutsideToUnpin = false;
                }
            }

            onCanceled: {
                if (delegateRoot.rootRef) {
                    delegateRoot.rootRef.activeDropIndex = -1;
                }
                if (isDragging && movedDuringDrag && delegateRoot.tasksModelRef) {
                    delegateRoot.tasksModelRef.syncLaunchers();
                }
                isDragging = false;
                movedDuringDrag = false;
                draggedOutsideToUnpin = false;
                suppressNextClick = false;
            }

            onClicked: function(mouse) {
                if (suppressNextClick || isDragging || movedDuringDrag) {
                    suppressNextClick = false;
                    return;
                }
                const idx = freshModelIndex();
                if (!idx) {
                    return;
                }
                if (mouse.button === Qt.RightButton) {
                    return;
                }
                if (mouse.button === Qt.MiddleButton) {
                    delegateRoot.tasksModelRef.requestNewInstance(idx);
                    return;
                }
                if (delegateRoot.isGroupParent && delegateRoot.childCount > 1 && delegateRoot.groupDialogRef) {
                    delegateRoot.groupDialogRef.openForDelegate(delegateRoot);
                    return;
                }
                if (delegateRoot.isLauncher) {
                    delegateRoot.tasksModelRef.requestActivate(idx);
                } else if (delegateRoot.isActive && delegateRoot.minimizeActiveOnClick) {
                    delegateRoot.tasksModelRef.requestToggleMinimized(idx);
                } else {
                    delegateRoot.tasksModelRef.requestActivate(idx);
                }
            }

            onWheel: function(wheel) {
                if (delegateRoot.isLauncher) {
                    return;
                }
                const idx = freshModelIndex();
                if (!idx) {
                    return;
                }
                if (wheel.angleDelta.y > 0) {
                    delegateRoot.tasksModelRef.requestActivate(idx);
                } else if (wheel.angleDelta.y < 0 && delegateRoot.isActive && delegateRoot.isMinimizable) {
                    delegateRoot.tasksModelRef.requestToggleMinimized(idx);
                }
            }
        }
    }

    // ── Pixel-exact overlay layer ────────────────────────────────────────────────
    // The delegate is authored at its *magnified* logical size and GPU-scaled down by
    // restingScale at rest, so anything drawn directly in it lands on fractional pixels and
    // is resampled (blurry dots, smeared symbolic glyphs, 4 px text). This layer counter-scales
    // by exactly 1 / restingScale, so at rest its children map 1:1 onto physical pixels. It is
    // anchored to the corner DockPhysicsEngine snaps to the device-pixel grid when settled, and
    // every coordinate below is an integer number of physical pixels.
    // While magnifying, the combined transform is the smooth magnification factor (constant
    // overlay scale → zero per-frame bindings here); vector content (rects, CurveRenderer glyph
    // and text) stays sharp at any zoom.
    Item {
        id: overlay
        z: 15

        readonly property real dpr: delegateRoot.dockLayoutRef
            ? Math.max(0.5, delegateRoot.dockLayoutRef.devicePixelRatio) : 1.0
        readonly property int sidePx: Math.max(1, Math.round(delegateRoot.effectiveBaseSize * dpr))
        readonly property real side: sidePx / dpr
        readonly property real r: delegateRoot.restingScale

        // Integer physical-pixel count for a logical length, with a logical minimum.
        function npx(v, minLogical) {
            return Math.max(Math.max(1, Math.round(minLogical * dpr)), Math.round(v * dpr));
        }
        // Same, but with parity matched to `extentPx` so the span centres exactly on whole pixels.
        function centredPx(v, minLogical, extentPx) {
            const n = npx(v, minLogical);
            return (((n - extentPx) % 2) + 2) % 2 === 0 ? n : n + 1;
        }
        function px(n) { return n / dpr; }

        width: side
        height: side
        transformOrigin: Item.TopLeft
        scale: 1.0 / r
        // Pin to the engine's pixel-aligned corner: leading edge + screen-edge base.
        x: delegateRoot.dockEdge === 3 ? delegateRoot.width - side / r : 0
        y: delegateRoot.dockEdge === 0 ? delegateRoot.height - side / r : 0

        // Glyph/text rendering: hinted native glyphs 1:1 at rest, resolution-independent curves when zoomed.
        readonly property int textRenderType: delegateRoot.isMagnifying ? Text.CurveRendering : Text.NativeRendering

        // SmartLauncher progress bar (bottom of icon)
        Rectangle {
            id: progressTrack
            visible: delegateRoot.smartProgressVisible
            readonly property int insetPx: overlay.npx(6 * overlay.r, 2)
            readonly property int hPx: overlay.npx(4 * overlay.r, 2)
            readonly property int bottomPx: overlay.npx(8 * overlay.r, 2)
            x: overlay.px(insetPx)
            y: overlay.px(overlay.sidePx - bottomPx - hPx)
            width: overlay.px(Math.max(2, overlay.sidePx - 2 * insetPx))
            height: overlay.px(hPx)
            radius: height * 0.5
            color: Qt.rgba(0, 0, 0, 0.55)

            Rectangle {
                width: overlay.px(Math.round(Math.max(0, Math.min(1, delegateRoot.smartProgress)) * progressTrack.width * overlay.dpr))
                height: parent.height
                radius: parent.radius
                color: Kirigami.Theme.highlightColor
            }
        }

        // Customizable Running / Active / Attention Indicator
        // indicatorStyle: 0 = Pill Bar, 1 = Classic Dot, 2 = Glowing Underline, 3 = Minimal Line, 4 = None
        // indicatorPosition: 0 = Screen Edge (Outer), 1 = Inside Icon
        Item {
            id: indicatorHolder
            visible: delegateRoot.indicatorStyle !== 4
                && (delegateRoot.isWindow || delegateRoot.isGroupParent || delegateRoot.isDemandingAttention)

            readonly property real b: overlay.side
            readonly property real r: overlay.r
            // Lengths in resting logical px (legacy delegate-unit formulas × restingScale).
            readonly property real primaryLen: {
                switch (delegateRoot.indicatorStyle) {
                case 1: return Math.max(6 * r, b * 0.10);
                case 2: return delegateRoot.isActive ? Math.max(18 * r, b * 0.42) : Math.max(10 * r, b * 0.22);
                case 3: return delegateRoot.isActive ? Math.max(14 * r, b * 0.30) : Math.max(8 * r, b * 0.16);
                case 0:
                default: return delegateRoot.isActive ? Math.max(12 * r, b * 0.24) : Math.max(6 * r, b * 0.11);
                }
            }
            readonly property real crossLen: {
                switch (delegateRoot.indicatorStyle) {
                case 1: return primaryLen;
                case 2: return Math.max(4 * r, b * 0.065);
                case 3: return 2.5 * r;
                case 0:
                default: return Math.max(5 * r, b * 0.08);
                }
            }
            readonly property int primaryPx: overlay.centredPx(primaryLen, 3, overlay.sidePx)
            readonly property int crossPx: delegateRoot.indicatorStyle === 1
                ? primaryPx
                : overlay.npx(crossLen, delegateRoot.indicatorStyle === 3 ? 1 : 2)
            // Outer position: centre the indicator in the gap between the icon and the dock's
            // outer edge (pill padding or screen-edge padding) instead of overlapping the icon.
            readonly property int marginPx: Math.round(delegateRoot.iconMargin * overlay.dpr)
            readonly property int gapPx: Math.round(delegateRoot.indicatorOutset * overlay.dpr) + marginPx
            readonly property int outsetPx: gapPx >= crossPx + marginPx + 2
                ? Math.round(gapPx / 2 - marginPx + crossPx / 2)
                : 0
            readonly property int insetPx: delegateRoot.indicatorPosition === 1 ? overlay.npx(4 * r, 1) : -outsetPx
            readonly property int glowPadPx: overlay.npx(3 * r, 1)
            readonly property color indicatorColor: {
                if (delegateRoot.isDemandingAttention) {
                    return Kirigami.Theme.negativeTextColor;
                }
                switch (delegateRoot.indicatorColorMode) {
                case 1: return Kirigami.Theme.textColor;
                case 2: return Kirigami.Theme.highlightColor;
                default: return delegateRoot.isActive ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor;
                }
            }

            readonly property int wPx: delegateRoot.isHorizontalDock ? primaryPx : crossPx
            readonly property int hPx: delegateRoot.isHorizontalDock ? crossPx : primaryPx

            width: overlay.px(wPx)
            height: overlay.px(hPx)
            x: overlay.px(delegateRoot.isHorizontalDock
                ? (overlay.sidePx - wPx) / 2
                : (delegateRoot.dockEdge === 2 ? insetPx : overlay.sidePx - wPx - insetPx))
            y: overlay.px(!delegateRoot.isHorizontalDock
                ? (overlay.sidePx - hPx) / 2
                : (delegateRoot.dockEdge === 1 ? insetPx : overlay.sidePx - hPx - insetPx))

            // Soft accent glow for Glowing Underline style
            Rectangle {
                visible: delegateRoot.indicatorStyle === 2
                readonly property int padPx: indicatorHolder.glowPadPx
                x: -overlay.px(padPx)
                y: -overlay.px(Math.max(1, padPx - 1))
                width: parent.width + 2 * overlay.px(padPx)
                height: parent.height + 2 * overlay.px(Math.max(1, padPx - 1))
                radius: Math.min(width, height) * 0.5
                color: indicatorHolder.indicatorColor
                opacity: delegateRoot.isActive ? 0.35 : 0.18
            }

            Rectangle {
                anchors.fill: parent
                radius: Math.min(width, height) * 0.5
                color: indicatorHolder.indicatorColor
                opacity: delegateRoot.isMinimized ? 0.45 : 0.95
            }
        }

        // SmartLauncher unread count badge OR group count badge (top-right)
        Rectangle {
            id: countBadge
            readonly property int sizePx: overlay.npx(Math.min(20, Math.max(11, overlay.side * 0.38)), 11)
            readonly property int wPx: Math.max(sizePx, Math.ceil(countLabel.implicitWidth * overlay.dpr) + Math.round(sizePx * 0.5))
            visible: delegateRoot.smartCountVisible || (delegateRoot.isGroupParent && delegateRoot.childCount > 1)
            x: overlay.px(overlay.sidePx - wPx)
            y: 0
            width: overlay.px(wPx)
            height: overlay.px(sizePx)
            radius: height * 0.5
            color: delegateRoot.smartCountVisible ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.highlightColor

            Text {
                id: countLabel
                x: overlay.px(Math.round((countBadge.wPx - implicitWidth * overlay.dpr) / 2))
                y: overlay.px(Math.round((countBadge.sizePx - implicitHeight * overlay.dpr) / 2))
                text: delegateRoot.smartCountVisible
                    ? (delegateRoot.smartCount > 99 ? "99+" : String(delegateRoot.smartCount))
                    : String(delegateRoot.childCount)
                color: "#ffffff"
                font.pixelSize: Math.max(8, Math.round(countBadge.height * 0.68))
                font.bold: true
                renderType: overlay.textRenderType
            }
        }

        // Shortcut index badge (shown smoothly while holding Meta when enabled in settings)
        Rectangle {
            id: indexBadge
            readonly property int sizePx: overlay.npx(Math.min(22, Math.max(12, overlay.side * 0.42)), 12)
            readonly property bool badgeActive: delegateRoot.showIndexBadge && delegateRoot.taskIndex >= 0 && delegateRoot.taskIndex < 9
            visible: opacity > 0.01 && delegateRoot.taskIndex >= 0 && delegateRoot.taskIndex < 9
            opacity: badgeActive ? 1.0 : 0.0
            scale: badgeActive ? 1.0 : 0.82
            x: 0
            y: 0
            width: overlay.px(sizePx)
            height: overlay.px(sizePx)
            radius: overlay.px(Math.max(2, Math.round(sizePx * 0.28)))
            color: Qt.rgba(0, 0, 0, 0.72)
            border.width: overlay.px(1)
            border.color: Qt.rgba(1, 1, 1, 0.22)

            Behavior on opacity { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }

            Text {
                x: overlay.px(Math.round((indexBadge.sizePx - implicitWidth * overlay.dpr) / 2))
                y: overlay.px(Math.round((indexBadge.sizePx - implicitHeight * overlay.dpr) / 2))
                text: String(delegateRoot.taskIndex + 1)
                color: "#ffffff"
                font.pixelSize: Math.max(8, Math.round(indexBadge.height * 0.62))
                font.bold: true
                renderType: overlay.textRenderType
            }
        }

        // Clickable Audio / Mute Indicator Badge (top-right unless the count badge occupies it).
        // The speaker glyph is drawn as vector paths with the CurveRenderer (analytic coverage AA in
        // the fragment shader), so it is sharp at 1:1 and stays sharp under magnification, unlike a
        // pre-rasterized 16 px symbolic icon resampled to ~8 px.
        Rectangle {
            id: audioBadge
            readonly property int sizePx: overlay.npx(Math.min(20, Math.max(11, overlay.side * 0.40)), 11)
            readonly property bool countBadgeVisible: countBadge.visible
            visible: delegateRoot.hasAudioStream && !(delegateRoot.showIndexBadge && countBadgeVisible)
            x: countBadgeVisible ? 0 : overlay.px(overlay.sidePx - sizePx)
            y: 0
            width: overlay.px(sizePx)
            height: overlay.px(sizePx)
            radius: height * 0.5
            color: delegateRoot.isAudioMuted
                ? Kirigami.Theme.negativeTextColor
                : (audioMouseArea.containsMouse
                    ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.90)
                    : Qt.rgba(0, 0, 0, 0.55))

            Behavior on color { ColorAnimation { duration: 100 } }

            // 16×16 design grid (Breeze-style speaker), uniformly scaled into the badge.
            Shape {
                id: speakerGlyph
                readonly property real glyphSize: audioBadge.width * 0.70
                width: 16
                height: 16
                x: (audioBadge.width - glyphSize) * 0.5
                y: (audioBadge.height - glyphSize) * 0.5
                transformOrigin: Item.TopLeft
                scale: glyphSize / 16
                preferredRendererType: Shape.CurveRenderer

                // Speaker body
                ShapePath {
                    strokeWidth: -1
                    fillColor: "#ffffff"
                    startX: 1.5; startY: 5.5
                    PathLine { x: 4.5; y: 5.5 }
                    PathLine { x: 8.5; y: 1.5 }
                    PathLine { x: 8.5; y: 14.5 }
                    PathLine { x: 4.5; y: 10.5 }
                    PathLine { x: 1.5; y: 10.5 }
                    PathLine { x: 1.5; y: 5.5 }
                }
                // Sound waves (playing)
                ShapePath {
                    strokeColor: delegateRoot.isAudioMuted ? "transparent" : "#ffffff"
                    strokeWidth: 1.6
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    startX: 10.8; startY: 5.6
                    PathArc { x: 10.8; y: 10.4; radiusX: 3.4; radiusY: 3.4 }
                }
                ShapePath {
                    strokeColor: delegateRoot.isAudioMuted ? "transparent" : "#ffffff"
                    strokeWidth: 1.6
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    startX: 12.6; startY: 3.2
                    PathArc { x: 12.6; y: 12.8; radiusX: 6.2; radiusY: 6.2 }
                }
                // Mute cross
                ShapePath {
                    strokeColor: delegateRoot.isAudioMuted ? "#ffffff" : "transparent"
                    strokeWidth: 1.6
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    startX: 10.5; startY: 5.5
                    PathLine { x: 15; y: 10.5 }
                    PathMove { x: 15; y: 5.5 }
                    PathLine { x: 10.5; y: 10.5 }
                }
            }

            MouseArea {
                id: audioMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: function(mouse) {
                    mouse.accepted = true;
                    if (delegateRoot.audioTrackerRef) {
                        delegateRoot.audioTrackerRef.toggleMuteForTask(
                            delegateRoot.appPid,
                            delegateRoot.appName,
                            delegateRoot.appId
                        );
                    }
                }
            }
        }
    }
}
