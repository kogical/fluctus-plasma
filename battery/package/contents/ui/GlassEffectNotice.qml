// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Explains how to get Fluctus Glass, the KWin effect behind real Liquid Glass, and what the
// widget does meanwhile (Frosted, the pre-Liquid Glass look). Hidden once the effect runs.
// Mirrors Fluctus Dock's notice; see docs/preferences-guidelines.md.
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

Kirigami.InlineMessage {
    id: notice

    // 0 = not installed, 1 = installed but turned off, 2 = running
    readonly property int effectState: Plasmoid.glassEffectState || 0
    /// The user chose Liquid Glass, so this is a Warning about what they see instead.
    property bool liquidChosen: false
    readonly property string installCommand: "just glass install && just glass enable"
    readonly property string guideUrl: "https://github.com/kogical/fluctus/tree/main/glass#readme"

    Layout.fillWidth: true
    visible: effectState !== 2
    type: liquidChosen ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
    text: {
        const meanwhile = liquidChosen
            ? i18n("Until then the widget shows Frosted glass, the look macOS had before Liquid Glass.")
            : i18n("Without it, Liquid Glass looks show as Frosted glass, the look macOS had before Liquid Glass.");
        if (effectState === 1) {
            return i18n("Fluctus Glass is installed but turned off. Turn it on for real Liquid Glass.") + " " + meanwhile;
        }
        return i18n("Liquid Glass needs Fluctus Glass, a KWin effect that bends what is behind the widget. In the Fluctus source folder, run “%1”.", installCommand) + " " + meanwhile;
    }

    actions: [
        Kirigami.Action {
            visible: notice.effectState === 1
            icon.name: "dialog-ok-apply"
            text: i18n("Turn on Fluctus Glass")
            onTriggered: {
                Plasmoid.setFluctusGlassEnabled(true);
                Plasmoid.refreshGlassEffectState();
            }
        },
        Kirigami.Action {
            visible: notice.effectState === 0
            icon.name: "edit-copy"
            text: i18n("Copy command")
            onTriggered: Plasmoid.copyToClipboard(notice.installCommand)
        },
        Kirigami.Action {
            visible: notice.effectState === 0
            icon.name: "help-contents"
            text: i18n("Installation guide")
            onTriggered: Qt.openUrlExternally(notice.guideUrl)
        },
        Kirigami.Action {
            icon.name: "view-refresh"
            text: i18n("Check again")
            onTriggered: Plasmoid.refreshGlassEffectState()
        }
    ]
}
