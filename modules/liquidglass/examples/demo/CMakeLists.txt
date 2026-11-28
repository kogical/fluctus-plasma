# SPDX-License-Identifier: MIT
# Standalone demo / visual test for Kogical.LiquidGlass.
#   cmake -S . -B build && cmake --build build && ./build/liquidglass_demo [--grab out.png]
cmake_minimum_required(VERSION 3.21)
project(liquidglass_demo LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Quick)
qt_standard_project_setup(REQUIRES 6.8)
add_subdirectory(../.. liquidglass)
qt_add_executable(liquidglass_demo main.cpp)
qt_add_qml_module(liquidglass_demo URI LiquidGlassDemo VERSION 1.0 QML_FILES Demo.qml)
target_link_libraries(liquidglass_demo PRIVATE Qt6::Gui Qt6::Quick kogical_liquidglass)
