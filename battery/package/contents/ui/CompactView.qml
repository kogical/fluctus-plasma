// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Adaptive panel representation: scales cleanly from tiny 22-32px bars up to tall/vertical panels.
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

MouseArea {
    id: compact

    required property var devicesModel
    required property bool isVertical
    required property var toggleExpanded

    readonly property var metrics: Plasmoid.layoutMetrics(
        Math.max(1, width),
        Math.max(1, height),
        true,
        isVertical
    )
    readonly property bool showPercentage: Plasmoid.configuration.showPercentageInPanel
    readonly property bool showDeviceIcon: Plasmoid.configuration.showDeviceIconInPanel
    readonly property bool showAll: Plasmoid.configuration.showAllBatteriesInPanel && !isVertical && devicesModel.batteryCount > 1

    hoverEnabled: true
    acceptedButtons: Qt.LeftButton | Qt.MiddleButton
    onClicked: function(mouse) {
        if (mouse.button === Qt.MiddleButton) {
            Plasmoid.refresh();
        } else {
            compact.toggleExpanded();
        }
    }

    Layout.minimumWidth: isVertical ? Kirigami.Units.iconSizes.small : Math.ceil(contentLoader.implicitWidth + metrics.padding * 2)
    Layout.minimumHeight: isVertical ? Math.ceil(contentLoader.implicitHeight + metrics.padding * 2) : Kirigami.Units.iconSizes.small
    Layout.preferredWidth: isVertical ? -1 : Math.ceil(contentLoader.implicitWidth + metrics.padding * 2)
    Layout.preferredHeight: isVertical ? Math.ceil(contentLoader.implicitHeight + metrics.padding * 2) : -1

    // Subtle hover pill highlight
    Rectangle {
        anchors.fill: parent
        anchors.margins: Math.max(1, Math.round(compact.metrics.padding * 0.4))
        radius: compact.metrics.cornerRadius
        color: Qt.rgba(
            Kirigami.Theme.textColor.r,
            Kirigami.Theme.textColor.g,
            Kirigami.Theme.textColor.b,
            compact.containsMouse ? 0.10 : 0.0
        )
        Behavior on color {
            ColorAnimation { duration: Kirigami.Units.veryShortDuration }
        }
    }

    Loader {
        id: contentLoader
        anchors.centerIn: parent
        sourceComponent: compact.showAll
            ? multiChipComponent
            : (compact.isVertical ? verticalSingleComponent : horizontalSingleComponent)
    }

    Component {
        id: horizontalSingleComponent

        RowLayout {
            spacing: compact.metrics.spacing

            Loader {
                Layout.alignment: Qt.AlignVCenter
                sourceComponent: compact.showDeviceIcon ? deviceIconGlyph : capsuleGlyph
            }

            ColumnLayout {
                visible: compact.showPercentage && compact.devicesModel.hasAnyBattery
                spacing: 0
                Layout.alignment: Qt.AlignVCenter

                RowLayout {
                    spacing: Math.max(2, Math.round(compact.metrics.spacing * 0.5))

                    Text {
                        text: compact.devicesModel.primaryPercentageText
                        color: compact.devicesModel.primarySeverity === "critical"
                            ? Kirigami.Theme.negativeTextColor
                            : Kirigami.Theme.textColor
                        font.pixelSize: compact.metrics.fontSizePx
                        font.weight: Font.DemiBold
                        font.features: ({ "tnum": 1 })
                        verticalAlignment: Text.AlignVCenter
                    }

                    Kirigami.Icon {
                        visible: compact.devicesModel.primaryCharging
                        source: "battery-charging-symbolic"
                        Layout.preferredWidth: Math.max(10, Math.round(compact.metrics.fontSizePx * 0.92))
                        Layout.preferredHeight: Layout.preferredWidth
                        isMask: true
                        color: Kirigami.Theme.positiveTextColor
                    }
                }

                Text {
                    visible: compact.metrics.showSecondaryLabel && compact.devicesModel.primaryName.length > 0
                    text: compact.devicesModel.primaryName
                    color: Kirigami.Theme.disabledTextColor
                    font.pixelSize: compact.metrics.subFontSizePx
                    elide: Text.ElideRight
                    Layout.maximumWidth: Kirigami.Units.gridUnit * 7
                }
            }
        }
    }

    Component {
        id: verticalSingleComponent

        ColumnLayout {
            spacing: Math.max(2, Math.round(compact.metrics.spacing * 0.6))

            Loader {
                Layout.alignment: Qt.AlignHCenter
                sourceComponent: compact.showDeviceIcon ? deviceIconGlyph : capsuleGlyph
            }

            Text {
                visible: compact.showPercentage && compact.devicesModel.hasAnyBattery
                Layout.alignment: Qt.AlignHCenter
                text: compact.devicesModel.primaryPercentageText
                color: compact.devicesModel.primarySeverity === "critical"
                    ? Kirigami.Theme.negativeTextColor
                    : Kirigami.Theme.textColor
                font.pixelSize: compact.metrics.fontSizePx
                font.weight: Font.DemiBold
                font.features: ({ "tnum": 1 })
            }
        }
    }

    Component {
        id: multiChipComponent

        RowLayout {
            spacing: Math.max(6, Math.round(compact.metrics.spacing * 1.4))

            Repeater {
                model: compact.devicesModel

                delegate: RowLayout {
                    required property int index
                    required property bool powerAvailable
                    required property string categoryIcon
                    required property string percentageText
                    required property bool isCharging
                    required property string severity

                    visible: powerAvailable && index < 3
                    spacing: Math.max(3, Math.round(compact.metrics.spacing * 0.6))

                    Kirigami.Icon {
                        source: categoryIcon
                        Layout.preferredWidth: compact.metrics.iconSize
                        Layout.preferredHeight: compact.metrics.iconSize
                        isMask: true
                        color: isCharging ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.textColor
                    }

                    Text {
                        visible: compact.showPercentage
                        text: percentageText
                        color: severity === "critical"
                            ? Kirigami.Theme.negativeTextColor
                            : Kirigami.Theme.textColor
                        font.pixelSize: compact.metrics.fontSizePx
                        font.weight: Font.DemiBold
                        font.features: ({ "tnum": 1 })
                    }
                }
            }
        }
    }

    Component {
        id: capsuleGlyph

        BatteryCapsule {
            height: compact.metrics.iconSize
            width: Math.round(compact.metrics.iconSize * 1.55)
            percentage: compact.devicesModel.primaryPercentage
            hasPercentage: compact.devicesModel.hasPrimaryPercentage
            charging: compact.devicesModel.primaryCharging
            severity: compact.devicesModel.primarySeverity
        }
    }

    Component {
        id: deviceIconGlyph

        Kirigami.Icon {
            source: compact.devicesModel.hasAnyBattery
                ? compact.devicesModel.primaryCategoryIcon
                : "battery-missing-symbolic"
            width: compact.metrics.iconSize
            height: compact.metrics.iconSize
            isMask: true
            color: compact.devicesModel.primaryCharging
                ? Kirigami.Theme.positiveTextColor
                : Kirigami.Theme.textColor
        }
    }
}
