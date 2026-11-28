# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: Apache-2.0

add_executable(test_glassclient test_glassclient.cpp)
target_link_libraries(test_glassclient PRIVATE Fluctus::GlassClient fluctus_warnings Qt6::Test)
set_target_properties(test_glassclient PROPERTIES AUTOMOC ON)
add_test(NAME GlassClientTests COMMAND test_glassclient)
set_tests_properties(GlassClientTests PROPERTIES LABELS glass)
