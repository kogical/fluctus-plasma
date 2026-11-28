# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: Apache-2.0
#
# Shared build settings for every Fluctus component: language level, build speed-ups and the
# strict warning / sanitizer flags required by AGENTS.md. Included once from the top-level
# CMakeLists.txt so all components compile identically.

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FLUCTUS_ENABLE_SANITIZERS "Enable ASan + UBSan for Fluctus targets" OFF)
option(FLUCTUS_USE_PCH "Use precompiled headers to speed up C++ builds" ON)
option(FLUCTUS_USE_FAST_LINKER "Prefer mold or lld when available" ON)

# Auto-detect a compiler cache (sccache / ccache) when not explicitly configured.
if(NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(FLUCTUS_CCACHE_PROGRAM NAMES sccache ccache)
    if(FLUCTUS_CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${FLUCTUS_CCACHE_PROGRAM}")
        message(STATUS "Using compiler launcher: ${FLUCTUS_CCACHE_PROGRAM}")
    endif()
endif()

# Prefer a fast linker (mold or lld) when available.
if(FLUCTUS_USE_FAST_LINKER AND NOT CMAKE_LINKER_TYPE)
    find_program(FLUCTUS_MOLD_LINKER mold)
    find_program(FLUCTUS_LLD_LINKER lld)
    if(FLUCTUS_MOLD_LINKER)
        set(CMAKE_LINKER_TYPE MOLD)
        message(STATUS "Using fast linker: mold")
    elseif(FLUCTUS_LLD_LINKER)
        set(CMAKE_LINKER_TYPE LLD)
        message(STATUS "Using fast linker: lld")
    endif()
endif()

# Strict warning flags, applied only to Fluctus targets (never to third-party headers).
if(NOT TARGET fluctus_warnings)
    add_library(fluctus_warnings INTERFACE)
    add_library(Fluctus::Warnings ALIAS fluctus_warnings)
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
endif()

# Loadable plugins (Plasma applets, KWin effects): Clang does not link the ASan/UBSan runtime
# into shared objects (the host executable provides it), so ECM's -Wl,--no-undefined would fail.
function(fluctus_allow_sanitizer_undefs target)
    if(FLUCTUS_ENABLE_SANITIZERS)
        target_link_options(${target} PRIVATE -Wl,-z,undefs)
    endif()
endfunction()
