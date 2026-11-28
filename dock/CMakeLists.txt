# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: Apache-2.0
#
# Fluctus Dock: compiled Plasma::Applet (QML embedded into the plugin .so).
# Shared dependencies (ECM, Qt, KF6WindowSystem, fluctus_warnings) come from the top level.

set(FLUCTUS_APPLET_ID "com.kogical.fluctus.dock")
# Applet id used before the monorepo split; dock/justfile migrates installs and panels from it.
set(FLUCTUS_LEGACY_APPLET_ID "com.kogical.fluctus")

# -----------------------------------------------------------------------------
# Pure C++23 Physics & Layout Core (zero QtQuick dependency)
# -----------------------------------------------------------------------------
add_library(fluctus_core STATIC
    src/core/AxisGeometry.hpp
    src/core/MagnificationCurves.hpp
    src/core/DampedSpring.hpp
    src/core/BouncePhysics.hpp
    src/core/DockPhysicsEngine.hpp
    src/core/DockPhysicsEngine.cpp
)
target_include_directories(fluctus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fluctus_core PRIVATE fluctus_warnings)
set_target_properties(fluctus_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(FLUCTUS_QUICK_SOURCES
    src/quick/DockAnimationDriver.hpp
    src/quick/DockAnimationDriver.cpp
    src/quick/BlurRegionController.hpp
    src/quick/BlurRegionController.cpp
    src/quick/DockIconItem.hpp
    src/quick/DockIconItem.cpp
    src/quick/DockLayoutItem.hpp
    src/quick/DockLayoutItem.cpp
)

set(FLUCTUS_PCH_HEADERS
    <QAbstractItemModel>
    <QObject>
    <QPointer>
    <QQmlComponent>
    <QQuickItem>
    <QQuickWindow>
    <QRectF>
)

# -----------------------------------------------------------------------------
# Qt Quick Layer (used by unit tests or non-ECM builds; compiled directly into
# the Plasma applet target when ECM is present to avoid duplicate builds)
# -----------------------------------------------------------------------------
if(FLUCTUS_BUILD_TESTS OR NOT ECM_FOUND)
    add_library(fluctus_quick STATIC ${FLUCTUS_QUICK_SOURCES})
    target_include_directories(fluctus_quick PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(fluctus_quick
        PUBLIC
            fluctus_core
            Fluctus::GlassClient
            Qt6::Core
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
        PRIVATE
            fluctus_warnings
            KF6::WindowSystem
    )
    set_target_properties(fluctus_quick PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(FLUCTUS_USE_PCH)
        target_precompile_headers(fluctus_quick PRIVATE ${FLUCTUS_PCH_HEADERS})
    endif()
endif()

# -----------------------------------------------------------------------------
# Compiled Plasma::Applet
# -----------------------------------------------------------------------------
set(FLUCTUS_QML_SOURCES
    package/contents/ui/main.qml
    package/contents/ui/AudioStreamTracker.qml
    package/contents/ui/TaskDelegate.qml
    package/contents/ui/TaskContextMenu.qml
    package/contents/ui/ToolTipDelegate.qml
    package/contents/ui/GroupDialog.qml
    package/contents/ui/ConfigAppearance.qml
    package/contents/ui/ConfigPhysics.qml
    package/contents/ui/ConfigBehavior.qml
    package/contents/ui/ConfigPresets.qml
    package/contents/ui/ConfigSize.qml
    package/contents/ui/ConfigSlider.qml
    package/contents/ui/ConfigHint.qml
    package/contents/ui/GlassEffectNotice.qml
    package/contents/ui/DockBackground.qml
    package/contents/ui/DockPreview.qml
    package/contents/ui/Presets.js
    package/contents/config/config.qml
)

set(FLUCTUS_RESOURCES
    package/contents/config/main.xml
)

foreach(file IN LISTS FLUCTUS_QML_SOURCES FLUCTUS_RESOURCES)
    get_filename_component(alias "${file}" NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${alias})
endforeach()

if(ECM_FOUND)
    find_package(KF6CoreAddons 6.0 REQUIRED)
    find_package(KF6Config 6.0 REQUIRED)
    find_package(KF6GuiAddons 6.0 REQUIRED)
    find_package(KF6I18n 6.0 REQUIRED)
    find_package(KF6GlobalAccel 6.0 REQUIRED)
    find_package(KF6Service 6.0 REQUIRED)
    find_package(KF6KIO 6.0 REQUIRED)
    find_package(Plasma 6.0 REQUIRED)
    find_package(LibTaskManager REQUIRED)

    plasma_add_applet(${FLUCTUS_APPLET_ID}
        QML_SOURCES ${FLUCTUS_QML_SOURCES}
        RESOURCES ${FLUCTUS_RESOURCES}
        CPP_SOURCES
            ${FLUCTUS_QUICK_SOURCES}
            src/backend/ShortcutManager.hpp
            src/backend/ShortcutManager.cpp
            src/backend/SmartLauncherBackend.hpp
            src/backend/SmartLauncherBackend.cpp
            src/backend/TaskBackend.hpp
            src/backend/TaskBackend.cpp
            src/backend/FluctusApplet.hpp
            src/backend/FluctusApplet.cpp
    )

    target_include_directories(${FLUCTUS_APPLET_ID} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/quick
    )

    target_link_libraries(${FLUCTUS_APPLET_ID} PRIVATE
        fluctus_core
        fluctus_warnings
        Fluctus::GlassClient
        kogical_liquidglass
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::DBus
        Plasma::Plasma
        PW::LibTaskManager
        KF6::CoreAddons
        KF6::ConfigCore
        KF6::GuiAddons
        KF6::I18n
        KF6::WindowSystem
        KF6::GlobalAccel
        KF6::Service
        KF6::KIOGui
    )

    fluctus_allow_sanitizer_undefs(${FLUCTUS_APPLET_ID})

    if(FLUCTUS_USE_PCH)
        target_precompile_headers(${FLUCTUS_APPLET_ID} PRIVATE ${FLUCTUS_PCH_HEADERS})
    endif()

    # Bundled Plasma/LayoutTemplate for a pre-configured dock panel
    install(
        DIRECTORY layout-templates/
        DESTINATION "${KDE_INSTALL_DATADIR}/plasma/layout-templates/${FLUCTUS_APPLET_ID}.panel"
    )
endif()

if(FLUCTUS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
