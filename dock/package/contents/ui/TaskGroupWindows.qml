import QtQuick
import org.kde.taskmanager as TaskManager

// The windows of one grouped task, read straight from TasksModel by row.
//
// A DelegateModel rooted at the group's QModelIndex goes stale whenever TasksModel regroups or
// re-sorts (its rootIndex is not re-resolved), which left tooltips and the group picker empty.
// Here the parent index is rebuilt from the row on every model change instead.
//
// Use `count` as a Repeater model and read `windows[index]` in the delegate: the Repeater then
// only rebuilds when windows come or go, so live previews are not restarted on every title
// change. Each entry mirrors the role names a DelegateModel would expose (display, decoration,
// IsActive, IsMinimized, WinIdList, LastActivated).
QtObject {
    id: group

    property var tasksModel
    /// Top-level TasksModel row of the group parent.
    property int row: -1
    /// Only watch the model while someone is looking (tooltip or picker open).
    property bool enabled: true

    property int _revision: 0
    readonly property var windows: _read(_revision, tasksModel, row, enabled)
    readonly property int count: windows.length

    /// Fresh QModelIndex of the child at `child`, for requestActivate / requestClose.
    function modelIndex(child) {
        return tasksModel.makeModelIndex(row, child);
    }

    function _read() {
        if (!enabled || !tasksModel || row < 0 || row >= tasksModel.count) {
            return [];
        }
        const parent = tasksModel.makeModelIndex(row);
        if (!tasksModel.data(parent, TaskManager.AbstractTasksModel.IsGroupParent)) {
            return [];
        }
        const result = [];
        const children = tasksModel.rowCount(parent);
        for (let child = 0; child < children; ++child) {
            const idx = tasksModel.makeModelIndex(row, child);
            result.push({
                display: tasksModel.data(idx, Qt.DisplayRole) || "",
                decoration: tasksModel.data(idx, Qt.DecorationRole),
                IsActive: Boolean(tasksModel.data(idx, TaskManager.AbstractTasksModel.IsActive)),
                IsMinimized: Boolean(tasksModel.data(idx, TaskManager.AbstractTasksModel.IsMinimized)),
                WinIdList: tasksModel.data(idx, TaskManager.AbstractTasksModel.WinIdList) || [],
                LastActivated: tasksModel.data(idx, TaskManager.AbstractTasksModel.LastActivated)
            });
        }
        return result;
    }

    function _refresh() {
        ++_revision;
    }

    property Connections _watch: Connections {
        target: group.enabled ? group.tasksModel : null
        function onDataChanged() { group._refresh(); }
        function onRowsInserted() { group._refresh(); }
        function onRowsRemoved() { group._refresh(); }
        function onRowsMoved() { group._refresh(); }
        function onLayoutChanged() { group._refresh(); }
        function onModelReset() { group._refresh(); }
    }
}
