import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Slider with a fixed-width value read-out, used by all settings pages.
RowLayout {
    id: row

    property alias value: slider.value
    property alias from: slider.from
    property alias to: slider.to
    property alias stepSize: slider.stepSize
    /// Formats the read-out; defaults to a percentage.
    property var format: function(v) { return Math.round(v * 100) + "%"; }

    signal moved()

    spacing: Kirigami.Units.largeSpacing
    Layout.fillWidth: true
    Layout.maximumWidth: Kirigami.Units.gridUnit * 22

    QQC2.Slider {
        id: slider
        Layout.fillWidth: true
        onMoved: row.moved()
    }

    QQC2.Label {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 4
        horizontalAlignment: Text.AlignRight
        text: row.format(slider.value)
        font.features: { "tnum": 1 }
    }
}
