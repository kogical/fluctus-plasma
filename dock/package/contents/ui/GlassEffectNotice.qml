import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

// Explains how to get the Glass KWin effect (kwin-effects-glass), which Liquid Glass needs for
// real refraction. Hidden once the effect is running.
Kirigami.InlineMessage {
    id: notice

    // 0 = not installed, 1 = installed but not running, 2 = running
    readonly property int effectState: Plasmoid.glassEffectState || 0
    readonly property bool onArch: Plasmoid.distroFamily === "arch"
    readonly property string archCommand: "yay -S kwin-effects-glass-git"
    readonly property string guideUrl: "https://github.com/4v3ngR/kwin-effects-glass#installation"

    Layout.fillWidth: true
    visible: effectState !== 2
    type: Kirigami.MessageType.Information
    text: {
        if (effectState === 1) {
            return i18n("The Glass effect is installed but not turned on. In Desktop Effects, turn on Glass and turn off Blur, since the two can't run together.");
        }
        const intro = i18n("Liquid Glass needs the Glass effect for KWin, which bends whatever is behind the dock, windows included.");
        const how = onArch
            ? i18n("Install it from the AUR with “%1” (or your usual AUR helper), then turn it on in Desktop Effects.", archCommand)
            : i18n("There is no ready-made package for your distribution, so it is built from source. The installation guide walks you through it.");
        const caveat = i18n("It replaces KWin's blur for your whole desktop and needs reinstalling after Plasma updates.");
        return intro + " " + how + " " + caveat;
    }

    actions: [
        Kirigami.Action {
            visible: notice.effectState === 0 && notice.onArch
            icon.name: "edit-copy"
            text: i18n("Copy command")
            onTriggered: Plasmoid.copyToClipboard(notice.archCommand)
        },
        Kirigami.Action {
            visible: notice.effectState === 0
            icon.name: "help-contents"
            text: i18n("Installation guide")
            onTriggered: Qt.openUrlExternally(notice.guideUrl)
        },
        Kirigami.Action {
            visible: notice.effectState === 1
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
