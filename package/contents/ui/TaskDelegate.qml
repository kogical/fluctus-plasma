import QtQuick
import org.kde.kirigami as Kirigami

// Invariant: NEVER bind x, y, width, height, or scale here.
// DockLayoutItem owns delegate geometry and drives position + scale directly from C++.
Item {
    id: delegateRoot

    // Properties populated and kept in sync by C++ DockLayoutItem
    property int taskIndex: -1
    property var modelIndex
    property string displayTitle: ""
    property var decorationIcon
    property bool isActive: false
    property bool isLauncher: false
    property bool isWindow: false
    property bool isStartup: false
    property bool isDemandingAttention: false
    property bool isMinimized: false
    property bool isGroupParent: false
    property int childCount: 0
    property string appId: ""
    property url launcherUrl
    property int dockEdge: 0
    property real delegateLogicalSize: 87
    property real slotScale: 1.0
    property real bounceOffset: 0.0

    // Properties passed from main.qml
    property var tasksModelRef
    property bool showIndexBadge: false
    property bool minimizeActiveOnClick: true
    property bool enableMipmap: false

    Kirigami.Icon {
        id: iconItem
        anchors.fill: parent
        anchors.margins: 2
        source: delegateRoot.decorationIcon
        active: mouseArea.containsMouse
        smooth: true
        layer.enabled: delegateRoot.enableMipmap
        layer.mipmap: delegateRoot.enableMipmap
        layer.smooth: true
    }

    // Running / active / attention indicator dot anchored to the screen-edge side
    Rectangle {
        id: runningDot
        visible: delegateRoot.isWindow || delegateRoot.isGroupParent || delegateRoot.isDemandingAttention
        width: delegateRoot.isActive ? Math.max(10, delegateRoot.delegateLogicalSize * 0.22) : Math.max(6, delegateRoot.delegateLogicalSize * 0.11)
        height: Math.max(5, delegateRoot.delegateLogicalSize * 0.08)
        radius: height * 0.5
        color: delegateRoot.isDemandingAttention
            ? Kirigami.Theme.negativeTextColor
            : (delegateRoot.isActive ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor)
        opacity: delegateRoot.isMinimized ? 0.45 : 0.95

        anchors.horizontalCenter: (delegateRoot.dockEdge === 0 || delegateRoot.dockEdge === 1) ? parent.horizontalCenter : undefined
        anchors.verticalCenter: (delegateRoot.dockEdge === 2 || delegateRoot.dockEdge === 3) ? parent.verticalCenter : undefined
        anchors.bottom: delegateRoot.dockEdge === 0 ? parent.bottom : undefined
        anchors.top: delegateRoot.dockEdge === 1 ? parent.top : undefined
        anchors.left: delegateRoot.dockEdge === 2 ? parent.left : undefined
        anchors.right: delegateRoot.dockEdge === 3 ? parent.right : undefined
    }

    // Group count pill badge
    Rectangle {
        visible: delegateRoot.isGroupParent && delegateRoot.childCount > 1
        anchors.top: parent.top
        anchors.right: parent.right
        width: Math.max(20, countLabel.implicitWidth + 10)
        height: 20
        radius: 10
        color: Kirigami.Theme.highlightColor

        Text {
            id: countLabel
            anchors.centerIn: parent
            text: String(delegateRoot.childCount)
            color: Kirigami.Theme.highlightedTextColor
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

        Text {
            anchors.centerIn: parent
            text: String(delegateRoot.taskIndex + 1)
            color: "#ffffff"
            font.pixelSize: 13
            font.bold: true
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton

        onClicked: function(mouse) {
            if (!delegateRoot.tasksModelRef || !delegateRoot.modelIndex) {
                return;
            }
            if (mouse.button === Qt.MiddleButton) {
                delegateRoot.tasksModelRef.requestNewInstance(delegateRoot.modelIndex);
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
    }
}
