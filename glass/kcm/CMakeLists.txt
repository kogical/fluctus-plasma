# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Desktop Effects settings page for Fluctus Glass.

set(kwin_fluctusglass_config_SOURCES
    glassconfig.hpp
    glassconfig.cpp
)
kconfig_add_kcfg_files(kwin_fluctusglass_config_SOURCES ../src/fluctusglassconfig.kcfgc)

add_library(kwin_fluctusglass_config MODULE ${kwin_fluctusglass_config_SOURCES})
set_target_properties(kwin_fluctusglass_config PROPERTIES AUTOMOC ON)
target_link_libraries(kwin_fluctusglass_config PRIVATE
    fluctus_warnings
    Qt6::DBus
    Qt6::Widgets
    KF6::ConfigWidgets
    KF6::CoreAddons
    KF6::I18n
    KF6::KCMUtils
    KF6::WidgetsAddons
)
fluctus_allow_sanitizer_undefs(kwin_fluctusglass_config)
install(TARGETS kwin_fluctusglass_config DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs)
