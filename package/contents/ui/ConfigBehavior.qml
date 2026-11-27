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
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Task Filtering")
        }

        QQC2.CheckBox {
            id: currentScreenCheck
            Kirigami.FormData.label: i18n("Show tasks from:")
            text: i18n("Current screen only")
        }

        QQC2.CheckBox {
            id: currentDesktopCheck
            text: i18n("Current virtual desktop only")
        }

        QQC2.CheckBox {
            id: currentActivityCheck
            text: i18n("Current activity only")
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Interaction & Tooltips")
        }

        QQC2.CheckBox {
            id: minimizeActiveCheck
            Kirigami.FormData.label: i18n("Clicking active task:")
            text: i18n("Minimize active window")
        }

        QQC2.CheckBox {
            id: unhideAttentionCheck
            Kirigami.FormData.label: i18n("Notifications:")
            text: i18n("Unhide panel when a window demands attention")
        }

        QQC2.CheckBox {
            id: showTooltipsCheck
            Kirigami.FormData.label: i18n("Hover tooltips:")
            text: i18n("Show task tooltips on hover")
        }

        QQC2.CheckBox {
            id: showPreviewsCheck
            enabled: showTooltipsCheck.checked
            text: i18n("Show live PipeWire window thumbnails in tooltips")
        }

        QQC2.CheckBox {
            id: indicateAudioCheck
            Kirigami.FormData.label: i18n("Audio:")
            text: i18n("Show clickable mute indicator badge on apps playing audio")
        }

        QQC2.CheckBox {
            id: shortcutBadgesCheck
            Kirigami.FormData.label: i18n("Shortcuts:")
            text: i18n("Show 1–9 shortcut badges while holding the Meta key")
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Pinning & Order")
        }

        QQC2.CheckBox {
            id: separateLaunchersCheck
            Kirigami.FormData.label: i18n("Launchers:")
            text: i18n("Keep pinned launchers separate from running tasks")
        }

        QQC2.CheckBox {
            id: dragOffUnpinCheck
            text: i18n("Drag a pinned icon away from the dock to unpin it")
        }

        QQC2.CheckBox {
            id: reverseModeCheck
            Kirigami.FormData.label: i18n("Order:")
            text: i18n("Reverse task order")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Information
            text: i18n("Tip: Right-click any app icon to pin or unpin it, drag icons to reorder them, or drop apps from the Application Launcher directly onto the dock.")
        }
    }
}
