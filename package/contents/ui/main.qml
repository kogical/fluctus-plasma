import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.taskmanager as TaskManager

PlasmoidItem {
    id: root

    Plasmoid.constraintHints: Plasmoid.CanFillArea
    // Removes the hosting panel's own frame/blur/shadow (restored when disabled or removed).
    // FluctusApplet-specific properties are not visible to the statically typed attached
    // Plasmoid object, so bind through an explicit Binding.
    Binding {
        target: Plasmoid
        property: "hidePanelBackground"
        value: Plasmoid.configuration.hidePanelBackground
    }
    preferredRepresentation: fullRepresentation

    readonly property bool isVertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property int dockEdge: {
        switch (Plasmoid.location) {
        case PlasmaCore.Types.TopEdge:
            return 1;
        case PlasmaCore.Types.LeftEdge:
            return 2;
        case PlasmaCore.Types.RightEdge:
            return 3;
        case PlasmaCore.Types.BottomEdge:
        default:
            return 0;
        }
    }
    readonly property bool inEditMode: Boolean(Plasmoid.containment && Plasmoid.containment.corona && Plasmoid.containment.corona.editMode)
    readonly property int backgroundMode: Plasmoid.configuration.backgroundMode
    readonly property int blurStyle: Plasmoid.configuration.blurStyle
    readonly property bool isCustomGlassPill: backgroundMode === 1
    readonly property bool isPlasmaThemePill: backgroundMode === 2
    // Liquid Glass relies on the Glass KWin effect for real refraction. Without it the dock uses
    // Frosted instead, and the settings explain how to get the effect.
    readonly property bool liquidGlassAvailable: Plasmoid.glassEffectState === 2
    readonly property int effectiveMaterial: blurStyle === 1 && !liquidGlassAvailable ? 0 : blurStyle

    property int smartLauncherRevision: 0
    property int activeDropIndex: -1
    property int externalHoverIndex: -1

    // Hover-to-raise window / open group when dragging external files over a dock icon
    Timer {
        id: dropHoverTimer
        interval: 500
        repeat: false
        onTriggered: {
            if (root.externalHoverIndex < 0 || root.externalHoverIndex >= tasksModel.count) {
                return;
            }
            const item = dockLayout.itemAtIndex(root.externalHoverIndex);
            if (!item) {
                return;
            }
            if (item.isGroupParent && item.childCount > 1) {
                groupDialog.openForDelegate(item);
            } else if (item.isWindow && item.modelIndex) {
                tasksModel.requestActivate(item.modelIndex);
            }
        }
    }

    // Lazily load PulseAudio stream tracker so systems without plasma-pa degrade cleanly
    Loader {
        id: audioTrackerLoader
        active: Plasmoid.configuration.indicateAudioStreams
        source: "AudioStreamTracker.qml"
    }

    TaskManager.VirtualDesktopInfo {
        id: virtualDesktopInfo
    }

    TaskManager.ActivityInfo {
        id: activityInfo
    }

    TaskManager.TasksModel {
        id: tasksModel
        sortMode: TaskManager.TasksModel.SortManual
        groupMode: TaskManager.TasksModel.GroupApplications
        taskReorderingEnabled: true
        launchInPlace: true
        separateLaunchers: Plasmoid.configuration.separateLaunchers
        filterByScreen: Plasmoid.configuration.showOnlyCurrentScreen
        filterByVirtualDesktop: Plasmoid.configuration.showOnlyCurrentDesktop
        filterByActivity: Plasmoid.configuration.showOnlyCurrentActivity
        virtualDesktop: virtualDesktopInfo.currentDesktop
        activity: activityInfo.currentActivity
        screenGeometry: Plasmoid.screenGeometry
        launcherList: Plasmoid.configuration.launchers

        onLauncherListChanged: {
            Plasmoid.configuration.launchers = launcherList;
        }
    }

    TaskContextMenu {
        id: contextMenu
        tasksModelRef: tasksModel
        virtualDesktopInfoRef: virtualDesktopInfo
        activityInfoRef: activityInfo
        audioTrackerRef: audioTrackerLoader.item
    }

    GroupDialog {
        id: groupDialog
        tasksModelRef: tasksModel
    }

    function activateTaskAtIndex(index) {
        if (index < 0 || index >= tasksModel.count) {
            return;
        }

        const modelIndex = tasksModel.makeModelIndex(index);
        const isGroupParent = Boolean(tasksModel.data(modelIndex, TaskManager.AbstractTasksModel.IsGroupParent));
        const childCount = Number(tasksModel.data(modelIndex, TaskManager.AbstractTasksModel.ChildCount) || 0);
        if (isGroupParent && childCount > 1) {
            let activeChild = -1;
            for (let c = 0; c < childCount; ++c) {
                const childIdx = tasksModel.makeModelIndex(index, c);
                if (tasksModel.data(childIdx, TaskManager.AbstractTasksModel.IsActive)) {
                    activeChild = c;
                    break;
                }
            }
            const nextChild = (activeChild >= 0) ? ((activeChild + 1) % childCount) : 0;
            tasksModel.requestActivate(tasksModel.makeModelIndex(index, nextChild));
            return;
        }

        const isActive = tasksModel.data(modelIndex, TaskManager.AbstractTasksModel.IsActive);
        if (isActive && Plasmoid.configuration.minimizeActiveTaskOnClick) {
            tasksModel.requestToggleMinimized(modelIndex);
        } else {
            tasksModel.requestActivate(modelIndex);
        }
    }

    Connections {
        target: Plasmoid
        function onActivateTaskRequested(index) {
            root.activateTaskAtIndex(index);
        }
        function onScreenGeometryChanged() {
            Plasmoid.screenGeometry = root.Plasmoid.screenGeometry;
        }
        function onSmartLauncherUpdated() {
            root.smartLauncherRevision += 1;
        }
    }

    fullRepresentation: Item {
        id: container

        // Adapt cleanly to the surrounding KDE Plasma panel thickness without trapping it.
        Layout.fillWidth: root.isVertical
        Layout.fillHeight: !root.isVertical
        Layout.preferredWidth: root.isVertical ? -1 : dockLayout.implicitWidth
        Layout.preferredHeight: root.isVertical ? dockLayout.implicitHeight : -1
        Layout.minimumWidth: root.isVertical ? 24 : Math.min(dockLayout.implicitWidth, 64)
        Layout.minimumHeight: root.isVertical ? Math.min(dockLayout.implicitHeight, 64) : 24

        // Report panel geometry so the settings can recommend / set the optimal panel height.
        Binding {
            target: Plasmoid
            property: "dockCrossExtent"
            value: root.isVertical ? container.width : container.height
        }
        Component.onCompleted: Plasmoid.setPanelWindow(container.Window.window)
        Window.onWindowChanged: Plasmoid.setPanelWindow(container.Window.window)

        // PanelView rewrites the window blur when its background hints or edit mode change.
        Connections {
            target: Plasmoid
            function onPanelBackgroundStateChanged() {
                dockLayout.reapplyBlur();
            }
        }
        Connections {
            target: root
            function onInEditModeChanged() {
                dockLayout.reapplyBlur();
            }
        }

        // Dock surface (Glass pill / Plasma theme surface). Shared with the settings previews.
        DockBackground {
            id: dockSurface
            x: dockLayout.pillRect.x
            y: dockLayout.pillRect.y
            width: dockLayout.pillRect.width
            height: dockLayout.pillRect.height
            visible: (root.isCustomGlassPill || root.isPlasmaThemePill) && dockLayout.count > 0 && width > 0 && height > 0
            backgroundMode: root.backgroundMode
            materialIndex: root.effectiveMaterial
            colorMode: Plasmoid.configuration.pillColorMode
            customColor: Plasmoid.configuration.pillCustomColor
            tintOpacity: Plasmoid.configuration.pillOpacity
            cornerRadius: Plasmoid.configuration.pillCornerRadius
            borderStyle: Plasmoid.configuration.pillBorderStyle
            borderWidth: Plasmoid.configuration.pillBorderWidth
            borderOpacity: Plasmoid.configuration.pillBorderOpacity
            shadowEnabled: Plasmoid.configuration.pillShadow
            shadowStrength: Plasmoid.configuration.pillShadowStrength
            highlightStrength: Plasmoid.configuration.glassHighlight
            vertical: root.isVertical
            dockEdge: root.dockEdge
            interactive: Plasmoid.configuration.glassInteractive
            pointer: container.lastPointer
            // The physics engine's hover engagement: springs in/out and settles at rest.
            pointerStrength: dockLayout.hoverAlpha
        }

        DockLayoutItem {
            id: dockLayout
            anchors.fill: parent
            model: tasksModel
            edge: root.dockEdge
            reversePrimary: LayoutMirroring.enabled !== Plasmoid.configuration.reverseMode
            autoScaleToPanel: true
            panelIconRatio: Plasmoid.configuration.panelIconRatio
            maxZoomPanelRatio: Plasmoid.configuration.maxZoomPanelRatio
            iconSpacingRatio: Plasmoid.configuration.iconSpacingRatio
            clampToContainer: true
            influenceRadiusIcons: Plasmoid.configuration.influenceRadiusIcons
            magnificationCurve: Plasmoid.configuration.magnificationCurve
            magnificationEnabled: Plasmoid.configuration.magnificationEnabled
            bounceEnabled: Plasmoid.configuration.bounceEnabled
            bounceRatio: Plasmoid.configuration.bounceRatio
            // Never override KWindowEffects::enableBlurBehind in Default (Panel Colorizer) or Transparent modes!
            blurEnabled: (root.isCustomGlassPill || root.isPlasmaThemePill)
                && dockSurface.wantsBlur && Plasmoid.configuration.blurEnabled && !root.inEditMode
            blurContrast: dockSurface.compositorContrast?.contrast ?? 1.0
            blurIntensity: dockSurface.compositorContrast?.intensity ?? 1.0
            blurSaturation: dockSurface.compositorContrast?.saturation ?? 1.0
            blurMask: root.isPlasmaThemePill ? dockSurface.themeMask : undefined
            pillCornerRadius: Math.min(Plasmoid.configuration.pillCornerRadius, Math.min(pillRect.width, pillRect.height) * 0.5)
            edgePadding: Plasmoid.configuration.edgePadding
            pillPaddingPrimary: root.backgroundMode === 0 ? 4 : Plasmoid.configuration.pillPaddingPrimary
            pillPaddingCross: root.backgroundMode === 0 ? 0 : Plasmoid.configuration.pillPaddingCross
            topMargin: Plasmoid.configuration.topMargin
            editModeFrozen: root.inEditMode

            onHasAttentionTaskChanged: {
                if (Plasmoid.configuration.unhideOnAttention) {
                    Plasmoid.setNeedsAttention(hasAttentionTask);
                } else {
                    Plasmoid.setNeedsAttention(false);
                }
            }

            delegate: TaskDelegate {
                rootRef: root
                tasksModelRef: tasksModel
                dockLayoutRef: dockLayout
                contextMenuRef: contextMenu
                groupDialogRef: groupDialog
                audioTrackerRef: audioTrackerLoader.item
                showIndexBadge: Plasmoid.metaKeyPressed && Plasmoid.configuration.showShortcutBadgesOnMeta
                minimizeActiveOnClick: Plasmoid.configuration.minimizeActiveTaskOnClick
                enableMipmap: (Plasmoid.configuration.maxZoomPanelRatio / Math.max(0.25, Plasmoid.configuration.panelIconRatio)) > 1.8
                showReflection: Plasmoid.configuration.showReflection
                showTooltips: Plasmoid.configuration.showTooltips
                showWindowPreviews: Plasmoid.configuration.showWindowPreviews
                indicateAudioStreams: Plasmoid.configuration.indicateAudioStreams
                dragOffToUnpin: Plasmoid.configuration.dragOffToUnpin
                indicatorStyle: Plasmoid.configuration.indicatorStyle
                indicatorPosition: Plasmoid.configuration.indicatorPosition
                indicatorColorMode: Plasmoid.configuration.indicatorColorMode
                indicatorOutset: (root.isCustomGlassPill || root.isPlasmaThemePill)
                    ? Plasmoid.configuration.pillPaddingCross
                    : Plasmoid.configuration.edgePadding
                smartLauncherRevision: root.smartLauncherRevision
            }
        }

        // Visual Drop / Reorder Insertion Indicator
        Rectangle {
            id: dropIndicator
            readonly property rect indRect: root.activeDropIndex >= 0
                ? dockLayout.insertionIndicatorRect(root.activeDropIndex)
                : Qt.rect(0, 0, 0, 0)
            visible: root.activeDropIndex >= 0 && indRect.width > 0 && indRect.height > 0
            x: indRect.x
            y: indRect.y
            width: indRect.width
            height: indRect.height
            radius: Math.min(width, height) * 0.5
            color: Kirigami.Theme.highlightColor
            opacity: 0.9
            z: 50

            Behavior on x { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
            Behavior on y { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        }

        /// Last pointer position over the dock (kept after leaving so effects fade out in place).
        property point lastPointer: Qt.point(width / 2, height / 2)

        // Single passive HoverHandler — never swallowed by child delegates
        HoverHandler {
            id: dockHover
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
            onPointChanged: {
                if (hovered) {
                    dockLayout.updatePointer(point.position.x, point.position.y);
                    container.lastPointer = point.position;
                }
            }
            onHoveredChanged: {
                if (!hovered) {
                    dockLayout.clearPointer();
                } else {
                    dockLayout.updatePointer(point.position.x, point.position.y);
                }
            }
        }

        // Forward external drag-and-drop coordinates into C++ pointer entry point.
        // Supports pinning .desktop/app URLs, visual insertion guide, and 500ms hover-to-raise.
        DropArea {
            anchors.fill: parent
            onPositionChanged: function(drag) {
                dockLayout.updatePointer(drag.x, drag.y);
                const idx = dockLayout.restingInsertionIndex(drag.x, drag.y);

                let hasDesktopUrls = false;
                if (drag.hasUrls && drag.urls) {
                    for (let i = 0; i < drag.urls.length; ++i) {
                        const u = String(drag.urls[i]);
                        if (u.startsWith("applications:") || u.endsWith(".desktop")) {
                            hasDesktopUrls = true;
                            break;
                        }
                    }
                }

                root.activeDropIndex = hasDesktopUrls ? idx : -1;

                // For non-launcher files, start 500ms hover-to-raise / open group on the hovered slot
                if (!hasDesktopUrls && tasksModel.count > 0) {
                    const targetSlot = Math.max(0, Math.min(idx, tasksModel.count - 1));
                    if (root.externalHoverIndex !== targetSlot) {
                        root.externalHoverIndex = targetSlot;
                        dropHoverTimer.restart();
                    }
                } else {
                    root.externalHoverIndex = -1;
                    dropHoverTimer.stop();
                }
            }
            onExited: {
                root.activeDropIndex = -1;
                root.externalHoverIndex = -1;
                dropHoverTimer.stop();
                dockLayout.clearPointer();
            }
            onDropped: function(drop) {
                const insertIdx = dockLayout.restingInsertionIndex(drop.x, drop.y);
                root.activeDropIndex = -1;
                root.externalHoverIndex = -1;
                dropHoverTimer.stop();
                dockLayout.clearPointer();
                if (!drop.hasUrls || drop.urls.length === 0) {
                    return;
                }

                let pinnedAny = false;
                const fileUrls = [];
                for (let i = 0; i < drop.urls.length; ++i) {
                    const urlStr = String(drop.urls[i]);
                    if (urlStr.startsWith("applications:") || urlStr.endsWith(".desktop")) {
                        if (tasksModel.requestAddLauncher(drop.urls[i])) {
                            const addedPos = tasksModel.launcherPosition(drop.urls[i]);
                            if (addedPos >= 0 && insertIdx >= 0 && insertIdx < tasksModel.count && addedPos !== insertIdx) {
                                tasksModel.move(addedPos, insertIdx);
                            }
                            pinnedAny = true;
                        }
                    } else {
                        fileUrls.push(drop.urls[i]);
                    }
                }

                if (pinnedAny) {
                    tasksModel.syncLaunchers();
                    drop.acceptProposedAction();
                } else if (fileUrls.length > 0 && insertIdx >= 0 && insertIdx < tasksModel.count) {
                    const targetIdx = tasksModel.makeModelIndex(Math.min(insertIdx, tasksModel.count - 1));
                    tasksModel.requestOpenUrls(targetIdx, fileUrls);
                    drop.acceptProposedAction();
                }
            }
        }
    }
}
