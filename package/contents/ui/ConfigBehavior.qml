import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    property alias cfg_showOnlyCurrentScreen: currentScreenCheck.checked
    property alias cfg_showOnlyCurrentDesktop: currentDesktopCheck.checked
    property alias cfg_showOnlyCurrentActivity: currentActivityCheck.checked
    property alias cfg_unhideOnAttention: unhideAttentionCheck.checked
    property alias cfg_minimizeActiveTaskOnClick: minimizeActiveCheck.checked
    property alias cfg_separateLaunchers: separateLaunchersCheck.checked
    property alias cfg_reverseMode: reverseModeCheck.checked

    Kirigami.FormLayout {
        QQC2.CheckBox {
            id: currentScreenCheck
            Kirigami.FormData.label: i18n("Filters:")
            text: i18n("Show only tasks from current screen")
        }

        QQC2.CheckBox {
            id: currentDesktopCheck
            text: i18n("Show only tasks from current virtual desktop")
        }

        QQC2.CheckBox {
            id: currentActivityCheck
            text: i18n("Show only tasks from current activity")
        }

        QQC2.CheckBox {
            id: minimizeActiveCheck
            Kirigami.FormData.label: i18n("Interaction:")
            text: i18n("Clicking active task minimizes it")
        }

        QQC2.CheckBox {
            id: unhideAttentionCheck
            text: i18n("Unhide panel when a window demands attention")
        }

        QQC2.CheckBox {
            id: separateLaunchersCheck
            Kirigami.FormData.label: i18n("Layout:")
            text: i18n("Keep launchers separate from running tasks")
        }

        QQC2.CheckBox {
            id: reverseModeCheck
            text: i18n("Reverse task order")
        }
    }
}
