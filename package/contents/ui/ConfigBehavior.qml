import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    property alias cfg_showOnlyCurrentScreen: currentScreenCheck.checked
    property alias cfg_showOnlyCurrentDesktop: currentDesktopCheck.checked
    property alias cfg_showOnlyCurrentActivity: currentActivityCheck.checked
    property alias cfg_unhideOnAttention: unhideAttentionCheck.checked
    property alias cfg_minimizeActiveTaskOnClick: minimizeActiveCheck.checked
    property alias cfg_showTooltips: showTooltipsCheck.checked
    property alias cfg_showWindowPreviews: showPreviewsCheck.checked
    property alias cfg_indicateAudioStreams: indicateAudioCheck.checked
    property alias cfg_showShortcutBadgesOnMeta: shortcutBadgesCheck.checked
    property alias cfg_dragOffToUnpin: dragOffUnpinCheck.checked
    property alias cfg_separateLaunchers: separateLaunchersCheck.checked
    property alias cfg_reverseMode: reverseModeCheck.checked

    Kirigami.FormLayout {
        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Which Windows Appear")
        }

        QQC2.CheckBox {
            id: currentScreenCheck
            Kirigami.FormData.label: i18n("Only show windows from:")
            text: i18n("This screen")
        }

        QQC2.CheckBox {
            id: currentDesktopCheck
            text: i18n("The current virtual desktop")
        }

        QQC2.CheckBox {
            id: currentActivityCheck
            text: i18n("The current activity")
        }

        ConfigHint {
            text: i18n("Pinned apps always stay on the dock.")
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Clicking")
        }

        QQC2.CheckBox {
            id: minimizeActiveCheck
            Kirigami.FormData.label: i18n("Clicking the active app:")
            text: i18n("Minimizes its window")
        }

        ConfigHint {
            text: i18n("Clicking an app with several open windows lets you pick one.")
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Pinned Apps")
        }

        QQC2.CheckBox {
            id: separateLaunchersCheck
            Kirigami.FormData.label: i18n("Pinned apps:")
            text: i18n("Show open windows as separate icons")
        }

        QQC2.CheckBox {
            id: dragOffUnpinCheck
            text: i18n("Unpin by dragging the icon off the dock")
        }

        QQC2.CheckBox {
            id: reverseModeCheck
            Kirigami.FormData.label: i18n("Order:")
            text: i18n("Reverse the order of icons")
        }

        ConfigHint {
            text: i18n("Drag icons to rearrange them, drop apps from the launcher to pin them, or right-click an icon to pin or unpin it.")
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Hovering")
        }

        QQC2.CheckBox {
            id: showTooltipsCheck
            Kirigami.FormData.label: i18n("Tooltips:")
            text: i18n("Show the app name and its windows")
        }

        QQC2.CheckBox {
            id: showPreviewsCheck
            enabled: showTooltipsCheck.checked
            text: i18n("Include live window previews")
        }

        // ---------------------------------------------------------------------
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Badges and Alerts")
        }

        QQC2.CheckBox {
            id: indicateAudioCheck
            Kirigami.FormData.label: i18n("Sound:")
            text: i18n("Mark apps that are playing sound")
        }

        ConfigHint {
            text: i18n("Click the speaker badge to mute or unmute the app.")
        }

        QQC2.CheckBox {
            id: shortcutBadgesCheck
            Kirigami.FormData.label: i18n("Keyboard shortcuts:")
            text: i18n("Number the first nine icons while Meta is held")
        }

        ConfigHint {
            text: i18n("Meta+1 to Meta+9 open or switch to those apps.")
        }

        QQC2.CheckBox {
            id: unhideAttentionCheck
            Kirigami.FormData.label: i18n("Attention:")
            text: i18n("Reveal a hidden panel when an app needs attention")
        }
    }
}
