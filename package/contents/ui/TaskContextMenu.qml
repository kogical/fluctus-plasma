import QtQuick
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid
import org.kde.taskmanager as TaskManager

// Native KDE Plasma context menu (wraps QMenu via PlasmaExtras.Menu)
// Integrates directly with TaskManager.TasksModel for pinning, Jump Lists, and window actions.
PlasmaExtras.Menu {
    id: menu

    property var tasksModelRef
    property var virtualDesktopInfoRef
    property var activityInfoRef
    property var audioTrackerRef
    property var modelIndex
    property int appPid: 0
    property string appName: ""
    property string appId: ""
    property bool hasAudioStream: false
    property bool isAudioMuted: false
    property bool isLauncher: false
    property bool isWindow: false
    property bool isGroupParent: false
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
    property url launcherUrl
    property url launcherUrlFull

    readonly property bool hasLauncherUrl: launcherUrl.toString().length > 0
    readonly property bool isPinned: hasLauncherUrl && tasksModelRef
        ? tasksModelRef.launcherPosition(launcherUrl) >= 0
        : false
    readonly property bool hasMultipleActivities: Boolean(activityInfoRef && activityInfoRef.numberOfRunningActivities > 1)
    readonly property string nullActivityUuid: "00000000-0000-0000-0000-000000000000"

    property var dynamicItems: []

    function openForDelegate(delegateItem) {
        if (!delegateItem) {
            return;
        }
        visualParent = delegateItem;
        modelIndex = delegateItem.modelIndex;
        appPid = delegateItem.appPid || 0;
        appName = delegateItem.appName || "";
        appId = delegateItem.appId || "";
        hasAudioStream = Boolean(delegateItem.hasAudioStream);
        isAudioMuted = Boolean(delegateItem.isAudioMuted);
        isLauncher = delegateItem.isLauncher;
        isWindow = delegateItem.isWindow;
        isGroupParent = delegateItem.isGroupParent;
        isMinimized = delegateItem.isMinimized;
        isMaximized = delegateItem.isMaximized;
        isFullScreen = delegateItem.isFullScreen;
        isKeepAbove = delegateItem.isKeepAbove;
        isKeepBelow = delegateItem.isKeepBelow;
        isClosable = delegateItem.isClosable;
        isMinimizable = delegateItem.isMinimizable;
        isMaximizable = delegateItem.isMaximizable;
        isMovable = delegateItem.isMovable;
        isResizable = delegateItem.isResizable;
        isFullScreenable = delegateItem.isFullScreenable;
        isOnAllVirtualDesktops = delegateItem.isOnAllVirtualDesktops;
        virtualDesktops = delegateItem.virtualDesktops || [];
        launcherUrl = delegateItem.launcherUrl;
        launcherUrlFull = delegateItem.launcherUrlFull;

        rebuildJumpListItems();
        rebuildVirtualDesktopItems();
        rebuildActivityPinItems();
        openRelative();
    }

    function clearDynamicItems() {
        for (let i = 0; i < dynamicItems.length; ++i) {
            removeMenuItem(dynamicItems[i]);
            dynamicItems[i].destroy();
        }
        dynamicItems = [];
    }

    function rebuildJumpListItems() {
        clearDynamicItems();
        if (!hasLauncherUrl) {
            jumpListSeparator.visible = false;
            return;
        }
        const actions = Plasmoid.jumpListActions(launcherUrl);
        if (!actions || actions.length === 0) {
            jumpListSeparator.visible = false;
            return;
        }
        const created = [];
        for (let i = 0; i < actions.length; ++i) {
            const act = actions[i];
            const item = jumpListItemComponent.createObject(menu, {
                text: act.text,
                icon: act.iconName || "",
                actionName: act.name,
                targetUrl: menu.launcherUrl
            });
            addMenuItem(item, jumpListSeparator);
            created.push(item);
        }
        dynamicItems = created;
        jumpListSeparator.visible = created.length > 0;
    }

    function rebuildVirtualDesktopItems() {
        desktopSubMenu.clearMenuItems();
        if (!virtualDesktopInfoRef || virtualDesktopInfoRef.numberOfDesktops <= 1 || !isWindow) {
            return;
        }
        const allItem = desktopItemComponent.createObject(desktopSubMenu, {
            text: i18n("All Desktops"),
            checkable: true,
            checked: menu.isOnAllVirtualDesktops,
            desktopId: ""
        });
        desktopSubMenu.addMenuItem(allItem);

        const sep = separatorComponent.createObject(desktopSubMenu);
        desktopSubMenu.addMenuItem(sep);

        const names = virtualDesktopInfoRef.desktopNames || [];
        const ids = virtualDesktopInfoRef.desktopIds || [];
        for (let i = 0; i < ids.length; ++i) {
            const dId = ids[i];
            const onThis = !menu.isOnAllVirtualDesktops && menu.virtualDesktops && menu.virtualDesktops.indexOf(dId) >= 0;
            const dItem = desktopItemComponent.createObject(desktopSubMenu, {
                text: names[i] || i18n("Desktop %1", i + 1),
                checkable: true,
                checked: onThis,
                desktopId: dId
            });
            desktopSubMenu.addMenuItem(dItem);
        }
    }

    function rebuildActivityPinItems() {
        activityPinSubMenu.clearMenuItems();
        if (!hasMultipleActivities || !hasLauncherUrl || !tasksModelRef) {
            return;
        }
        const pinnedActs = tasksModelRef.launcherActivities(launcherUrl) || [];
        const onAll = pinnedActs.indexOf(nullActivityUuid) >= 0;

        const allActItem = activityPinItemComponent.createObject(activityPinSubMenu, {
            text: i18n("On All Activities"),
            checkable: true,
            checked: onAll,
            activityId: nullActivityUuid
        });
        activityPinSubMenu.addMenuItem(allActItem);

        const sep = separatorComponent.createObject(activityPinSubMenu);
        activityPinSubMenu.addMenuItem(sep);

        const running = activityInfoRef.runningActivities() || [];
        for (let i = 0; i < running.length; ++i) {
            const actId = running[i];
            const isChecked = onAll || pinnedActs.indexOf(actId) >= 0;
            const aItem = activityPinItemComponent.createObject(activityPinSubMenu, {
                text: activityInfoRef.activityName(actId) || actId,
                checkable: true,
                checked: isChecked,
                activityId: actId
            });
            activityPinSubMenu.addMenuItem(aItem);
        }
    }

    Component {
        id: activityPinItemComponent
        PlasmaExtras.MenuItem {
            property string activityId: ""
            onClicked: {
                if (!menu.tasksModelRef || !menu.hasLauncherUrl) {
                    return;
                }
                const urlToAdd = menu.launcherUrlFull.toString().length > 0
                    ? menu.launcherUrlFull
                    : menu.launcherUrl;
                if (activityId === menu.nullActivityUuid) {
                    const pinnedActs = menu.tasksModelRef.launcherActivities(menu.launcherUrl) || [];
                    if (pinnedActs.indexOf(menu.nullActivityUuid) >= 0) {
                        menu.tasksModelRef.requestRemoveLauncher(menu.launcherUrl);
                    } else {
                        menu.tasksModelRef.requestAddLauncher(urlToAdd);
                    }
                } else {
                    const pinnedActs = menu.tasksModelRef.launcherActivities(menu.launcherUrl) || [];
                    if (pinnedActs.indexOf(activityId) >= 0) {
                        menu.tasksModelRef.requestRemoveLauncherFromActivity(menu.launcherUrl, activityId);
                    } else {
                        menu.tasksModelRef.requestAddLauncherToActivity(urlToAdd, activityId);
                    }
                }
            }
        }
    }

    Component {
        id: jumpListItemComponent
        PlasmaExtras.MenuItem {
            property string actionName: ""
            property url targetUrl
            onClicked: Plasmoid.triggerJumpListAction(targetUrl, actionName)
        }
    }

    Component {
        id: separatorComponent
        PlasmaExtras.MenuItem {
            separator: true
        }
    }

    Component {
        id: desktopItemComponent
        PlasmaExtras.MenuItem {
            property var desktopId: ""
            onClicked: {
                if (!menu.tasksModelRef || !menu.modelIndex) {
                    return;
                }
                if (desktopId === "") {
                    menu.tasksModelRef.requestVirtualDesktops(menu.modelIndex, []);
                } else {
                    menu.tasksModelRef.requestVirtualDesktops(menu.modelIndex, [desktopId]);
                }
            }
        }
    }

    // Jump List actions are inserted dynamically right above this separator
    PlasmaExtras.MenuItem {
        id: jumpListSeparator
        separator: true
        visible: false
    }

    // Start New Instance
    PlasmaExtras.MenuItem {
        visible: menu.hasLauncherUrl
        icon: "window-new"
        text: menu.isLauncher ? i18n("Launch") : i18n("Start New Instance")
        onClicked: {
            if (!menu.tasksModelRef || !menu.modelIndex) {
                return;
            }
            if (menu.isLauncher) {
                menu.tasksModelRef.requestActivate(menu.modelIndex);
            } else {
                menu.tasksModelRef.requestNewInstance(menu.modelIndex);
            }
        }
    }

    // Mute / Unmute Audio Stream
    PlasmaExtras.MenuItem {
        visible: menu.hasAudioStream && Boolean(menu.audioTrackerRef)
        icon: menu.isAudioMuted ? "audio-volume-high" : "audio-volume-muted"
        text: menu.isAudioMuted ? i18n("Unmute") : i18n("Mute")
        onClicked: {
            if (menu.audioTrackerRef) {
                menu.audioTrackerRef.toggleMuteForTask(menu.appPid, menu.appName, menu.appId);
            }
        }
    }

    // Pin / Unpin (single-activity direct toggle)
    PlasmaExtras.MenuItem {
        visible: menu.hasLauncherUrl && !menu.hasMultipleActivities
        icon: menu.isPinned ? "window-unpin" : "pin"
        text: menu.isPinned ? i18n("Unpin from Dock") : i18n("Pin to Dock")
        onClicked: {
            if (!menu.tasksModelRef || !menu.hasLauncherUrl) {
                return;
            }
            if (menu.isPinned) {
                menu.tasksModelRef.requestRemoveLauncher(menu.launcherUrl);
            } else {
                const urlToAdd = menu.launcherUrlFull.toString().length > 0
                    ? menu.launcherUrlFull
                    : menu.launcherUrl;
                menu.tasksModelRef.requestAddLauncher(urlToAdd);
            }
        }
    }

    // Per-Activity Pinning Submenu (when multiple KDE Activities are running)
    PlasmaExtras.MenuItem {
        id: activityPinMenuItem
        visible: menu.hasLauncherUrl && menu.hasMultipleActivities
        icon: menu.isPinned ? "window-unpin" : "pin"
        text: i18n("Pin to Dock")

        PlasmaExtras.Menu {
            id: activityPinSubMenu
            visualParent: activityPinMenuItem.action
        }
    }

    // Window Management Section
    PlasmaExtras.MenuItem {
        separator: true
        visible: menu.isWindow || menu.isGroupParent
    }

    PlasmaExtras.MenuItem {
        visible: menu.isWindow && !menu.isGroupParent && menu.isMinimizable
        icon: menu.isMinimized ? "window-restore" : "window-minimize"
        text: menu.isMinimized ? i18n("Unminimize") : i18n("Minimize")
        onClicked: {
            if (menu.tasksModelRef && menu.modelIndex) {
                menu.tasksModelRef.requestToggleMinimized(menu.modelIndex);
            }
        }
    }

    PlasmaExtras.MenuItem {
        visible: menu.isWindow && !menu.isGroupParent && menu.isMaximizable
        icon: menu.isMaximized ? "window-restore" : "window-maximize"
        text: menu.isMaximized ? i18n("Restore") : i18n("Maximize")
        onClicked: {
            if (menu.tasksModelRef && menu.modelIndex) {
                menu.tasksModelRef.requestToggleMaximized(menu.modelIndex);
            }
        }
    }

    // Move to Virtual Desktop submenu
    PlasmaExtras.MenuItem {
        id: desktopMenuItem
        visible: menu.isWindow && !menu.isGroupParent && Boolean(menu.virtualDesktopInfoRef && menu.virtualDesktopInfoRef.numberOfDesktops > 1)
        icon: "virtual-desktops"
        text: i18n("Move to Desktop")

        PlasmaExtras.Menu {
            id: desktopSubMenu
            visualParent: desktopMenuItem.action
        }
    }

    // More Actions submenu
    PlasmaExtras.MenuItem {
        id: moreActionsItem
        visible: menu.isWindow && !menu.isGroupParent
        icon: "overflow-menu"
        text: i18n("More")

        PlasmaExtras.Menu {
            visualParent: moreActionsItem.action

            PlasmaExtras.MenuItem {
                visible: menu.isMovable
                icon: "transform-move"
                text: i18n("Move")
                onClicked: menu.tasksModelRef.requestMove(menu.modelIndex)
            }
            PlasmaExtras.MenuItem {
                visible: menu.isResizable
                icon: "transform-scale"
                text: i18n("Resize")
                onClicked: menu.tasksModelRef.requestResize(menu.modelIndex)
            }
            PlasmaExtras.MenuItem {
                separator: true
            }
            PlasmaExtras.MenuItem {
                checkable: true
                checked: menu.isKeepAbove
                icon: "window-keep-above"
                text: i18n("Keep Above Others")
                onClicked: menu.tasksModelRef.requestToggleKeepAbove(menu.modelIndex)
            }
            PlasmaExtras.MenuItem {
                checkable: true
                checked: menu.isKeepBelow
                icon: "window-keep-below"
                text: i18n("Keep Below Others")
                onClicked: menu.tasksModelRef.requestToggleKeepBelow(menu.modelIndex)
            }
            PlasmaExtras.MenuItem {
                visible: menu.isFullScreenable
                checkable: true
                checked: menu.isFullScreen
                icon: "view-fullscreen"
                text: i18n("Fullscreen")
                onClicked: menu.tasksModelRef.requestToggleFullScreen(menu.modelIndex)
            }
        }
    }

    PlasmaExtras.MenuItem {
        separator: true
        visible: (menu.isWindow || menu.isGroupParent) && menu.isClosable
    }

    PlasmaExtras.MenuItem {
        visible: (menu.isWindow || menu.isGroupParent) && menu.isClosable
        icon: "window-close"
        text: menu.isGroupParent ? i18n("Close All") : i18n("Close")
        onClicked: {
            if (menu.tasksModelRef && menu.modelIndex) {
                menu.tasksModelRef.requestClose(menu.modelIndex);
            }
        }
    }
}
