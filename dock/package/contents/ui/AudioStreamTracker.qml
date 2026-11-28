import QtQuick
import org.kde.plasma.private.volume as PlasmaPa

// Tracks active PulseAudio / PipeWire playback streams via KDE's plasma-pa QML module.
// Loaded lazily via Loader in main.qml so systems without plasma-pa degrade gracefully.
Item {
    id: tracker
    visible: false

    property int revision: 0
    property var _streams: []

    PlasmaPa.SinkInputModel {
        id: sinkInputModel
    }

    Instantiator {
        id: streamInstantiator
        model: sinkInputModel

        delegate: QtObject {
            id: streamProxy
            required property var model
            readonly property var pulseObj: model.PulseObject
            readonly property int pid: {
                if (!pulseObj || !pulseObj.client || !pulseObj.client.properties) {
                    return 0;
                }
                const raw = pulseObj.client.properties["application.process.id"];
                return raw ? parseInt(raw, 10) : 0;
            }
            readonly property string appName: {
                if (!pulseObj) return "";
                if (pulseObj.client && pulseObj.client.name) return String(pulseObj.client.name).toLowerCase();
                return String(pulseObj.name || "").toLowerCase();
            }
            readonly property string desktopId: {
                if (!pulseObj || !pulseObj.client || !pulseObj.client.properties) {
                    return "";
                }
                const raw = pulseObj.client.properties["application.icon_name"]
                    || pulseObj.client.properties["application.id"]
                    || "";
                return String(raw).toLowerCase().replace(/\.desktop$/, "");
            }
            readonly property bool muted: Boolean(pulseObj && pulseObj.muted)
            readonly property bool corked: Boolean(pulseObj && pulseObj.corked)

            onMutedChanged: tracker.revision += 1
            onCorkedChanged: tracker.revision += 1
        }

        onObjectAdded: function(index, object) {
            const next = tracker._streams.slice();
            next.push(object);
            tracker._streams = next;
            tracker.revision += 1;
        }

        onObjectRemoved: function(index, object) {
            tracker._streams = tracker._streams.filter(function(s) { return s !== object; });
            tracker.revision += 1;
        }
    }

    function matchingStreams(pid, appName, appId) {
        const needleName = String(appName || "").toLowerCase();
        const needleId = String(appId || "").toLowerCase().replace(/\.desktop$/, "");
        const result = [];
        for (let i = 0; i < _streams.length; ++i) {
            const s = _streams[i];
            if (!s || !s.pulseObj) {
                continue;
            }
            const pidMatch = pid > 0 && s.pid > 0 && s.pid === pid;
            const idMatch = needleId.length > 0 && s.desktopId.length > 0
                && (s.desktopId === needleId || needleId.indexOf(s.desktopId) >= 0 || s.desktopId.indexOf(needleId) >= 0);
            const nameMatch = needleName.length > 0 && s.appName.length > 0
                && (s.appName === needleName || s.appName.indexOf(needleName) >= 0 || needleName.indexOf(s.appName) >= 0);
            if (pidMatch || idMatch || nameMatch) {
                result.push(s);
            }
        }
        return result;
    }

    function audioStateForTask(pid, appName, appId) {
        const list = matchingStreams(pid, appName, appId);
        if (list.length === 0) {
            return { hasAudio: false, playing: false, muted: false };
        }
        let anyPlaying = false;
        let allMuted = true;
        for (let i = 0; i < list.length; ++i) {
            if (!list[i].corked) anyPlaying = true;
            if (!list[i].muted) allMuted = false;
        }
        return { hasAudio: true, playing: anyPlaying, muted: allMuted };
    }

    function toggleMuteForTask(pid, appName, appId) {
        const list = matchingStreams(pid, appName, appId);
        if (list.length === 0) {
            return;
        }
        let allMuted = true;
        for (let i = 0; i < list.length; ++i) {
            if (!list[i].muted) {
                allMuted = false;
                break;
            }
        }
        const targetMuted = !allMuted;
        for (let i = 0; i < list.length; ++i) {
            if (list[i].pulseObj) {
                list[i].pulseObj.muted = targetMuted;
            }
        }
        tracker.revision += 1;
    }
}
