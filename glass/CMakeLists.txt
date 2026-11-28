# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fluctus Glass: thin KWin compositor effect adapter (GPL-3.0-or-later, see glass/LICENSE)
# delegating all Liquid Glass rendering to the standalone Apache-2.0 Rust `liquidglass` engine.
# Built against KWin's private development headers: rebuild after every KWin upgrade.

find_package(KWin 6.8 CONFIG)
if(NOT KWin_FOUND)
    message(STATUS "Fluctus Glass: KWin development files not found; skipping the KWin effect.")
    return()
endif()

if(NOT ECM_FOUND)
    message(STATUS "Fluctus Glass: needs extra-cmake-modules; skipping the KWin effect.")
    return()
endif()

find_program(CARGO_EXECUTABLE cargo)
if(NOT CARGO_EXECUTABLE)
    message(FATAL_ERROR "Fluctus Glass requires Cargo (Rust toolchain) to build crates/liquidglass.")
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(KF6 6.0 REQUIRED COMPONENTS Config ConfigWidgets CoreAddons I18n KCMUtils WidgetsAddons)
find_package(KDecoration3 REQUIRED)
find_package(KWinDBusInterface CONFIG QUIET)

message(STATUS "Fluctus Glass: building against KWin ${KWin_VERSION}")
set(FLUCTUS_GLASS_KWIN_VERSION "${KWin_VERSION}" CACHE INTERNAL "KWin version Fluctus Glass is built against")

# Build the standalone Rust rendering engine as a minimal staticlib (gl + ffi only, zero wgpu bloat).
set(LG_CRATE_DIR "${CMAKE_SOURCE_DIR}/crates/liquidglass")
set(LG_RUST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/rust-target")
set(LG_RUST_LIB "${LG_RUST_TARGET_DIR}/release/libliquidglass.a")
set(LG_RUST_HEADER "${LG_CRATE_DIR}/include/liquidglass.h")

file(GLOB LG_RUST_SOURCES CONFIGURE_DEPENDS
    "${LG_CRATE_DIR}/Cargo.toml"
    "${LG_CRATE_DIR}/build.rs"
    "${LG_CRATE_DIR}/cbindgen.toml"
    "${LG_CRATE_DIR}/src/*.rs"
    "${LG_CRATE_DIR}/shaders/*.wgsl"
)

add_custom_command(
    OUTPUT "${LG_RUST_LIB}" "${LG_RUST_HEADER}"
    COMMAND "${CARGO_EXECUTABLE}" build
        --manifest-path "${LG_CRATE_DIR}/Cargo.toml"
        --release
        --no-default-features
        --features "gl,ffi"
        --target-dir "${LG_RUST_TARGET_DIR}"
    DEPENDS ${LG_RUST_SOURCES}
    COMMENT "Building Rust liquidglass staticlib (gl,ffi) + liquidglass.h"
    VERBATIM
)

add_custom_target(fluctus_liquidglass_rust DEPENDS "${LG_RUST_LIB}" "${LG_RUST_HEADER}")

add_library(Fluctus::LiquidGlassRust STATIC IMPORTED GLOBAL)
set_target_properties(Fluctus::LiquidGlassRust PROPERTIES
    IMPORTED_LOCATION "${LG_RUST_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${LG_CRATE_DIR}/include"
)
add_dependencies(Fluctus::LiquidGlassRust fluctus_liquidglass_rust)

# Pure helpers: no KWin dependency, unit-tested.
add_library(fluctus_glass_logic STATIC
    src/shapes.hpp
    src/shapes.cpp
    src/material.hpp
    src/material.cpp
    src/glassservice.hpp
    src/glassservice.cpp
)
set_target_properties(fluctus_glass_logic PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(fluctus_glass_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fluctus_glass_logic
    PUBLIC Qt6::Core Qt6::Gui Qt6::DBus Fluctus::GlassProtocol
    PRIVATE fluctus_warnings
)

set(GLASS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
configure_file(src/fluctusglass.qrc.in "${CMAKE_CURRENT_BINARY_DIR}/fluctusglass.qrc" @ONLY)

set(fluctusglass_SOURCES
    src/glasseffect.hpp
    src/glasseffect.cpp
    src/main.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/fluctusglass.qrc"
)
kconfig_add_kcfg_files(fluctusglass_SOURCES src/fluctusglassconfig.kcfgc)

add_library(fluctusglass MODULE ${fluctusglass_SOURCES})
set_target_properties(fluctusglass PROPERTIES AUTOMOC ON AUTORCC ON)
target_include_directories(fluctusglass PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fluctusglass PRIVATE
    fluctus_glass_logic
    Fluctus::LiquidGlassRust
    fluctus_warnings
    KWin::kwin
    KF6::ConfigGui
    KDecoration3::KDecoration
    ${CMAKE_DL_LIBS}
)
fluctus_allow_sanitizer_undefs(fluctusglass)
install(TARGETS fluctusglass DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins)

add_subdirectory(kcm)

if(FLUCTUS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
