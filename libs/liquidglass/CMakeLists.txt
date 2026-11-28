# SPDX-License-Identifier: Apache-2.0
#
# Kogical.LiquidGlass — reusable Qt Quick glass material (QML + one fragment shader).
#
# Use from any Qt 6.8+ CMake project:
#     add_subdirectory(path/to/liquidglass)
#     target_link_libraries(myapp PRIVATE kogical_liquidglass)
# then in QML:
#     import Kogical.LiquidGlass
#
# Builds a STATIC QML module without a plugin: its qmldir, QML and compiled shader land in
# the qrc:/qt/qml/Kogical/LiquidGlass resource path, which Qt's QML engine searches by default.
cmake_minimum_required(VERSION 3.21)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick ShaderTools)

qt_add_library(kogical_liquidglass STATIC)

set_source_files_properties(GlassMaterials.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(kogical_liquidglass
    URI Kogical.LiquidGlass
    VERSION 1.0
    NO_PLUGIN
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/Kogical/LiquidGlass"
    QML_FILES
        LiquidGlass.qml
        GlassMaterials.qml
)

# qsb has no #include support: inline the shared shading core (also used verbatim by the
# Fluctus Glass KWin effect) into the Qt Quick shader before baking it.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/liquidglass_core.glsl" _lg_core)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/liquidglass.frag" _lg_frag)
string(REPLACE "#include \"liquidglass_core.glsl\"" "${_lg_core}" _lg_frag "${_lg_frag}")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/liquidglass.frag" CONTENT "${_lg_frag}" @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    shaders/liquidglass_core.glsl
    shaders/liquidglass.frag
)

qt_add_shaders(kogical_liquidglass "kogical_liquidglass_shaders"
    PREFIX "/qt/qml/Kogical/LiquidGlass"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/shaders/liquidglass.frag"
)

# Path of the shared core for other components (the KWin effect embeds it in its own qrc).
set(KOGICAL_LIQUIDGLASS_CORE_GLSL "${CMAKE_CURRENT_SOURCE_DIR}/shaders/liquidglass_core.glsl" CACHE INTERNAL "")

set_target_properties(kogical_liquidglass PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(kogical_liquidglass PUBLIC Qt6::Quick)

# The module has no C++ sources, so AUTOMOC never creates the include dir Qt exports;
# create it to keep consumers building cleanly with -Wmissing-include-dirs.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/kogical_liquidglass_autogen/include")
