import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

// Explains how to get a glass effect for KWin, which Liquid Glass needs for real refraction.
// Recommends Fluctus Glass (built from this repository); kwin-effects-glass works too.
// Hidden once either effect is running.
Kirigami.InlineMessage {
    id: notice

    // 0 = not installed, 1 = installed but not running, 2 = running
    readonly property int effectState: Plasmoid.glassEffectState || 0
    // "fluctusglass", "glass" (kwin-effects-glass) or "" when neither is installed
    readonly property string provider: Plasmoid.glassProvider || ""
    readonly property string installCommand: "just glass install && just glass enable"
    readonly property string guideUrl: "https://github.com/kogical/fluctus/tree/main/glass#readme"

    Layout.fillWidth: true
    visible: effectState !== 2
    type: Kirigami.MessageType.Information
    text: {
        if (effectState === 1 && provider === "fluctusglass") {
            return i18n("Fluctus Glass is installed but turned off. Turn it on to give the dock real Liquid Glass.");
        }
        if (effectState === 1) {
            return i18n("The Glass effect is installed but not turned on. In Desktop Effects, turn on Glass and turn off Blur, since the two can't run together.");
        }
        return i18n("Liquid Glass needs Fluctus Glass, a KWin effect that bends whatever is behind the dock, windows included. It comes with Fluctus Dock: in the Fluctus source folder, run “%1”. Rebuild it after KWin updates.", installCommand);
    }

    actions: [
        Kirigami.Action {
            visible: notice.effectState === 1 && notice.provider === "fluctusglass"
            icon.name: "dialog-ok-apply"
            text: i18n("Turn on Fluctus Glass")
            onTriggered: {
                Plasmoid.setKWinEffectEnabled("fluctusglass", true);
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
            visible: notice.effectState === 1 && notice.provider !== "fluctusglass"
            icon.name: "preferences-desktop-effects"
            text: i18n("Open Desktop Effects")
            onTriggered: Plasmoid.openDesktopEffectsSettings()
        },
        Kirigami.Action {
            icon.name: "view-refresh"
            text: i18n("Check again")
            onTriggered: Plasmoid.refreshGlassEffectState()
        }
    ]
}
