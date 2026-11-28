# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: Apache-2.0
#
# Fluctus — a family of KDE Plasma extensions built from one source tree.
#
#   libs/liquidglass   Kogical.LiquidGlass Qt Quick material + shared GLSL core    (Apache-2.0)
#   libs/glassclient   Qt client helper for requesting Fluctus Glass               (Apache-2.0)
#   dock/              Fluctus Dock, a Plasma 6.8+ task manager applet             (Apache-2.0)
#   glass/             Fluctus Glass, a KWin compositor effect                      (GPL-3.0-or-later)
#
# Dependencies are discovered once here and shared by every component. Each component installs
# under its own CMake install component, so `cmake --install <build> --component dock` (used
# by dock/justfile) touches only that component's files.
cmake_minimum_required(VERSION 3.29)

project(fluctus
    VERSION 0.2.0
    DESCRIPTION "Fluctus: dock, glass and tooling for KDE Plasma"
    LANGUAGES CXX
)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FluctusSettings)

option(FLUCTUS_BUILD_DOCK "Build Fluctus Dock (Plasma applet)" ON)
option(FLUCTUS_BUILD_GLASS "Build Fluctus Glass (KWin effect; needs KWin development files)" ON)
option(FLUCTUS_BUILD_GLASS_QML "Build and install the Kogical.FluctusGlass QML module for other apps" ON)
option(FLUCTUS_BUILD_TESTS "Build unit tests" ON)
option(FLUCTUS_BUILD_EXAMPLES "Build example programs" OFF)
option(FLUCTUS_REQUIRE_ECM "Fail if extra-cmake-modules (ECM) is not installed" OFF)

# -----------------------------------------------------------------------------
# Shared dependencies
# -----------------------------------------------------------------------------
find_package(ECM 6.0.0 QUIET NO_MODULE)
if(ECM_FOUND)
    list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
    include(KDEInstallDirs)
    include(KDECMakeSettings)
    include(KDECompilerSettings NO_POLICY_SCOPE)
    include(ECMQmlModule)
    include(FeatureSummary)
else()
    if(FLUCTUS_REQUIRE_ECM)
        message(FATAL_ERROR "extra-cmake-modules (ECM >= 6.0.0) is required to build and install Fluctus components.")
    endif()
    message(STATUS "ECM not found: building only the headless dock core, the Qt Quick layer and unit tests.")
    include(GNUInstallDirs)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick DBus)
find_package(KF6WindowSystem 6.0 REQUIRED)
if(FLUCTUS_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()

# -----------------------------------------------------------------------------
# Shared libraries (always Apache-2.0, never link GPL code)
# -----------------------------------------------------------------------------
add_subdirectory(libs/glassclient)
if(ECM_FOUND OR FLUCTUS_BUILD_EXAMPLES)
    add_subdirectory(libs/liquidglass)
endif()

# -----------------------------------------------------------------------------
# Components
# -----------------------------------------------------------------------------
if(FLUCTUS_BUILD_DOCK)
    set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME dock)
    add_subdirectory(dock)
endif()

if(FLUCTUS_BUILD_GLASS)
    set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME glass)
    add_subdirectory(glass)
endif()

unset(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME)

if(ECM_FOUND)
    feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
