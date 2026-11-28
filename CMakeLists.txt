cmake_minimum_required(VERSION 3.28)

project(fluctus-plasma
    VERSION 0.1.0
    DESCRIPTION "macOS-style animated task manager dock for KDE Plasma 6.8+"
    LANGUAGES CXX
)

set(FLUCTUS_APPLET_ID "com.kogical.fluctus")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FLUCTUS_BUILD_TESTS "Build unit tests" ON)
option(FLUCTUS_ENABLE_SANITIZERS "Enable ASan + UBSan for local targets" OFF)
option(FLUCTUS_REQUIRE_ECM "Fail if extra-cmake-modules (ECM) is not installed" OFF)
option(FLUCTUS_USE_PCH "Use precompiled headers to speed up C++ builds" ON)
option(FLUCTUS_USE_FAST_LINKER "Prefer mold or lld when available" ON)

# Auto-detect compiler cache (sccache / ccache) when not explicitly configured
if(NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(FLUCTUS_CCACHE_PROGRAM NAMES sccache ccache)
    if(FLUCTUS_CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${FLUCTUS_CCACHE_PROGRAM}")
        message(STATUS "Using compiler launcher: ${FLUCTUS_CCACHE_PROGRAM}")
    endif()
endif()

# Prefer a fast linker (mold or lld) when available
if(FLUCTUS_USE_FAST_LINKER AND NOT CMAKE_LINKER_TYPE)
    find_program(FLUCTUS_MOLD_LINKER mold)
    find_program(FLUCTUS_LLD_LINKER lld)
    if(FLUCTUS_MOLD_LINKER)
        if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29")
            set(CMAKE_LINKER_TYPE MOLD)
        else()
            add_link_options(-fuse-ld=mold)
        endif()
        message(STATUS "Using fast linker: mold")
    elseif(FLUCTUS_LLD_LINKER)
        if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29")
            set(CMAKE_LINKER_TYPE LLD)
        else()
            add_link_options(-fuse-ld=lld)
        endif()
        message(STATUS "Using fast linker: lld")
    endif()
endif()

find_package(ECM 6.0.0 QUIET NO_MODULE)
if(ECM_FOUND)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
    include(KDEInstallDirs)
    include(KDECMakeSettings)
    include(KDECompilerSettings NO_POLICY_SCOPE)
    include(ECMQmlModule)
    include(FeatureSummary)
else()
    if(FLUCTUS_REQUIRE_ECM)
        message(FATAL_ERROR "extra-cmake-modules (ECM >= 6.0.0) is required to build and install the Plasma applet.")
    endif()
    message(STATUS "ECM (extra-cmake-modules) not found — building fluctus_core, fluctus_quick, and unit tests. Install extra-cmake-modules to build/install the full Plasma::Applet plugin.")
    include(GNUInstallDirs)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    DBus
)

find_package(KF6WindowSystem 6.0 REQUIRED)

# Strict warning flags applied only to Fluctus targets.
add_library(fluctus_warnings INTERFACE)
target_compile_options(fluctus_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Wnon-virtual-dtor
)

if(FLUCTUS_ENABLE_SANITIZERS)
    target_compile_options(fluctus_warnings INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(fluctus_warnings INTERFACE
        -fsanitize=address,undefined
    )
endif()

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

target_include_directories(fluctus_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(fluctus_core PRIVATE
    fluctus_warnings
)

set_target_properties(fluctus_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# Qt Quick Layer (used by unit tests or non-ECM builds; compiled directly into
# the Plasma applet target when ECM is present to avoid duplicate builds)
# -----------------------------------------------------------------------------
if(FLUCTUS_BUILD_TESTS OR NOT ECM_FOUND)
    add_library(fluctus_quick STATIC
        src/quick/DockAnimationDriver.hpp
        src/quick/DockAnimationDriver.cpp
        src/quick/BlurRegionController.hpp
        src/quick/BlurRegionController.cpp
        src/quick/DockIconItem.hpp
        src/quick/DockIconItem.cpp
        src/quick/DockLayoutItem.hpp
        src/quick/DockLayoutItem.cpp
    )

    target_include_directories(fluctus_quick PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(fluctus_quick
        PUBLIC
            fluctus_core
            Qt6::Core
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
        PRIVATE
            fluctus_warnings
            KF6::WindowSystem
    )

    set_target_properties(fluctus_quick PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    if(FLUCTUS_USE_PCH)
        target_precompile_headers(fluctus_quick PRIVATE
            <QAbstractItemModel>
            <QObject>
            <QPointer>
            <QQmlComponent>
            <QQuickItem>
            <QQuickWindow>
            <QRectF>
        )
    endif()
endif()

# -----------------------------------------------------------------------------
# Compiled Plasma::Applet (QML embedded into plugin .so)
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

set_source_files_properties(package/contents/ui/main.qml PROPERTIES QT_RESOURCE_ALIAS main.qml)
set_source_files_properties(package/contents/ui/AudioStreamTracker.qml PROPERTIES QT_RESOURCE_ALIAS AudioStreamTracker.qml)
set_source_files_properties(package/contents/ui/TaskDelegate.qml PROPERTIES QT_RESOURCE_ALIAS TaskDelegate.qml)
set_source_files_properties(package/contents/ui/TaskContextMenu.qml PROPERTIES QT_RESOURCE_ALIAS TaskContextMenu.qml)
set_source_files_properties(package/contents/ui/ToolTipDelegate.qml PROPERTIES QT_RESOURCE_ALIAS ToolTipDelegate.qml)
set_source_files_properties(package/contents/ui/GroupDialog.qml PROPERTIES QT_RESOURCE_ALIAS GroupDialog.qml)
set_source_files_properties(package/contents/ui/ConfigAppearance.qml PROPERTIES QT_RESOURCE_ALIAS ConfigAppearance.qml)
set_source_files_properties(package/contents/ui/ConfigPhysics.qml PROPERTIES QT_RESOURCE_ALIAS ConfigPhysics.qml)
set_source_files_properties(package/contents/ui/ConfigBehavior.qml PROPERTIES QT_RESOURCE_ALIAS ConfigBehavior.qml)
set_source_files_properties(package/contents/ui/ConfigPresets.qml PROPERTIES QT_RESOURCE_ALIAS ConfigPresets.qml)
set_source_files_properties(package/contents/ui/ConfigSize.qml PROPERTIES QT_RESOURCE_ALIAS ConfigSize.qml)
set_source_files_properties(package/contents/ui/ConfigSlider.qml PROPERTIES QT_RESOURCE_ALIAS ConfigSlider.qml)
set_source_files_properties(package/contents/ui/ConfigHint.qml PROPERTIES QT_RESOURCE_ALIAS ConfigHint.qml)
set_source_files_properties(package/contents/ui/GlassEffectNotice.qml PROPERTIES QT_RESOURCE_ALIAS GlassEffectNotice.qml)
set_source_files_properties(package/contents/ui/DockBackground.qml PROPERTIES QT_RESOURCE_ALIAS DockBackground.qml)
set_source_files_properties(package/contents/ui/DockPreview.qml PROPERTIES QT_RESOURCE_ALIAS DockPreview.qml)
set_source_files_properties(package/contents/ui/Presets.js PROPERTIES QT_RESOURCE_ALIAS Presets.js)
set_source_files_properties(package/contents/config/config.qml PROPERTIES QT_RESOURCE_ALIAS config.qml)
set_source_files_properties(package/contents/config/main.xml PROPERTIES QT_RESOURCE_ALIAS main.xml)

if(ECM_FOUND)
    # Reusable glass material module (QML + shader), linked statically into the applet.
    add_subdirectory(modules/liquidglass)

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
            src/quick/DockAnimationDriver.hpp
            src/quick/DockAnimationDriver.cpp
            src/quick/BlurRegionController.hpp
            src/quick/BlurRegionController.cpp
            src/quick/DockIconItem.hpp
            src/quick/DockIconItem.cpp
            src/quick/DockLayoutItem.hpp
            src/quick/DockLayoutItem.cpp
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
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::DBus
        Plasma::Plasma
        PW::LibTaskManager
        kogical_liquidglass
        KF6::CoreAddons
        KF6::ConfigCore
        KF6::GuiAddons
        KF6::I18n
        KF6::WindowSystem
        KF6::GlobalAccel
        KF6::Service
        KF6::KIOGui
    )

    if(FLUCTUS_ENABLE_SANITIZERS)
        # Clang does not link the ASan/UBSan runtime into shared objects (the host executable
        # provides it), so ECM's -Wl,--no-undefined would fail the plugin link. Re-allow
        # undefined symbols for the sanitized plugin only; test executables still link the runtime.
        target_link_options(${FLUCTUS_APPLET_ID} PRIVATE -Wl,-z,undefs)
    endif()

    if(FLUCTUS_USE_PCH)
        target_precompile_headers(${FLUCTUS_APPLET_ID} PRIVATE
            <QAbstractItemModel>
            <QObject>
            <QPointer>
            <QQmlComponent>
            <QQuickItem>
            <QQuickWindow>
            <QRectF>
        )
    endif()

    # Bundled Plasma/LayoutTemplate for a pre-configured dock panel
    install(
        DIRECTORY layout-templates/
        DESTINATION "${KDE_INSTALL_DATADIR}/plasma/layout-templates/${FLUCTUS_APPLET_ID}.panel"
    )
endif()

# -----------------------------------------------------------------------------
# Unit Tests (QtTest / CTest)
# -----------------------------------------------------------------------------
if(FLUCTUS_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

if(ECM_FOUND)
    feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
