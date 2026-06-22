cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

# SipProfileManager unit tests
qt_add_executable(test_sip_profile_manager
    test_sip_profile_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
)

target_include_directories(test_sip_profile_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sip_profile_manager PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_sip_profile_manager COMMAND test_sip_profile_manager)
