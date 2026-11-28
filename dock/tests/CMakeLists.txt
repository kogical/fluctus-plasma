# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: Apache-2.0

add_executable(test_dock_physics
    test_dock_physics.cpp
)

target_link_libraries(test_dock_physics PRIVATE
    fluctus_core
    fluctus_warnings
    Qt6::Test
)

add_test(NAME DockPhysicsTests COMMAND test_dock_physics)

add_executable(test_dock_layout_item
    test_dock_layout_item.cpp
)

target_link_libraries(test_dock_layout_item PRIVATE
    fluctus_quick
    fluctus_warnings
    Qt6::Test
)

add_test(NAME DockLayoutItemTests COMMAND test_dock_layout_item)
set_tests_properties(DockLayoutItemTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
set_tests_properties(DockPhysicsTests DockLayoutItemTests PROPERTIES LABELS dock)
