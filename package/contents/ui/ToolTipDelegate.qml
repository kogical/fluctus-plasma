import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.pipewire as PipeWire
import org.kde.taskmanager as TaskManager
import org.kde.plasma.private.mpris as Mpris

// Rich Plasma tooltip with PipeWire live window preview and MPRIS media controls.
Item {
    id: toolTipRoot

    property var tasksModelRef
    property var modelIndex
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
    readonly property bool canShowThumbnail: showPreview && isWindow && !isGroupParent && primaryWinId.length > 0

    Mpris.Mpris2Model {
        id: mprisModel
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

    implicitWidth: Math.max(200, Math.min(300, mainColumn.implicitWidth + Kirigami.Units.largeSpacing * 2))
    implicitHeight: mainColumn.implicitHeight + Kirigami.Units.largeSpacing * 2

    ColumnLayout {
        id: mainColumn
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        // Header: icon + title + close button
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
                    visible: toolTipRoot.displayTitle.length > 0 && toolTipRoot.displayTitle !== toolTipRoot.appName
                    text: toolTipRoot.isGroupParent
                        ? i18np("%1 window open", "%1 windows open", toolTipRoot.childCount)
                        : toolTipRoot.displayTitle
                    opacity: 0.75
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }

            PlasmaComponents3.ToolButton {
                visible: toolTipRoot.isWindow || toolTipRoot.isGroupParent
                icon.name: "window-close"
                display: PlasmaComponents3.AbstractButton.IconOnly
                implicitWidth: Kirigami.Units.iconSizes.smallMedium + 6
                implicitHeight: Kirigami.Units.iconSizes.smallMedium + 6
                onClicked: {
                    if (toolTipRoot.tasksModelRef && toolTipRoot.modelIndex) {
                        toolTipRoot.tasksModelRef.requestClose(toolTipRoot.modelIndex);
                    }
                }
            }
        }

        // Live PipeWire Window Preview
        Item {
            id: thumbnailContainer
            visible: toolTipRoot.canShowThumbnail
            Layout.fillWidth: true
            Layout.preferredWidth: 240
            Layout.preferredHeight: 140
            clip: true

            TaskManager.ScreencastingRequest {
                id: screencastRequest
                uuid: toolTipRoot.visible && toolTipRoot.canShowThumbnail ? toolTipRoot.primaryWinId : ""
            }

            PipeWire.PipeWireSourceItem {
                id: pipeWireItem
                anchors.fill: parent
                nodeId: screencastRequest.nodeId
                visible: screencastRequest.nodeId > 0
            }

            MouseArea {
                anchors.fill: parent
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
