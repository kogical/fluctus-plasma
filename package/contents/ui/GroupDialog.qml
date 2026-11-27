import QtQuick
import QtQuick.Layouts
import QtQml.Models
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.plasmoid
import org.kde.pipewire as PipeWire
import org.kde.taskmanager as TaskManager

// Popup window selector for grouped applications (childCount > 1).
// Shows live PipeWire preview cards when window previews are enabled (<= 5 windows),
// or a clean full-height vertical window list otherwise.
PlasmaCore.Dialog {
    id: groupDialog

    property var tasksModelRef
    property var parentModelIndex
    property string groupTitle: ""
    property var groupIcon
    property int groupCount: 0

    readonly property bool usePreviewCards: Plasmoid.configuration.showWindowPreviews
        && childDelegateModel.count > 0
        && childDelegateModel.count <= 5

    type: PlasmaCore.Dialog.PopupMenu
    flags: Qt.WindowStaysOnTopHint
    hideOnWindowDeactivate: true
    location: PlasmaCore.Types.Floating

    function openForDelegate(delegateItem) {
        if (!delegateItem || !tasksModelRef) {
            return;
        }
        visualParent = delegateItem;
        parentModelIndex = (delegateItem.taskIndex >= 0 && delegateItem.taskIndex < tasksModelRef.count)
            ? tasksModelRef.makeModelIndex(delegateItem.taskIndex)
            : delegateItem.modelIndex;
        groupTitle = delegateItem.appName || delegateItem.displayTitle;
        groupIcon = delegateItem.decorationIcon;
        groupCount = delegateItem.childCount || 0;
        visible = true;
        requestActivate();
        contentRoot.forceActiveFocus();
    }

    function cleanWindowTitle(rawTitle) {
        const title = String(rawTitle || "").trim();
        if (!title) {
            return groupTitle;
        }
        const suffixes = [" — " + groupTitle, " - " + groupTitle, " – " + groupTitle];
        for (let i = 0; i < suffixes.length; ++i) {
            if (groupTitle.length > 0 && title.endsWith(suffixes[i]) && title.length > suffixes[i].length) {
                return title.slice(0, title.length - suffixes[i].length).trim();
            }
        }
        return title;
    }

    readonly property DelegateModel _childModel: DelegateModel {
        id: childDelegateModel
        model: groupDialog.tasksModelRef
        rootIndex: groupDialog.parentModelIndex

        onCountChanged: {
            if (groupDialog.visible && count <= 1) {
                groupDialog.visible = false;
            }
        }
    }

    mainItem: Item {
        id: contentRoot
        implicitWidth: Math.max(280, mainColumn.implicitWidth + Kirigami.Units.largeSpacing * 2)
        implicitHeight: mainColumn.implicitHeight + Kirigami.Units.largeSpacing * 2

        Keys.onEscapePressed: groupDialog.visible = false

        ColumnLayout {
            id: mainColumn
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing

            // Header: app icon + app name + open window count
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                    source: groupDialog.groupIcon
                    visible: Boolean(groupDialog.groupIcon)
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: groupDialog.groupTitle
                    font.bold: true
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                PlasmaComponents3.Label {
                    text: i18np("%1 window", "%1 windows", childDelegateModel.count > 0 ? childDelegateModel.count : groupDialog.groupCount)
                    opacity: 0.65
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            // Preview card grid (when window previews are enabled and <= 5 windows)
            RowLayout {
                visible: groupDialog.usePreviewCards
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: groupDialog.usePreviewCards ? childDelegateModel : null

                    delegate: Rectangle {
                        id: cardDelegate
                        required property int index
                        required property var model

                        readonly property string winUuid: (model.WinIdList && model.WinIdList.length > 0)
                            ? String(model.WinIdList[0])
                            : ""
                        readonly property bool isActiveWin: Boolean(model.IsActive)
                        readonly property bool isMinWin: Boolean(model.IsMinimized)

                        Layout.preferredWidth: 210
                        Layout.preferredHeight: 158
                        radius: Kirigami.Units.cornerRadius
                        color: cardHover.hovered
                            ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.16)
                            : (isActiveWin
                                ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
                                : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.03))
                        border.width: isActiveWin || cardHover.hovered ? 1 : 0
                        border.color: isActiveWin
                            ? Kirigami.Theme.highlightColor
                            : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.25)

                        HoverHandler {
                            id: cardHover
                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: {
                                const idx = childDelegateModel.modelIndex(cardDelegate.index);
                                groupDialog.tasksModelRef.requestActivate(idx);
                                groupDialog.visible = false;
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.MiddleButton
                            onTapped: {
                                const idx = childDelegateModel.modelIndex(cardDelegate.index);
                                groupDialog.tasksModelRef.requestClose(idx);
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
                                    text: groupDialog.cleanWindowTitle(cardDelegate.model.display)
                                    font.bold: cardDelegate.isActiveWin
                                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                    opacity: cardDelegate.isMinWin ? 0.6 : 1.0
                                }

                                PlasmaComponents3.ToolButton {
                                    id: cardCloseBtn
                                    icon.name: "window-close-symbolic"
                                    display: PlasmaComponents3.AbstractButton.IconOnly
                                    flat: true
                                    implicitWidth: 22
                                    implicitHeight: 22
                                    opacity: (cardHover.hovered || cardCloseBtn.hovered) ? 1.0 : 0.45
                                    Behavior on opacity { NumberAnimation { duration: 100 } }
                                    onClicked: {
                                        const idx = childDelegateModel.modelIndex(cardDelegate.index);
                                        groupDialog.tasksModelRef.requestClose(idx);
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
                                    id: cardPipeWireLoader
                                    anchors.fill: parent
                                    active: groupDialog.visible
                                        && groupDialog.usePreviewCards
                                        && cardDelegate.winUuid.length > 0
                                    sourceComponent: PipeWire.PipeWireSourceItem {
                                        anchors.fill: parent
                                        nodeId: cardScreencast.nodeId

                                        TaskManager.ScreencastingRequest {
                                            id: cardScreencast
                                            uuid: cardDelegate.winUuid
                                        }
                                    }
                                }

                                Kirigami.Icon {
                                    anchors.centerIn: parent
                                    width: Kirigami.Units.iconSizes.large
                                    height: width
                                    source: cardDelegate.model.decoration
                                    visible: !cardPipeWireLoader.item || !cardPipeWireLoader.item.ready
                                    opacity: 0.75
                                }
                            }
                        }
                    }
                }
            }

            // Full-height vertical window list (fallback when previews disabled or > 5 windows)
            ColumnLayout {
                visible: !groupDialog.usePreviewCards
                Layout.fillWidth: true
                Layout.minimumWidth: 300
                spacing: 2

                Repeater {
                    model: !groupDialog.usePreviewCards ? childDelegateModel : null

                    delegate: PlasmaComponents3.ItemDelegate {
                        id: rowDelegate
                        required property int index
                        required property var model

                        Layout.fillWidth: true
                        implicitHeight: Math.max(36, rowLayout.implicitHeight + Kirigami.Units.smallSpacing * 2)
                        highlighted: Boolean(model.IsActive)
                        hoverEnabled: true

                        Rectangle {
                            visible: Boolean(rowDelegate.model.IsActive)
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3
                            height: parent.height * 0.55
                            radius: 1.5
                            color: Kirigami.Theme.highlightColor
                        }

                        contentItem: RowLayout {
                            id: rowLayout
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                Layout.leftMargin: 2
                                Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                                source: rowDelegate.model.decoration
                                opacity: rowDelegate.model.IsMinimized ? 0.55 : 1.0
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                PlasmaComponents3.Label {
                                    Layout.fillWidth: true
                                    text: groupDialog.cleanWindowTitle(rowDelegate.model.display)
                                    font.bold: Boolean(rowDelegate.model.IsActive)
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                    opacity: rowDelegate.model.IsMinimized ? 0.6 : 1.0
                                }

                                PlasmaComponents3.Label {
                                    Layout.fillWidth: true
                                    visible: Boolean(rowDelegate.model.IsMinimized)
                                    text: i18n("Minimized")
                                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                                    opacity: 0.55
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }

                            PlasmaComponents3.ToolButton {
                                id: closeBtn
                                icon.name: "window-close-symbolic"
                                display: PlasmaComponents3.AbstractButton.IconOnly
                                flat: true
                                implicitWidth: Kirigami.Units.iconSizes.smallMedium + 6
                                implicitHeight: Kirigami.Units.iconSizes.smallMedium + 6
                                opacity: (rowDelegate.hovered || closeBtn.hovered) ? 1.0 : 0.45
                                Behavior on opacity { NumberAnimation { duration: 100 } }
                                onClicked: {
                                    const idx = childDelegateModel.modelIndex(rowDelegate.index);
                                    groupDialog.tasksModelRef.requestClose(idx);
                                }
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.MiddleButton
                            onTapped: {
                                const idx = childDelegateModel.modelIndex(rowDelegate.index);
                                groupDialog.tasksModelRef.requestClose(idx);
                            }
                        }

                        onClicked: {
                            const idx = childDelegateModel.modelIndex(rowDelegate.index);
                            groupDialog.tasksModelRef.requestActivate(idx);
                            groupDialog.visible = false;
                        }
                    }
                }
            }
        }
    }
}
