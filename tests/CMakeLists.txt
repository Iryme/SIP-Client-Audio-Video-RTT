cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

# Minimal smoke test — verifies the Qt Test framework runs at all
qt_add_executable(test_smoke
    test_smoke.cpp
)
target_link_libraries(test_smoke PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_smoke COMMAND test_smoke)

# ApplicationSettings unit tests
qt_add_executable(test_application_settings
    test_application_settings.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/ApplicationSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
)

target_include_directories(test_application_settings PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_application_settings PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_application_settings COMMAND test_application_settings)
