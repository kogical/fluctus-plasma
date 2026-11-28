import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18n("Presets")
        icon: "games-config-theme"
        source: "ConfigPresets.qml"
    }
    ConfigCategory {
        name: i18n("Appearance")
        icon: "preferences-desktop-theme"
        source: "ConfigAppearance.qml"
    }
    ConfigCategory {
        name: i18n("Size & Spacing")
        icon: "transform-scale"
        source: "ConfigSize.qml"
    }
    ConfigCategory {
        name: i18n("Magnification")
        icon: "zoom-in"
        source: "ConfigPhysics.qml"
    }
    ConfigCategory {
        name: i18n("Behavior")
        icon: "preferences-system-windows"
        source: "ConfigBehavior.qml"
    }
}
