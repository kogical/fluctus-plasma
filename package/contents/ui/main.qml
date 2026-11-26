import QtQuick
import QtQuick.Layouts

import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.taskmanager as TaskManager

PlasmoidItem {
    id: root

    Plasmoid.constraintHints: Plasmoid.CanFillArea
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
    property bool showIndexBadges: false

    Timer {
        id: badgeTimer
        interval: 1500
        onTriggered: root.showIndexBadges = false
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

    function activateTaskAtIndex(index) {
        if (index < 0 || index >= tasksModel.count) {
            return;
        }
        root.showIndexBadges = true;
        badgeTimer.restart();

        const modelIndex = tasksModel.makeModelIndex(index);
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
    }

    onInEditModeChanged: {
        dockLayout.reapplyBlur();
    }

    fullRepresentation: Item {
        id: container

        Layout.preferredWidth: dockLayout.implicitWidth
        Layout.preferredHeight: dockLayout.implicitHeight
        Layout.minimumWidth: root.isVertical ? dockLayout.recommendedPanelThickness : dockLayout.implicitWidth
        Layout.minimumHeight: root.isVertical ? dockLayout.implicitHeight : dockLayout.recommendedPanelThickness

        // Clean Plasma/glass dock pill synced to C++ pillRect
        Rectangle {
            id: dockPill
            x: dockLayout.pillRect.x
            y: dockLayout.pillRect.y
            width: dockLayout.pillRect.width
            height: dockLayout.pillRect.height
            visible: dockLayout.count > 0 && width > 0 && height > 0
            radius: Plasmoid.configuration.pillCornerRadius
            color: Qt.rgba(
                Kirigami.Theme.backgroundColor.r,
                Kirigami.Theme.backgroundColor.g,
                Kirigami.Theme.backgroundColor.b,
                Plasmoid.configuration.pillOpacity
            )
            border.width: 1
            border.color: Qt.rgba(
                Kirigami.Theme.textColor.r,
                Kirigami.Theme.textColor.g,
                Kirigami.Theme.textColor.b,
                0.16
            )
        }

        DockLayoutItem {
            id: dockLayout
            anchors.fill: parent
            model: tasksModel
            edge: root.dockEdge
            reversePrimary: LayoutMirroring.enabled !== Plasmoid.configuration.reverseMode
            baseIconSize: Plasmoid.configuration.baseIconSize
            iconSpacing: Plasmoid.configuration.iconSpacing
            maxScale: Plasmoid.configuration.maxScale
            influenceRadiusIcons: Plasmoid.configuration.influenceRadiusIcons
            magnificationCurve: Plasmoid.configuration.magnificationCurve
            magnificationEnabled: Plasmoid.configuration.magnificationEnabled
            bounceEnabled: Plasmoid.configuration.bounceEnabled
            bounceRatio: Plasmoid.configuration.bounceRatio
            blurEnabled: Plasmoid.configuration.blurEnabled && !root.inEditMode
            pillCornerRadius: Plasmoid.configuration.pillCornerRadius
            edgePadding: Plasmoid.configuration.edgePadding
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
                tasksModelRef: tasksModel
                showIndexBadge: root.showIndexBadges
                minimizeActiveOnClick: Plasmoid.configuration.minimizeActiveTaskOnClick
                enableMipmap: Plasmoid.configuration.maxScale > 1.8
            }
        }

        // Single passive HoverHandler — never swallowed by child delegates
        HoverHandler {
            id: dockHover
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
            onPointChanged: {
                if (hovered) {
                    dockLayout.updatePointer(point.position.x, point.position.y);
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

        // Forward drag-and-drop coordinates into the same C++ pointer entry point (Wayland suppresses hover during DnD)
        DropArea {
            anchors.fill: parent
            onPositionChanged: function(drag) {
                dockLayout.updatePointer(drag.x, drag.y);
            }
            onExited: {
                dockLayout.clearPointer();
            }
            onDropped: function(drop) {
                dockLayout.clearPointer();
                if (drop.hasUrls) {
                    for (let i = 0; i < drop.urls.length; ++i) {
                        tasksModel.requestAddLauncher(drop.urls[i]);
                    }
                    drop.acceptProposedAction();
                }
            }
        }
    }
}
