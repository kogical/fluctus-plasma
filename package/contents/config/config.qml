import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18n("Appearance")
        icon: "preferences-desktop-theme"
        source: "ConfigAppearance.qml"
    }
    ConfigCategory {
        name: i18n("Physics & Animation")
        icon: "preferences-desktop-effects"
        source: "ConfigPhysics.qml"
    }
    ConfigCategory {
        name: i18n("Behavior")
        icon: "preferences-system-windows"
        source: "ConfigBehavior.qml"
    }
}
