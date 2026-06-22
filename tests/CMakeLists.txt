cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

# Common security sources for tests
set(SECURITY_SOURCES
    ${CMAKE_SOURCE_DIR}/src/security/CredentialStore.cpp
    ${CMAKE_SOURCE_DIR}/src/security/MemoryCredentialBackend.cpp
)
set(SECURITY_HEADERS
    ${CMAKE_SOURCE_DIR}/src/security/ICredentialBackend.h
    ${CMAKE_SOURCE_DIR}/src/security/CredentialStore.h
    ${CMAKE_SOURCE_DIR}/src/security/MemoryCredentialBackend.h
)
if(WIN32)
    list(APPEND SECURITY_SOURCES ${CMAKE_SOURCE_DIR}/src/security/WindowsCredentialBackend.cpp)
    list(APPEND SECURITY_HEADERS ${CMAKE_SOURCE_DIR}/src/security/WindowsCredentialBackend.h)
endif()

# ---- test_sip_profile_manager ------------------------------------------
qt_add_executable(test_sip_profile_manager
    test_sip_profile_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_sip_profile_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sip_profile_manager PRIVATE
    Qt6::Core
    Qt6::Test
)
if(WIN32)
    target_link_libraries(test_sip_profile_manager PRIVATE Advapi32)
endif()

add_test(NAME test_sip_profile_manager COMMAND test_sip_profile_manager)

# ---- test_credential_store ---------------------------------------------
qt_add_executable(test_credential_store
    test_credential_store.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_credential_store PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_credential_store PRIVATE
    Qt6::Core
    Qt6::Test
)
if(WIN32)
    target_link_libraries(test_credential_store PRIVATE Advapi32)
endif()

add_test(NAME test_credential_store COMMAND test_credential_store)
