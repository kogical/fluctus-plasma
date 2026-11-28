# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: Apache-2.0
#
# Fluctus Battery: compiled Plasma::Applet (QML embedded into the plugin .so).
# Shared dependencies (ECM, Qt, fluctus_warnings, GlassClient, LiquidGlass) come from the top level.

set(FLUCTUS_BATTERY_APPLET_ID "com.kogical.fluctus.battery")

# -----------------------------------------------------------------------------
# Pure C++23 Battery & Adaptive Layout Core (zero QtQuick or KDE dependencies)
# -----------------------------------------------------------------------------
add_library(fluctus_battery_core STATIC
    src/core/BatteryTypes.hpp
    src/core/BatteryLogic.hpp
    src/core/BatteryLogic.cpp
    src/core/AdaptiveLayout.hpp
)
target_include_directories(fluctus_battery_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fluctus_battery_core PRIVATE fluctus_warnings)
set_target_properties(fluctus_battery_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# -----------------------------------------------------------------------------
# Qt Model & Snapshot Parser (Qt6::Core only — headless unit-testable)
# -----------------------------------------------------------------------------
add_library(fluctus_battery_model STATIC
    src/backend/BatteryDeviceModel.hpp
    src/backend/BatteryDeviceModel.cpp
)
set_target_properties(fluctus_battery_model PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    AUTOMOC ON
)
target_include_directories(fluctus_battery_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fluctus_battery_model
    PUBLIC
        fluctus_battery_core
        Qt6::Core
    PRIVATE
        fluctus_warnings
)

# -----------------------------------------------------------------------------
# Compiled Plasma::Applet
# -----------------------------------------------------------------------------
set(FLUCTUS_BATTERY_QML_SOURCES
    package/contents/ui/main.qml
    package/contents/ui/GlassSurface.qml
    package/contents/ui/BatteryRing.qml
    package/contents/ui/BatteryCapsule.qml
    package/contents/ui/CompactView.qml
    package/contents/ui/DeviceCard.qml
    package/contents/ui/FullView.qml
    package/contents/ui/ConfigGeneral.qml
    package/contents/ui/ConfigAppearance.qml
    package/contents/config/config.qml
)

set(FLUCTUS_BATTERY_RESOURCES
    package/contents/config/main.xml
)

foreach(file IN LISTS FLUCTUS_BATTERY_QML_SOURCES FLUCTUS_BATTERY_RESOURCES)
    get_filename_component(alias "${file}" NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${alias})
endforeach()

if(ECM_FOUND)
    find_program(CARGO_EXECUTABLE cargo)
    if(NOT CARGO_EXECUTABLE)
        message(FATAL_ERROR "Fluctus Battery requires Cargo (Rust toolchain) to build crates/scintilla.")
    endif()

    find_package(KF6CoreAddons 6.0 REQUIRED)
    find_package(KF6I18n 6.0 REQUIRED)
    find_package(Plasma 6.0 REQUIRED)

    # Build the standalone Rust scintilla crate as a staticlib with its C ABI (`ffi` feature).
    set(FLUCTUS_CARGO_TARGET_DIR "${PROJECT_SOURCE_DIR}/target" CACHE PATH "Cargo target directory for Rust crates")
    set(SCINTILLA_CRATE_DIR "${PROJECT_SOURCE_DIR}/crates/scintilla")
    set(SCINTILLA_RUST_LIB "${FLUCTUS_CARGO_TARGET_DIR}/release/libscintilla.a")

    file(GLOB_RECURSE SCINTILLA_RUST_SOURCES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/Cargo.toml"
        "${PROJECT_SOURCE_DIR}/Cargo.lock"
        "${SCINTILLA_CRATE_DIR}/Cargo.toml"
        "${SCINTILLA_CRATE_DIR}/include/*.h"
        "${SCINTILLA_CRATE_DIR}/src/*.rs"
    )

    add_custom_command(
        OUTPUT "${SCINTILLA_RUST_LIB}"
        COMMAND "${CARGO_EXECUTABLE}" build
            --manifest-path "${SCINTILLA_CRATE_DIR}/Cargo.toml"
            --release
            --locked
            --features "ffi"
            --target-dir "${FLUCTUS_CARGO_TARGET_DIR}"
        DEPENDS ${SCINTILLA_RUST_SOURCES}
        COMMENT "Building Rust scintilla staticlib (ffi)"
        VERBATIM
    )

    add_custom_target(fluctus_scintilla_rust DEPENDS "${SCINTILLA_RUST_LIB}")

    add_library(Fluctus::ScintillaRust STATIC IMPORTED GLOBAL)
    set_target_properties(Fluctus::ScintillaRust PROPERTIES
        IMPORTED_LOCATION "${SCINTILLA_RUST_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${SCINTILLA_CRATE_DIR}/include"
    )
    add_dependencies(Fluctus::ScintillaRust fluctus_scintilla_rust)

    plasma_add_applet(${FLUCTUS_BATTERY_APPLET_ID}
        QML_SOURCES ${FLUCTUS_BATTERY_QML_SOURCES}
        RESOURCES ${FLUCTUS_BATTERY_RESOURCES}
        CPP_SOURCES
            src/backend/ScintillaBridge.hpp
            src/backend/ScintillaBridge.cpp
            src/backend/BatteryApplet.hpp
            src/backend/BatteryApplet.cpp
    )

    target_include_directories(${FLUCTUS_BATTERY_APPLET_ID} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(${FLUCTUS_BATTERY_APPLET_ID} PRIVATE
        fluctus_battery_core
        fluctus_battery_model
        fluctus_warnings
        Fluctus::ScintillaRust
        Fluctus::GlassClient
        kogical_liquidglass
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::DBus
        Plasma::Plasma
        KF6::CoreAddons
        KF6::I18n
        ${CMAKE_DL_LIBS}
    )

    fluctus_allow_sanitizer_undefs(${FLUCTUS_BATTERY_APPLET_ID})
endif()

if(FLUCTUS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
