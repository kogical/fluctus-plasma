# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: Apache-2.0
#
# FluctusGlass client helper (Apache-2.0). Speaks only D-Bus to the Fluctus Glass KWin effect, so it
# never links GPL code and can be embedded in any application.
#
#   Fluctus::GlassClient      static C++ library (GlassRequest, GlassProfile, GlassProtocol.hpp)
#   Fluctus::GlassWidgets     optional Qt Widgets editor & modal dialog (GlassMaterialWidget, GlassSettingsDialog)
#   Kogical.FluctusGlass      installed QML module (GlassArea, GlassProfile, GlassSettingsDialog)

add_library(fluctus_glassclient STATIC
    include/FluctusGlass/GlassProtocol.hpp
    include/FluctusGlass/GlassRequest.hpp
    include/FluctusGlass/GlassProfile.hpp
    src/GlassRequest.cpp
    src/GlassProfile.cpp
)
add_library(Fluctus::GlassClient ALIAS fluctus_glassclient)
set_target_properties(fluctus_glassclient PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    AUTOMOC ON
)
target_include_directories(fluctus_glassclient PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(fluctus_glassclient
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
    PRIVATE
        fluctus_warnings
        KF6::WindowSystem
)

# Header-only protocol definitions for the effect (QtCore only, no client code).
add_library(fluctus_glassprotocol INTERFACE)
add_library(Fluctus::GlassProtocol ALIAS fluctus_glassprotocol)
target_include_directories(fluctus_glassprotocol INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Optional Qt Widgets UI shell (shared by Qt Widgets apps and the Desktop Effects KCM).
find_package(Qt6 6.8 QUIET COMPONENTS Widgets)
if(Qt6Widgets_FOUND)
    add_library(fluctus_glasswidgets STATIC
        include/FluctusGlass/GlassMaterialWidget.hpp
        include/FluctusGlass/GlassSettingsDialog.hpp
        widgets/GlassMaterialWidget.cpp
        widgets/GlassSettingsDialog.cpp
    )
    add_library(Fluctus::GlassWidgets ALIAS fluctus_glasswidgets)
    set_target_properties(fluctus_glasswidgets PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        AUTOMOC ON
    )
    target_link_libraries(fluctus_glasswidgets
        PUBLIC
            Fluctus::GlassClient
            Qt6::Widgets
        PRIVATE
            fluctus_warnings
    )
endif()

if(FLUCTUS_BUILD_GLASS_QML AND ECM_FOUND)
    set_source_files_properties(qml/GlassSettingsDialog.qml PROPERTIES
        QT_RESOURCE_ALIAS GlassSettingsDialog.qml
    )
    qt_add_qml_module(fluctus_glass_qml
        URI Kogical.FluctusGlass
        VERSION 1.0
        PLUGIN_TARGET fluctus_glass_qmlplugin
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/Kogical/FluctusGlass"
        SOURCES
            qml/GlassArea.hpp
            qml/GlassArea.cpp
            qml/GlassProfileQml.hpp
        QML_FILES
            qml/GlassSettingsDialog.qml
    )
    target_include_directories(fluctus_glass_qml PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/qml)
    target_link_libraries(fluctus_glass_qml PRIVATE
        fluctus_glassclient
        fluctus_warnings
        Qt6::Quick
    )
    set_target_properties(fluctus_glass_qml PROPERTIES POSITION_INDEPENDENT_CODE ON)
    fluctus_allow_sanitizer_undefs(fluctus_glass_qml)
    # KDECMakeSettings drops the "lib" prefix from modules, but the QML engine looks for
    # lib<plugin>.so as named in qmldir.
    set_target_properties(fluctus_glass_qmlplugin PROPERTIES PREFIX "lib")
    fluctus_allow_sanitizer_undefs(fluctus_glass_qmlplugin)

    # The QML module ships with Fluctus Glass (the effect it talks to).
    set(_qmldir "${KDE_INSTALL_QMLDIR}/Kogical/FluctusGlass")
    install(TARGETS fluctus_glass_qmlplugin LIBRARY DESTINATION "${_qmldir}" COMPONENT glass)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/qml/Kogical/FluctusGlass/qmldir"
        "${CMAKE_CURRENT_BINARY_DIR}/qml/Kogical/FluctusGlass/fluctus_glass_qml.qmltypes"
        DESTINATION "${_qmldir}" COMPONENT glass)
endif()

if(FLUCTUS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
