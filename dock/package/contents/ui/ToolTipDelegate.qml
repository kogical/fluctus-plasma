import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.pipewire as PipeWire
import org.kde.taskmanager as TaskManager
import org.kde.plasma.private.mpris as Mpris

// Rich Plasma tooltip with live PipeWire window previews (single window OR grouped windows)
// and MPRIS media controls.
Item {
    id: toolTipRoot

    property var tasksModelRef
    property var modelIndex
    /// Top-level TasksModel row (used to read a group's windows fresh).
    property int taskIndex: -1
    property string displayTitle: ""
    property string appName: ""
    property var decorationIcon
    property bool isWindow: false
    property bool isGroupParent: false
    property int childCount: 0
    property var winIdList: []
    property string appId: ""
    property bool showPreview: true

    readonly property string primaryWinId: (winIdList && winIdList.length > 0) ? String(winIdList[0]) : ""
    readonly property bool isGroup: isGroupParent && childCount > 1
    readonly property bool canShowSingleThumbnail: showPreview && isWindow && !isGroup && primaryWinId.length > 0
    readonly property bool canShowGroupCards: isGroup && Boolean(tasksModelRef) && taskIndex >= 0
    readonly property bool showGroupCards: canShowGroupCards && showPreview && childCount <= 4

    function cleanWindowTitle(rawTitle) {
        const title = String(rawTitle || "").trim();
        const app = String(appName || "").trim();
        if (!title) {
            return app || displayTitle;
        }
        if (app.length > 0) {
            const suffixes = [" — " + app, " - " + app, " – " + app];
            for (let i = 0; i < suffixes.length; ++i) {
                if (title.endsWith(suffixes[i]) && title.length > suffixes[i].length) {
                    return title.slice(0, title.length - suffixes[i].length).trim();
                }
            }
        }
        return title;
    }

    Mpris.Mpris2Model {
        id: mprisModel
    }

    TaskGroupWindows {
        id: groupChildModel
        tasksModel: toolTipRoot.tasksModelRef
        row: toolTipRoot.taskIndex
        enabled: toolTipRoot.canShowGroupCards
    }

    readonly property int matchedPlayerIndex: {
        if (!appId || mprisModel.count === 0) {
            return -1;
        }
        const needle = appId.toLowerCase().replace(/\.desktop$/, "");
        for (let i = 0; i < mprisModel.count; ++i) {
            const idx = mprisModel.index(i, 0);
            const desktop = String(mprisModel.data(idx, Mpris.Mpris2Model.DesktopEntryRole) || "").toLowerCase();
            if (desktop && (desktop === needle || needle.indexOf(desktop) >= 0 || desktop.indexOf(needle) >= 0)) {
                return i;
            }
        }
        return -1;
    }

    readonly property bool hasMpris: matchedPlayerIndex >= 0
    readonly property var mprisIndex: hasMpris ? mprisModel.index(matchedPlayerIndex, 0) : null
    readonly property string mprisTrack: hasMpris ? String(mprisModel.data(mprisIndex, Mpris.Mpris2Model.TrackRole) || "") : ""
    readonly property string mprisArtist: hasMpris ? String(mprisModel.data(mprisIndex, Mpris.Mpris2Model.ArtistRole) || "") : ""
    readonly property bool mprisPlaying: hasMpris && mprisModel.data(mprisIndex, Mpris.Mpris2Model.PlaybackStatusRole) === Mpris.PlaybackStatus.Playing

    implicitWidth: Math.max(210, Math.min(toolTipRoot.canShowGroupCards ? 960 : 310, mainColumn.implicitWidth + Kirigami.Units.largeSpacing * 2))
    implicitHeight: mainColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
    // The tooltip dialog follows these (not the implicit size) after it first appears, so
    // pinning them lets it grow when the window previews of a group arrive.
    Layout.minimumWidth: implicitWidth
    Layout.maximumWidth: implicitWidth
    Layout.minimumHeight: implicitHeight
    Layout.maximumHeight: implicitHeight

    ColumnLayout {
        id: mainColumn
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        // Header: icon + title + subtitle / count + close button
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                source: toolTipRoot.decorationIcon
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: toolTipRoot.appName.length > 0 ? toolTipRoot.appName : toolTipRoot.displayTitle
                    font.bold: true
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    visible: toolTipRoot.isGroup
                        || (toolTipRoot.displayTitle.length > 0 && toolTipRoot.displayTitle !== toolTipRoot.appName)
                    text: toolTipRoot.isGroup
                        ? i18np("%1 window open", "%1 windows open", toolTipRoot.childCount)
                        : toolTipRoot.cleanWindowTitle(toolTipRoot.displayTitle)
                    opacity: 0.72
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }

            PlasmaComponents3.ToolButton {
                visible: (toolTipRoot.isWindow || toolTipRoot.isGroupParent) && !toolTipRoot.canShowGroupCards
                icon.name: "window-close-symbolic"
                display: PlasmaComponents3.AbstractButton.IconOnly
                flat: true
                implicitWidth: Kirigami.Units.iconSizes.smallMedium + 6
                implicitHeight: Kirigami.Units.iconSizes.smallMedium + 6
                onClicked: {
                    if (toolTipRoot.tasksModelRef && toolTipRoot.modelIndex) {
                        toolTipRoot.tasksModelRef.requestClose(toolTipRoot.modelIndex);
                    }
                }
            }
        }

        // Grouped windows: side-by-side live PipeWire preview cards (<= 4) or compact rows
        RowLayout {
            visible: toolTipRoot.showGroupCards
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: toolTipRoot.showGroupCards ? groupChildModel.count : 0

                delegate: Rectangle {
                    id: groupCard
                    required property int index
                    readonly property var model: groupChildModel.windows[index] || ({})

                    readonly property string winUuid: (model.WinIdList && model.WinIdList.length > 0)
                        ? String(model.WinIdList[0])
                        : ""
                    readonly property bool isActiveWin: Boolean(model.IsActive)
                    readonly property bool isMinWin: Boolean(model.IsMinimized)

                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 146
                    radius: Kirigami.Units.cornerRadius
                    color: groupCardHover.hovered
                        ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.16)
                        : (isActiveWin
                            ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
                            : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.03))
                    border.width: isActiveWin || groupCardHover.hovered ? 1 : 0
                    border.color: isActiveWin
                        ? Kirigami.Theme.highlightColor
                        : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.25)

                    HoverHandler {
                        id: groupCardHover
                    }

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onTapped: {
                            const idx = groupChildModel.modelIndex(groupCard.index);
                            toolTipRoot.tasksModelRef.requestActivate(idx);
                        }
                    }

                    TapHandler {
                        acceptedButtons: Qt.MiddleButton
                        onTapped: {
                            const idx = groupChildModel.modelIndex(groupCard.index);
                            toolTipRoot.tasksModelRef.requestClose(idx);
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            PlasmaComponents3.Label {
                                Layout.fillWidth: true
                                text: toolTipRoot.cleanWindowTitle(groupCard.model.display)
                                font.bold: groupCard.isActiveWin
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                opacity: groupCard.isMinWin ? 0.6 : 1.0
                            }

                            PlasmaComponents3.ToolButton {
                                id: groupCardClose
                                icon.name: "window-close-symbolic"
                                display: PlasmaComponents3.AbstractButton.IconOnly
                                flat: true
                                implicitWidth: 20
                                implicitHeight: 20
                                opacity: (groupCardHover.hovered || groupCardClose.hovered) ? 1.0 : 0.45
                                Behavior on opacity { NumberAnimation { duration: 100 } }
                                onClicked: {
                                    const idx = groupChildModel.modelIndex(groupCard.index);
                                    toolTipRoot.tasksModelRef.requestClose(idx);
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: Math.max(2, Kirigami.Units.cornerRadius - 2)
                            color: Qt.rgba(0, 0, 0, 0.28)
                            clip: true

                            Loader {
                                id: groupPipeWireLoader
                                anchors.fill: parent
                                active: toolTipRoot.Window.window !== null
                                    && toolTipRoot.Window.window.visible
                                    && toolTipRoot.showPreview
                                    && groupCard.winUuid.length > 0
                                sourceComponent: PipeWire.PipeWireSourceItem {
                                    anchors.fill: parent
                                    nodeId: groupScreencast.nodeId

                                    TaskManager.ScreencastingRequest {
                                        id: groupScreencast
                                        uuid: groupCard.winUuid
                                    }
                                }
                            }

                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: Kirigami.Units.iconSizes.large
                                height: width
                                source: groupCard.model.decoration
                                visible: !groupPipeWireLoader.item || !groupPipeWireLoader.item.ready
                                opacity: 0.75
                            }
                        }
                    }
                }
            }
        }

        // Grouped windows fallback list (when previews disabled or > 4 windows)
        ColumnLayout {
            visible: toolTipRoot.canShowGroupCards && !toolTipRoot.showGroupCards
            Layout.fillWidth: true
            spacing: 2

            Repeater {
                model: (toolTipRoot.canShowGroupCards && !toolTipRoot.showGroupCards) ? groupChildModel.count : 0

                delegate: PlasmaComponents3.ItemDelegate {
                    id: groupRow
                    required property int index
                    readonly property var model: groupChildModel.windows[index] || ({})

                    Layout.fillWidth: true
                    highlighted: Boolean(model.IsActive)
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing
                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            text: toolTipRoot.cleanWindowTitle(groupRow.model.display)
                            font.bold: Boolean(groupRow.model.IsActive)
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            opacity: groupRow.model.IsMinimized ? 0.6 : 1.0
                        }
                        PlasmaComponents3.ToolButton {
                            icon.name: "window-close-symbolic"
                            display: PlasmaComponents3.AbstractButton.IconOnly
                            flat: true
                            implicitWidth: 20
                            implicitHeight: 20
                            onClicked: {
                                const idx = groupChildModel.modelIndex(groupRow.index);
                                toolTipRoot.tasksModelRef.requestClose(idx);
                            }
                        }
                    }
                    onClicked: {
                        const idx = groupChildModel.modelIndex(groupRow.index);
                        toolTipRoot.tasksModelRef.requestActivate(idx);
                    }
                }
            }
        }

        // Single-window live PipeWire preview
        Rectangle {
            id: thumbnailContainer
            visible: toolTipRoot.canShowSingleThumbnail
            Layout.fillWidth: true
            Layout.preferredWidth: 240
            Layout.preferredHeight: 140
            radius: Math.max(2, Kirigami.Units.cornerRadius - 2)
            color: Qt.rgba(0, 0, 0, 0.28)
            clip: true

            Loader {
                id: singlePipeWireLoader
                anchors.fill: parent
                active: toolTipRoot.Window.window !== null
                    && toolTipRoot.Window.window.visible
                    && toolTipRoot.canShowSingleThumbnail
                sourceComponent: PipeWire.PipeWireSourceItem {
                    anchors.fill: parent
                    nodeId: screencastRequest.nodeId

                    TaskManager.ScreencastingRequest {
                        id: screencastRequest
                        uuid: toolTipRoot.primaryWinId
                    }
                }
            }

            Kirigami.Icon {
                anchors.centerIn: parent
                width: Kirigami.Units.iconSizes.large
                height: width
                source: toolTipRoot.decorationIcon
                visible: !singlePipeWireLoader.item || !singlePipeWireLoader.item.ready
                opacity: 0.75
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (toolTipRoot.tasksModelRef && toolTipRoot.modelIndex) {
                        toolTipRoot.tasksModelRef.requestActivate(toolTipRoot.modelIndex);
                    }
                }
            }
        }

        // MPRIS Media Controls (when app is an active media player)
        RowLayout {
            visible: toolTipRoot.hasMpris
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: toolTipRoot.mprisTrack
                    visible: text.length > 0
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    font.bold: true
                    elide: Text.ElideRight
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: toolTipRoot.mprisArtist
                    visible: text.length > 0
                    opacity: 0.72
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    elide: Text.ElideRight
                }
            }

            PlasmaComponents3.ToolButton {
                icon.name: "media-skip-backward"
                display: PlasmaComponents3.AbstractButton.IconOnly
                onClicked: {
                    const container = mprisModel.data(toolTipRoot.mprisIndex, Mpris.Mpris2Model.ContainerRole);
                    if (container) container.Previous();
                }
            }
            PlasmaComponents3.ToolButton {
                icon.name: toolTipRoot.mprisPlaying ? "media-playback-pause" : "media-playback-start"
                display: PlasmaComponents3.AbstractButton.IconOnly
                onClicked: {
                    const container = mprisModel.data(toolTipRoot.mprisIndex, Mpris.Mpris2Model.ContainerRole);
                    if (container) container.PlayPause();
                }
            }
            PlasmaComponents3.ToolButton {
                icon.name: "media-skip-forward"
                display: PlasmaComponents3.AbstractButton.IconOnly
                onClicked: {
                    const container = mprisModel.data(toolTipRoot.mprisIndex, Mpris.Mpris2Model.ContainerRole);
                    if (container) container.Next();
                }
            }
        }
    }
}
