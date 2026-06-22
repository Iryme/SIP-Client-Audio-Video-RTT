find_package(Qt6 REQUIRED COMPONENTS Core Test)

# DiagnosticsLogger unit tests
qt_add_executable(test_diagnostics
    diagnostics/DiagnosticsLoggerTests.cpp
    ${CMAKE_SOURCE_DIR}/src/diagnostics/DiagnosticsLogger.cpp
)

target_include_directories(test_diagnostics PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_diagnostics PRIVATE
    Qt6::Core
    Qt6::Test
)

set_target_properties(test_diagnostics PROPERTIES
    AUTOMOC ON
)

add_test(NAME test_diagnostics COMMAND test_diagnostics)

# --- LogFilterModel unit tests (Task 4) ---
qt_add_executable(test_log_filter
    gui/LogFilterModelTests.cpp
    ${CMAKE_SOURCE_DIR}/src/diagnostics/LogFilterModel.cpp
    ${CMAKE_SOURCE_DIR}/src/diagnostics/DiagnosticsLogger.cpp
)

target_include_directories(test_log_filter PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_log_filter PRIVATE
    Qt6::Core
    Qt6::Test
)

set_target_properties(test_log_filter PROPERTIES
    AUTOMOC ON
)

add_test(NAME test_log_filter COMMAND test_log_filter)
