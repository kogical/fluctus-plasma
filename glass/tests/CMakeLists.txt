# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(test_glass_logic test_glass_logic.cpp)
target_link_libraries(test_glass_logic PRIVATE fluctus_glass_logic fluctus_warnings Qt6::Test)
set_target_properties(test_glass_logic PROPERTIES AUTOMOC ON)
add_test(NAME GlassLogicTests COMMAND test_glass_logic)
