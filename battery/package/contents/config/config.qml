// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
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
        name: i18n("Devices")
        icon: "preferences-desktop-peripherals"
        source: "ConfigDevices.qml"
    }
    ConfigCategory {
        name: i18n("Behavior")
        icon: "preferences-system-windows"
        source: "ConfigBehavior.qml"
    }
}
