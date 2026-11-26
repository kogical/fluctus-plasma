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
# Qt Quick Layer (DockLayoutItem, DockAnimationDriver, BlurRegionController)
# -----------------------------------------------------------------------------
add_library(fluctus_quick STATIC
    src/quick/DockAnimationDriver.hpp
    src/quick/DockAnimationDriver.cpp
    src/quick/BlurRegionController.hpp
    src/quick/BlurRegionController.cpp
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

# -----------------------------------------------------------------------------
# Compiled Plasma::Applet (QML embedded into plugin .so)
# -----------------------------------------------------------------------------
set(FLUCTUS_QML_SOURCES
    package/contents/ui/main.qml
    package/contents/ui/TaskDelegate.qml
    package/contents/ui/ConfigAppearance.qml
    package/contents/ui/ConfigBehavior.qml
    package/contents/config/config.qml
)

set(FLUCTUS_RESOURCES
    package/contents/config/main.xml
)

set_source_files_properties(package/contents/ui/main.qml PROPERTIES QT_RESOURCE_ALIAS main.qml)
set_source_files_properties(package/contents/ui/TaskDelegate.qml PROPERTIES QT_RESOURCE_ALIAS TaskDelegate.qml)
set_source_files_properties(package/contents/ui/ConfigAppearance.qml PROPERTIES QT_RESOURCE_ALIAS ConfigAppearance.qml)
set_source_files_properties(package/contents/ui/ConfigBehavior.qml PROPERTIES QT_RESOURCE_ALIAS ConfigBehavior.qml)
set_source_files_properties(package/contents/config/config.qml PROPERTIES QT_RESOURCE_ALIAS config.qml)
set_source_files_properties(package/contents/config/main.xml PROPERTIES QT_RESOURCE_ALIAS main.xml)

if(ECM_FOUND)
    find_package(KF6CoreAddons 6.0 REQUIRED)
    find_package(KF6I18n 6.0 REQUIRED)
    find_package(KF6GlobalAccel 6.0 REQUIRED)
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
            src/quick/DockLayoutItem.hpp
            src/quick/DockLayoutItem.cpp
            src/backend/ShortcutManager.hpp
            src/backend/ShortcutManager.cpp
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
        KF6::CoreAddons
        KF6::I18n
        KF6::WindowSystem
        KF6::GlobalAccel
    )

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
