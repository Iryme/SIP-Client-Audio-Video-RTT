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

# ---- test_sip_profile_editor -------------------------------------------
qt_add_executable(test_sip_profile_editor
    test_sip_profile_editor.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_sip_profile_editor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sip_profile_editor PRIVATE
    Qt6::Core
    Qt6::Test
)
if(WIN32)
    target_link_libraries(test_sip_profile_editor PRIVATE Advapi32)
endif()

add_test(NAME test_sip_profile_editor COMMAND test_sip_profile_editor)

# ---- test_media_device -------------------------------------------------
qt_add_executable(test_media_device
    test_media_device.cpp
    ${CMAKE_SOURCE_DIR}/src/media/MediaDeviceManager.cpp
    ${CMAKE_SOURCE_DIR}/src/media/MediaDeviceSelectionModel.cpp
    ${CMAKE_SOURCE_DIR}/src/media/QtMediaDeviceBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
)

target_include_directories(test_media_device PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_media_device PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
)

add_test(NAME test_media_device COMMAND test_media_device)

# ---- test_registration_state_machine -----------------------------------
qt_add_executable(test_registration_state_machine
    test_registration_state_machine.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
)

target_include_directories(test_registration_state_machine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_registration_state_machine PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_registration_state_machine COMMAND test_registration_state_machine)

# ---- test_sip_manager --------------------------------------------------
qt_add_executable(test_sip_manager
    test_sip_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipManager.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_sip_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sip_manager PRIVATE
    Qt6::Core
    Qt6::Test
)
if(WIN32)
    target_link_libraries(test_sip_manager PRIVATE Advapi32)
endif()

add_test(NAME test_sip_manager COMMAND test_sip_manager)
