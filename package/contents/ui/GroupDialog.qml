import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents3
import org.kde.taskmanager as TaskManager

// Popup window selector for grouped applications (childCount > 1).
PlasmaCore.Dialog {
    id: groupDialog

    property var tasksModelRef
    property var parentModelIndex
    property string groupTitle: ""

    type: PlasmaCore.Dialog.PopupMenu
    flags: Qt.WindowStaysOnTopHint
    hideOnWindowDeactivate: true
    location: PlasmaCore.Types.Floating

    function openForDelegate(delegateItem) {
        if (!delegateItem || !tasksModelRef) {
            return;
        }
        visualParent = delegateItem;
        parentModelIndex = delegateItem.modelIndex;
        groupTitle = delegateItem.appName || delegateItem.displayTitle;
        visible = true;
        requestActivate();
    }

    mainItem: ColumnLayout {
        implicitWidth: 280
        spacing: Kirigami.Units.smallSpacing

        PlasmaComponents3.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            Layout.topMargin: Kirigami.Units.smallSpacing
            text: groupDialog.groupTitle
            font.bold: true
            elide: Text.ElideRight
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        ListView {
            id: windowList
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 320)
            clip: true
            spacing: 2

            model: DelegateModel {
                id: childDelegateModel
                model: groupDialog.tasksModelRef
                rootIndex: groupDialog.parentModelIndex

                delegate: PlasmaComponents3.ItemDelegate {
                    width: windowList.width
                    highlighted: Boolean(model.IsActive)

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                            source: model.decoration
                        }

                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            text: model.display || ""
                            elide: Text.ElideRight
                            opacity: model.IsMinimized ? 0.6 : 1.0
                        }

                        PlasmaComponents3.ToolButton {
                            icon.name: "window-close"
                            display: PlasmaComponents3.AbstractButton.IconOnly
                            onClicked: {
                                const idx = childDelegateModel.modelIndex(index);
                                groupDialog.tasksModelRef.requestClose(idx);
                            }
                        }
                    }

                    onClicked: {
                        const idx = childDelegateModel.modelIndex(index);
                        groupDialog.tasksModelRef.requestActivate(idx);
                        groupDialog.visible = false;
                    }
                }
            }
        }
    }
}
