# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: Apache-2.0

add_executable(test_battery_core test_battery_core.cpp)
set_target_properties(test_battery_core PROPERTIES AUTOMOC ON)
target_link_libraries(test_battery_core PRIVATE
    fluctus_battery_core
    fluctus_battery_model
    fluctus_warnings
    Qt6::Core
    Qt6::Test
)
add_test(NAME test_battery_core COMMAND test_battery_core)
set_tests_properties(test_battery_core PROPERTIES LABELS battery)
