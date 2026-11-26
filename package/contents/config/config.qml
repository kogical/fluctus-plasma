import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18n("Appearance & Physics")
        icon: "preferences-desktop-theme"
        source: "ConfigAppearance.qml"
    }
    ConfigCategory {
        name: i18n("Behavior")
        icon: "preferences-system-windows"
        source: "ConfigBehavior.qml"
    }
}
