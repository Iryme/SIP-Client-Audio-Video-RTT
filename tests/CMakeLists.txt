cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Test Multimedia)

option(BUILD_LIVE_VALIDATION_TOOLS "Build manual live SIP validation tools" OFF)

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

# PJSIP audio mapper — required by any test that compiles SipManager.cpp
set(PJSIP_AUDIO_MAPPER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/sip/PjsipAudioMapper.cpp
)

# Codec manager — required by any test that compiles SipManager.cpp
set(CODEC_MANAGER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/sip/CodecManager.cpp
)

# Call subsystem sources — required by any test that compiles SipManager.cpp
set(CALL_SOURCES
    ${CMAKE_SOURCE_DIR}/src/sip/SipCall.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/CallStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/rtt/RttSession.cpp
)
# SipCall.cpp references PjsipGdiRenderer::deviceIndex() under HAVE_PJSIP+_WIN32.
# Include the renderer so tests link cleanly when PJSIP is enabled on Windows.
# PjsipGdiRenderer.cpp needs Qt6::Widgets and Qt6::Gui.
if(WIN32)
    list(APPEND CALL_SOURCES ${CMAKE_SOURCE_DIR}/src/media/PjsipGdiRenderer.cpp)
    set(CALL_SOURCES_EXTRA_LIBS Qt6::Widgets Qt6::Gui)
else()
    set(CALL_SOURCES_EXTRA_LIBS "")
endif()

# SIP trace sources — required by any test that compiles SipManager.cpp
set(SIP_TRACE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/sip/SipTraceLogger.cpp
)

# Audio + video media sources — required by any test that uses AudioMediaManager or VideoMediaManager
set(AUDIO_MEDIA_SOURCES
    ${CMAKE_SOURCE_DIR}/src/media/AudioMediaManager.cpp
    ${CMAKE_SOURCE_DIR}/src/media/VideoMediaManager.cpp
    ${CMAKE_SOURCE_DIR}/src/media/MediaDeviceManager.cpp
    ${CMAKE_SOURCE_DIR}/src/media/MediaDeviceSelectionModel.cpp
    ${CMAKE_SOURCE_DIR}/src/media/QtMediaDeviceBackend.cpp
)

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
    ${CALL_SOURCES}
    ${AUDIO_MEDIA_SOURCES}
)

target_include_directories(test_registration_state_machine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_registration_state_machine PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
)

add_test(NAME test_registration_state_machine COMMAND test_registration_state_machine)

# ---- test_sip_manager --------------------------------------------------
qt_add_executable(test_sip_manager
    test_sip_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipManager.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRetryPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRefreshConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CALL_SOURCES}
    ${AUDIO_MEDIA_SOURCES}
    ${SIP_TRACE_SOURCES}
    ${PJSIP_AUDIO_MAPPER_SOURCES}
    ${CODEC_MANAGER_SOURCES}
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_sip_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sip_manager PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
)
if(WIN32)
    target_link_libraries(test_sip_manager PRIVATE Advapi32)
endif()

add_test(NAME test_sip_manager COMMAND test_sip_manager)

# ---- test_registration_retry -------------------------------------------
qt_add_executable(test_registration_retry
    test_registration_retry.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipManager.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRetryPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRefreshConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CALL_SOURCES}
    ${AUDIO_MEDIA_SOURCES}
    ${SIP_TRACE_SOURCES}
    ${PJSIP_AUDIO_MAPPER_SOURCES}
    ${CODEC_MANAGER_SOURCES}
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_registration_retry PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_registration_retry PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
)
if(WIN32)
    target_link_libraries(test_registration_retry PRIVATE Advapi32)
endif()

add_test(NAME test_registration_retry COMMAND test_registration_retry)

# ---- test_registration_expiry ------------------------------------------
qt_add_executable(test_registration_expiry
    test_registration_expiry.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipManager.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRetryPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRefreshConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CALL_SOURCES}
    ${AUDIO_MEDIA_SOURCES}
    ${SIP_TRACE_SOURCES}
    ${PJSIP_AUDIO_MAPPER_SOURCES}
    ${CODEC_MANAGER_SOURCES}
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_registration_expiry PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_registration_expiry PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
)
if(WIN32)
    target_link_libraries(test_registration_expiry PRIVATE Advapi32)
endif()

add_test(NAME test_registration_expiry COMMAND test_registration_expiry)

# ---- test_profile_switch -----------------------------------------------
qt_add_executable(test_profile_switch
    test_profile_switch.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipManager.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRetryPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRefreshConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CALL_SOURCES}
    ${AUDIO_MEDIA_SOURCES}
    ${SIP_TRACE_SOURCES}
    ${PJSIP_AUDIO_MAPPER_SOURCES}
    ${CODEC_MANAGER_SOURCES}
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_profile_switch PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_profile_switch PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
)
if(WIN32)
    target_link_libraries(test_profile_switch PRIVATE Advapi32)
endif()

add_test(NAME test_profile_switch COMMAND test_profile_switch)

# ---- test_sip_uri_normalizer -------------------------------------------
qt_add_executable(test_sip_uri_normalizer
    test_sip_uri_normalizer.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipUriNormalizer.cpp
)

target_include_directories(test_sip_uri_normalizer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sip_uri_normalizer PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_sip_uri_normalizer COMMAND test_sip_uri_normalizer)

# ---- test_call_state_machine -------------------------------------------
qt_add_executable(test_call_state_machine
    test_call_state_machine.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/CallStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
)

target_include_directories(test_call_state_machine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_call_state_machine PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_call_state_machine COMMAND test_call_state_machine)

# ---- test_audio_media --------------------------------------------------
qt_add_executable(test_audio_media
    test_audio_media.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/CallStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CALL_SOURCES}
    ${AUDIO_MEDIA_SOURCES}
)

target_include_directories(test_audio_media PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_audio_media PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
)

add_test(NAME test_audio_media COMMAND test_audio_media)

# ---- test_video_media --------------------------------------------------
qt_add_executable(test_video_media
    test_video_media.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/CallStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CALL_SOURCES}
    ${AUDIO_MEDIA_SOURCES}
)

target_include_directories(test_video_media PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_video_media PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
)

add_test(NAME test_video_media COMMAND test_video_media)

# ---- test_codec_manager ------------------------------------------------
qt_add_executable(test_codec_manager
    test_codec_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/CodecManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
)

target_include_directories(test_codec_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_codec_manager PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_codec_manager COMMAND test_codec_manager)

# ---- test_rtt_session --------------------------------------------------
# Note: RttSession.cpp is already part of CALL_SOURCES (added above).
qt_add_executable(test_rtt_session
    test_rtt_session.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CALL_SOURCES}
    ${AUDIO_MEDIA_SOURCES}
)

target_include_directories(test_rtt_session PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_rtt_session PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
)

add_test(NAME test_rtt_session COMMAND test_rtt_session)

# ---- test_sip_trace ----------------------------------------------------
qt_add_executable(test_sip_trace
    test_sip_trace.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${SIP_TRACE_SOURCES}
)

target_include_directories(test_sip_trace PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sip_trace PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_sip_trace COMMAND test_sip_trace)

# PjsipGdiRenderer.cpp (included via CALL_SOURCES on Windows) needs Qt6::Widgets and Qt6::Gui.
if(CALL_SOURCES_EXTRA_LIBS)
    foreach(_call_test IN ITEMS
        test_registration_state_machine
        test_sip_manager
        test_registration_retry
        test_registration_expiry
        test_profile_switch
        test_audio_media
        test_video_media
        test_rtt_session
    )
        if(TARGET ${_call_test})
            target_link_libraries(${_call_test} PRIVATE ${CALL_SOURCES_EXTRA_LIBS})
        endif()
    endforeach()
endif()

if(ENABLE_PJSIP AND PJSIP_FOUND)
    foreach(_pjsip_test_target IN ITEMS
        test_registration_state_machine
        test_sip_manager
        test_registration_retry
        test_registration_expiry
        test_profile_switch
        test_audio_media
        test_video_media
        test_codec_manager
        test_rtt_session
    )
        if(TARGET ${_pjsip_test_target})
            target_link_libraries(${_pjsip_test_target} PRIVATE PJSIP::pjsua2)
        endif()
    endforeach()
endif()

if(BUILD_LIVE_VALIDATION_TOOLS)
    if(NOT ENABLE_PJSIP OR NOT PJSIP_FOUND)
        message(FATAL_ERROR "BUILD_LIVE_VALIDATION_TOOLS requires ENABLE_PJSIP=ON and a real PJSIP backend")
    endif()

    qt_add_executable(live_registration_probe
        live_registration_probe.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipManager.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipCall.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/CallStateMachine.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRetryPolicy.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRefreshConfig.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipTraceLogger.cpp
        ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
        ${AUDIO_MEDIA_SOURCES}
        ${PJSIP_AUDIO_MAPPER_SOURCES}
        ${SECURITY_SOURCES}
        ${SECURITY_HEADERS}
    )

    target_include_directories(live_registration_probe PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(live_registration_probe PRIVATE
        Qt6::Core
        Qt6::Multimedia
        PJSIP::pjsua2
    )

    if(WIN32)
        target_link_libraries(live_registration_probe PRIVATE Advapi32)
    endif()

    qt_add_executable(live_audio_call_probe
        live_audio_call_probe.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipManager.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipCall.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/CallStateMachine.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRetryPolicy.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRefreshConfig.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipTraceLogger.cpp
        ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
        ${AUDIO_MEDIA_SOURCES}
        ${PJSIP_AUDIO_MAPPER_SOURCES}
        ${SECURITY_SOURCES}
        ${SECURITY_HEADERS}
    )

    target_include_directories(live_audio_call_probe PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(live_audio_call_probe PRIVATE
        Qt6::Core
        Qt6::Multimedia
        PJSIP::pjsua2
    )

    if(WIN32)
        target_link_libraries(live_audio_call_probe PRIVATE Advapi32)
    endif()
endif()
