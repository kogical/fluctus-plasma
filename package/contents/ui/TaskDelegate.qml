import QtQuick
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
    property int smartLauncherRevision: 0

    readonly property bool isHorizontalDock: dockEdge === 0 || dockEdge === 1
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
        anchors.left: iconItem.left
        anchors.right: iconItem.right
        anchors.top: iconItem.bottom
        height: iconItem.height * 0.38
        clip: true
        opacity: 0.24

        Kirigami.Icon {
            width: iconItem.width
            height: iconItem.height
            source: delegateRoot.decorationIcon
            smooth: true
            transform: Scale {
                origin.y: iconItem.height * 0.5
                yScale: -1
            }
        }
    }

    Kirigami.Icon {
        id: iconItem
        anchors.fill: parent
        anchors.margins: 2
        source: delegateRoot.decorationIcon
        active: mouseArea.containsMouse
        smooth: true
        opacity: mouseArea.draggedOutsideToUnpin ? 0.32 : (mouseArea.isDragging ? 0.72 : 1.0)
        layer.enabled: delegateRoot.enableMipmap
        layer.mipmap: delegateRoot.enableMipmap
        layer.smooth: true

        Behavior on opacity { NumberAnimation { duration: 110 } }
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

    // SmartLauncher progress bar (bottom of icon)
    Rectangle {
        visible: delegateRoot.smartProgressVisible
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.bottomMargin: 8
        height: 4
        radius: 2
        color: Qt.rgba(0, 0, 0, 0.55)

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.max(0, Math.min(parent.width, parent.width * delegateRoot.smartProgress))
            radius: 2
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

        readonly property real edgeInset: delegateRoot.indicatorPosition === 1 ? 4 : 0
        readonly property real primaryLen: {
            const s = delegateRoot.delegateLogicalSize;
            switch (delegateRoot.indicatorStyle) {
            case 1: // Classic Dot
                return Math.max(6, s * 0.10);
            case 2: // Glowing Underline
                return delegateRoot.isActive ? Math.max(18, s * 0.42) : Math.max(10, s * 0.22);
            case 3: // Minimal Line
                return delegateRoot.isActive ? Math.max(14, s * 0.30) : Math.max(8, s * 0.16);
            case 0: // Pill Bar (Default)
            default:
                return delegateRoot.isActive ? Math.max(12, s * 0.24) : Math.max(6, s * 0.11);
            }
        }
        readonly property real crossLen: {
            const s = delegateRoot.delegateLogicalSize;
            switch (delegateRoot.indicatorStyle) {
            case 1: // Classic Dot
                return Math.max(6, s * 0.10);
            case 2: // Glowing Underline
                return Math.max(4, s * 0.065);
            case 3: // Minimal Line
                return 2.5;
            case 0: // Pill Bar
            default:
                return Math.max(5, s * 0.08);
            }
        }
        readonly property color indicatorColor: delegateRoot.isDemandingAttention
            ? Kirigami.Theme.negativeTextColor
            : (delegateRoot.isActive ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor)

        width: delegateRoot.isHorizontalDock ? primaryLen : crossLen
        height: delegateRoot.isHorizontalDock ? crossLen : primaryLen

        anchors.horizontalCenter: delegateRoot.isHorizontalDock ? parent.horizontalCenter : undefined
        anchors.verticalCenter: !delegateRoot.isHorizontalDock ? parent.verticalCenter : undefined
        anchors.bottom: delegateRoot.dockEdge === 0 ? parent.bottom : undefined
        anchors.top: delegateRoot.dockEdge === 1 ? parent.top : undefined
        anchors.left: delegateRoot.dockEdge === 2 ? parent.left : undefined
        anchors.right: delegateRoot.dockEdge === 3 ? parent.right : undefined
        anchors.bottomMargin: delegateRoot.dockEdge === 0 ? edgeInset : 0
        anchors.topMargin: delegateRoot.dockEdge === 1 ? edgeInset : 0
        anchors.leftMargin: delegateRoot.dockEdge === 2 ? edgeInset : 0
        anchors.rightMargin: delegateRoot.dockEdge === 3 ? edgeInset : 0

        // Soft accent glow for Glowing Underline style
        Rectangle {
            visible: delegateRoot.indicatorStyle === 2
            anchors.centerIn: parent
            width: parent.width + 6
            height: parent.height + 4
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
        visible: delegateRoot.smartCountVisible || (delegateRoot.isGroupParent && delegateRoot.childCount > 1)
        anchors.top: parent.top
        anchors.right: parent.right
        width: Math.max(20, countLabel.implicitWidth + 10)
        height: 20
        radius: 10
        color: delegateRoot.smartCountVisible ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.highlightColor

        Text {
            id: countLabel
            anchors.centerIn: parent
            text: delegateRoot.smartCountVisible
                ? (delegateRoot.smartCount > 99 ? "99+" : String(delegateRoot.smartCount))
                : String(delegateRoot.childCount)
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }
    }

    // Timed shortcut index badge (shown briefly after Meta+1..9)
    Rectangle {
        visible: delegateRoot.showIndexBadge && delegateRoot.taskIndex >= 0 && delegateRoot.taskIndex < 9
        anchors.top: parent.top
        anchors.left: parent.left
        width: 22
        height: 22
        radius: 6
        color: Qt.rgba(0, 0, 0, 0.78)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.35)
        z: 12

        Text {
            anchors.centerIn: parent
            text: String(delegateRoot.taskIndex + 1)
            color: "#ffffff"
            font.pixelSize: 13
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
                    const rawTarget = delegateRoot.dockLayoutRef.restingInsertionIndex(pt.x, pt.y);
                    const maxIdx = Math.max(0, delegateRoot.tasksModelRef.count - 1);
                    const targetIdx = Math.max(0, Math.min(rawTarget, maxIdx));
                    if (delegateRoot.rootRef) {
                        delegateRoot.rootRef.activeDropIndex = rawTarget;
                    }
                    if (delegateRoot.taskIndex >= 0 && targetIdx !== delegateRoot.taskIndex) {
                        delegateRoot.tasksModelRef.move(delegateRoot.taskIndex, targetIdx);
                        movedDuringDrag = true;
                    }
                }
            }

            onReleased: function(mouse) {
                if (mouse.button === Qt.LeftButton && isDragging) {
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
            }

            onClicked: function(mouse) {
                if (isDragging || movedDuringDrag) {
                    return;
                }
                if (!delegateRoot.tasksModelRef || !delegateRoot.modelIndex) {
                    return;
                }
                if (mouse.button === Qt.RightButton) {
                    return;
                }
                if (mouse.button === Qt.MiddleButton) {
                    delegateRoot.tasksModelRef.requestNewInstance(delegateRoot.modelIndex);
                    return;
                }
                if (delegateRoot.isGroupParent && delegateRoot.childCount > 1 && delegateRoot.groupDialogRef) {
                    delegateRoot.groupDialogRef.openForDelegate(delegateRoot);
                    return;
                }
                if (delegateRoot.isLauncher) {
                    delegateRoot.tasksModelRef.requestActivate(delegateRoot.modelIndex);
                } else if (delegateRoot.isActive && delegateRoot.minimizeActiveOnClick) {
                    delegateRoot.tasksModelRef.requestToggleMinimized(delegateRoot.modelIndex);
                } else {
                    delegateRoot.tasksModelRef.requestActivate(delegateRoot.modelIndex);
                }
            }

            onWheel: function(wheel) {
                if (!delegateRoot.tasksModelRef || !delegateRoot.modelIndex || delegateRoot.isLauncher) {
                    return;
                }
                if (wheel.angleDelta.y > 0) {
                    delegateRoot.tasksModelRef.requestActivate(delegateRoot.modelIndex);
                } else if (wheel.angleDelta.y < 0 && delegateRoot.isActive && delegateRoot.isMinimizable) {
                    delegateRoot.tasksModelRef.requestToggleMinimized(delegateRoot.modelIndex);
                }
            }
        }
    }

    // Clickable Audio / Mute Indicator Badge (top-left, above MouseArea so clicks toggle mute directly)
    Rectangle {
        id: audioBadge
        visible: delegateRoot.hasAudioStream && !delegateRoot.showIndexBadge
        anchors.top: parent.top
        anchors.left: parent.left
        width: 22
        height: 22
        radius: 11
        color: delegateRoot.isAudioMuted
            ? Kirigami.Theme.negativeTextColor
            : Qt.rgba(Kirigami.Theme.backgroundColor.r, Kirigami.Theme.backgroundColor.g, Kirigami.Theme.backgroundColor.b, 0.88)
        border.width: 1
        border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.25)
        z: 15

        Kirigami.Icon {
            anchors.centerIn: parent
            width: 14
            height: 14
            source: delegateRoot.isAudioMuted ? "audio-volume-muted" : "audio-volume-high"
            color: delegateRoot.isAudioMuted ? "#ffffff" : Kirigami.Theme.textColor
        }

        MouseArea {
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
