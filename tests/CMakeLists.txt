cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Test Multimedia Network)

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
    # SipManager.cpp calls into MessageHistoryStore (Task W093) from
    # sendSipMessage()/onAccountInstantMessageReceived(); pure Qt, no
    # further link dependency (only uses ComposedSipMessage's fields, not
    # SipMessageComposer's own .cpp).
    ${CMAKE_SOURCE_DIR}/src/sip/MessageHistoryStore.cpp
    # Task W096: SipManager::onAccountInstantMessageReceived/sendImdnReport
    # now call MessagingContentKindDetector::detect, ImdnParser::parse, and
    # SipMessageComposer::composeImdnReport directly (previously
    # SipManager.cpp only referenced ComposedSipMessage's fields).
    ${CMAKE_SOURCE_DIR}/src/sip/MessagingContentKind.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/ImdnParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/ImdnGenerator.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipMessageComposer.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/CpimBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipUriNormalizer.cpp
    # Task W097: SipManager::onAccountInstantMessageReceived now also calls
    # IsComposingParser::parse directly, and SipMessageComposer.cpp now
    # calls IsComposingGenerator::generate.
    ${CMAKE_SOURCE_DIR}/src/sip/IsComposingParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/IsComposingGenerator.cpp
    # Task W098: SipManager::subscribePresence/onAccountBuddyPresenceChanged/
    # schedulePresenceResubscribe call PresenceStore::instance() directly
    # (SipAccount.cpp's Buddy subscription API needs no further .cpp — it
    # only uses PresenceInfo.h, header-only).
    ${CMAKE_SOURCE_DIR}/src/sip/PresenceStore.cpp
    # Task W100: SipCall.cpp's onCallSdpCreated calls
    # MsrpSipIntegration::detectFromSdp() directly (read-only SDP detection).
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpSipIntegration.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpSdpNegotiator.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpPath.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpSessionStore.cpp
    # Task W101: SipCall.cpp's onCallSdpCreated now also splices a real
    # m=message section into the live pjmedia_sdp_session via
    # MsrpSipMediaInjector, and Impl::ensureMsrpSessionForOutgoingSdp() owns
    # a real MsrpSession (passive listener) per call — pulls in the full
    # MSRP session dependency chain.
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpSipMediaInjector.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpTransport.h
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpSession.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpFrameParser.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpFrameSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpChunkAssembler.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpMessageChunker.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpTransactionStore.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpDiagnosticsStore.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpTcpTransport.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpTlsTransport.cpp
    # Task W104: MsrpSession.cpp's sendFile()/fileTransferReceived path calls
    # MsrpFileSelector::sanitizeFileNameForDisplay directly.
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpFileSelector.cpp
    # Task W101 Phase 5: SipManager.cpp's sendSipMessage() now calls
    # MessagingTransportPolicy::decideInitialTransport/
    # decideFallbackAfterMsrpFailure directly.
    ${CMAKE_SOURCE_DIR}/src/msrp/MessagingTransportPolicy.cpp
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
    ${CMAKE_SOURCE_DIR}/src/sip/SipRawMessageParser.cpp
)

# PjsipTraceModule.cpp includes <pjsua-lib/pjsua.h> when HAVE_PJSIP is defined
# (a global compile definition), so it can only be compiled by test targets
# that also get the real PJSIP include dirs/libs (the CALL_SOURCES /
# "foreach _pjsip_test_target" targets below) — never bundled into
# SIP_TRACE_SOURCES, which is also used by plain-Qt tests like test_sip_trace.
set(PJSIP_TRACE_MODULE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/sip/PjsipTraceModule.cpp
)

# Messaging Diagnostics parser sources — CPIM / IMDN / is-composing / SDP MSRP.
# Pure Qt/text parsing, no PJSIP types, so these are reused as-is by both the
# app target and the standalone parser test targets below.
set(MESSAGING_DIAGNOSTICS_SOURCES
    ${CMAKE_SOURCE_DIR}/src/sip/MessagingContentKind.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/CpimParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/ImdnParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/IsComposingParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SdpMsrpDiagnosticsParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipBodyExtractor.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/DeflateDecoder.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/UrlRedactor.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RcsFtHttpParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/MessagingDiagnosticsStore.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/MessagingEventStore.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/InteropTraceExporter.cpp
    # Task W098: InteropTraceExporter.cpp's no-arg exportToJson() overload
    # calls PresenceDiagnosticsStore::instance() directly, which in turn
    # calls PidfParser::parse().
    ${CMAKE_SOURCE_DIR}/src/sip/PidfParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/PresenceDiagnosticsStore.cpp
    # Task W100: InteropTraceExporter.cpp's no-arg exportToJson() overload
    # also calls MsrpSessionStore::instance()/MsrpDiagnosticsStore::instance()
    # directly — both are lightweight, Qt-Network-free singletons (no
    # transport code), so no further dependency is needed.
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpSessionStore.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpDiagnosticsStore.cpp
)

# SIP MESSAGE Foundation sources (Task W092) — composer + CPIM builder, plus
# the URI normalizer they depend on. Pure Qt/text, no PJSIP types; kept
# separate from MESSAGING_DIAGNOSTICS_SOURCES so existing messaging test
# targets that don't need URI normalization stay unaffected.
set(SIP_MESSAGE_FOUNDATION_SOURCES
    ${CMAKE_SOURCE_DIR}/src/sip/CpimBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipMessageComposer.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipUriNormalizer.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/ImdnGenerator.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/IsComposingGenerator.cpp
)

# Message History sources (Task W093). Pure Qt, no PJSIP types; independent
# of MESSAGING_DIAGNOSTICS_SOURCES/MessagingEventStore on purpose — see
# MessageHistoryStore.h for why the two pipelines are kept disjoint.
set(MESSAGE_HISTORY_SOURCES
    ${CMAKE_SOURCE_DIR}/src/sip/MessageHistoryStore.cpp
)

# Audio + video media sources — required by any test that uses AudioMediaManager or VideoMediaManager
set(AUDIO_MEDIA_SOURCES
    ${CMAKE_SOURCE_DIR}/src/media/AudioMediaManager.cpp
    ${CMAKE_SOURCE_DIR}/src/media/VideoMediaManager.cpp
    ${CMAKE_SOURCE_DIR}/src/media/MediaDeviceManager.cpp
    ${CMAKE_SOURCE_DIR}/src/media/MediaDeviceSelectionModel.cpp
    ${CMAKE_SOURCE_DIR}/src/media/QtMediaDeviceBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/media/VideoQualityManager.cpp
    ${CMAKE_SOURCE_DIR}/src/media/VideoSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/media/VideoStatistics.cpp
    ${CMAKE_SOURCE_DIR}/src/media/VideoPipelineMonitor.cpp
)

# XCAP Foundation sources (Task W099). XcapClient.cpp needs Qt6::Network and
# CredentialStore (+ its Logger dependency).
set(XCAP_SOURCES
    ${CMAKE_SOURCE_DIR}/src/sip/XcapUrlRedactor.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/XcapXmlValidator.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/XcapRequestBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/XcapClient.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/XcapDiagnosticsStore.cpp
)

# InteropTraceExporter.cpp's no-arg exportToJson() overload calls
# XcapDiagnosticsStore::instance()/entries() directly (Task W099), so every
# target that links InteropTraceExporter.cpp (i.e. everything already using
# MESSAGING_DIAGNOSTICS_SOURCES) must also link XCAP_SOURCES, or the symbol
# is left unresolved at link time even for tests that never call that
# overload — the linker still requires every function InteropTraceExporter.cpp
# calls to resolve.
list(APPEND MESSAGING_DIAGNOSTICS_SOURCES ${XCAP_SOURCES})

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
    Qt6::Network
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
    ${PJSIP_TRACE_MODULE_SOURCES}
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
    Qt6::Network
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
    ${PJSIP_TRACE_MODULE_SOURCES}
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
    Qt6::Network
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
    ${PJSIP_TRACE_MODULE_SOURCES}
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
    Qt6::Network
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
    ${PJSIP_TRACE_MODULE_SOURCES}
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
    Qt6::Network
)
if(WIN32)
    target_link_libraries(test_profile_switch PRIVATE Advapi32)
endif()

add_test(NAME test_profile_switch COMMAND test_profile_switch)

# ---- test_call_history --------------------------------------------------
qt_add_executable(test_call_history
    test_call_history.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CallHistoryEntry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CallHistoryStore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CallHistoryListModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CallHistoryFilterProxyModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
)

target_include_directories(test_call_history PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_call_history PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_call_history COMMAND test_call_history)

# ---- test_codec_info ----------------------------------------------------
# CodecInfo is header-only, so no src/ file needs to be compiled in.
qt_add_executable(test_codec_info
    test_codec_info.cpp
)

target_include_directories(test_codec_info PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_codec_info PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_codec_info COMMAND test_codec_info)

# ---- test_diagnostics_snapshot -----------------------------------------
qt_add_executable(test_diagnostics_snapshot
    test_diagnostics_snapshot.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DiagnosticsSnapshot.cpp
)

target_include_directories(test_diagnostics_snapshot PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_diagnostics_snapshot PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_diagnostics_snapshot COMMAND test_diagnostics_snapshot)

# ---- test_diagnostics_timeline ------------------------------------------
qt_add_executable(test_diagnostics_timeline
    test_diagnostics_timeline.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DiagnosticsTimelineEntry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DiagnosticsTimelineModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DiagnosticsTimelineFilterProxyModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DiagnosticsTimelineExporter.cpp
)

target_include_directories(test_diagnostics_timeline PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_diagnostics_timeline PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_diagnostics_timeline COMMAND test_diagnostics_timeline)

# ---- test_diagnostics_bundle --------------------------------------------
# DiagnosticsBundleExporter touches DiagnosticsTimelineService, which wires up
# to SipManager/AudioMediaManager/VideoMediaManager/CameraController — the
# same dependency chain test_sip_manager already needs.
qt_add_executable(test_diagnostics_bundle
    test_diagnostics_bundle.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DiagnosticsBundleExporter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DiagnosticsSnapshot.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DiagnosticsTimelineEntry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DiagnosticsTimelineModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DiagnosticsTimelineExporter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DiagnosticsTimelineService.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CallHistoryEntry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CallHistoryStore.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/CameraController.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipManager.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRetryPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRefreshConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
    ${CALL_SOURCES}
    ${AUDIO_MEDIA_SOURCES}
    ${SIP_TRACE_SOURCES}
    ${PJSIP_TRACE_MODULE_SOURCES}
    ${PJSIP_AUDIO_MAPPER_SOURCES}
    ${CODEC_MANAGER_SOURCES}
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_diagnostics_bundle PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_diagnostics_bundle PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
    Qt6::Network
)
if(WIN32)
    target_link_libraries(test_diagnostics_bundle PRIVATE Advapi32)
endif()
if(HAVE_QT_ZIP_WRITER)
    target_link_libraries(test_diagnostics_bundle PRIVATE Qt6::CorePrivate)
    target_compile_definitions(test_diagnostics_bundle PRIVATE HAVE_QT_ZIP_WRITER)
endif()

add_test(NAME test_diagnostics_bundle COMMAND test_diagnostics_bundle)

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
    Qt6::Network
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
    Qt6::Network
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
if(WIN32)
    target_link_libraries(test_codec_manager PRIVATE Ws2_32)
endif()

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
    Qt6::Network
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

# ---- test_sip_raw_message_parser ----------------------------------------
qt_add_executable(test_sip_raw_message_parser
    test_sip_raw_message_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipRawMessageParser.cpp
)

target_include_directories(test_sip_raw_message_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sip_raw_message_parser PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_sip_raw_message_parser COMMAND test_sip_raw_message_parser)

# ---- test_cpim_parser ---------------------------------------------------
qt_add_executable(test_cpim_parser
    test_cpim_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/CpimParser.cpp
)

target_include_directories(test_cpim_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_cpim_parser PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_cpim_parser COMMAND test_cpim_parser)

# ---- test_imdn_parser ----------------------------------------------------
qt_add_executable(test_imdn_parser
    test_imdn_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/ImdnParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/ImdnGenerator.cpp
)

target_include_directories(test_imdn_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_imdn_parser PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_imdn_parser COMMAND test_imdn_parser)

# ---- test_is_composing_parser --------------------------------------------
qt_add_executable(test_is_composing_parser
    test_is_composing_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/IsComposingParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/IsComposingGenerator.cpp
)

target_include_directories(test_is_composing_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_is_composing_parser PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_is_composing_parser COMMAND test_is_composing_parser)

# ---- test_typing_indicator_controller (Task W097) --------------------------
qt_add_executable(test_typing_indicator_controller
    test_typing_indicator_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/TypingIndicatorController.cpp
)

target_include_directories(test_typing_indicator_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_typing_indicator_controller PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_typing_indicator_controller COMMAND test_typing_indicator_controller)

# ---- test_pidf_parser (Task W098) ----------------------------------------
qt_add_executable(test_pidf_parser
    test_pidf_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/PidfParser.cpp
)

target_include_directories(test_pidf_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_pidf_parser PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_pidf_parser COMMAND test_pidf_parser)

# ---- test_presence_store (Task W098) --------------------------------------
qt_add_executable(test_presence_store
    test_presence_store.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/PresenceStore.cpp
)

target_include_directories(test_presence_store PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_presence_store PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_presence_store COMMAND test_presence_store)

# ---- test_presence_diagnostics_store (Task W098) ---------------------------
# Also covers the SUBSCRIBE/NOTIFY raw-trace mapping (Event/Subscription-State/
# Expires header extraction + PIDF parsing) without a live PJSIP stack.
qt_add_executable(test_presence_diagnostics_store
    test_presence_diagnostics_store.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/PidfParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/PresenceDiagnosticsStore.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipTraceLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipRawMessageParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipBodyExtractor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
)

target_include_directories(test_presence_diagnostics_store PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_presence_diagnostics_store PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_presence_diagnostics_store COMMAND test_presence_diagnostics_store)

# ---- test_presence_resubscribe_policy (Task W098) --------------------------
# Header-only pure logic — no PJSIP/network dependency at all.
qt_add_executable(test_presence_resubscribe_policy
    test_presence_resubscribe_policy.cpp
)

target_include_directories(test_presence_resubscribe_policy PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_presence_resubscribe_policy PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_presence_resubscribe_policy COMMAND test_presence_resubscribe_policy)

# ---- test_xcap_url_redactor (Task W099) ------------------------------------
qt_add_executable(test_xcap_url_redactor
    test_xcap_url_redactor.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/XcapUrlRedactor.cpp
)

target_include_directories(test_xcap_url_redactor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_xcap_url_redactor PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_xcap_url_redactor COMMAND test_xcap_url_redactor)

# ---- MSRP Foundation sources (Task W100) -----------------------------------
set(MSRP_PROTOCOL_SOURCES
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpPath.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpSdpNegotiator.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpFrameParser.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpFrameSerializer.cpp
)

# ---- test_msrp_path (Task W100) --------------------------------------------
qt_add_executable(test_msrp_path
    test_msrp_path.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpPath.cpp
)
target_include_directories(test_msrp_path PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_msrp_path PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_msrp_path COMMAND test_msrp_path)

# ---- test_msrp_sdp_negotiator (Task W100) ----------------------------------
qt_add_executable(test_msrp_sdp_negotiator
    test_msrp_sdp_negotiator.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpPath.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpSdpNegotiator.cpp
)
target_include_directories(test_msrp_sdp_negotiator PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_msrp_sdp_negotiator PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_msrp_sdp_negotiator COMMAND test_msrp_sdp_negotiator)

# ---- test_msrp_frame_parser (Task W100) ------------------------------------
qt_add_executable(test_msrp_frame_parser
    test_msrp_frame_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpFrameParser.cpp
)
target_include_directories(test_msrp_frame_parser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_msrp_frame_parser PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_msrp_frame_parser COMMAND test_msrp_frame_parser)

# ---- test_msrp_frame_serializer (Task W100) --------------------------------
qt_add_executable(test_msrp_frame_serializer
    test_msrp_frame_serializer.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpFrameParser.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpFrameSerializer.cpp
)
target_include_directories(test_msrp_frame_serializer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_msrp_frame_serializer PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_msrp_frame_serializer COMMAND test_msrp_frame_serializer)

# ---- test_msrp_chunker (Task W100) -----------------------------------------
qt_add_executable(test_msrp_chunker
    test_msrp_chunker.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpMessageChunker.cpp
)
target_include_directories(test_msrp_chunker PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_msrp_chunker PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_msrp_chunker COMMAND test_msrp_chunker)

# ---- test_msrp_chunk_assembler (Task W100) ---------------------------------
qt_add_executable(test_msrp_chunk_assembler
    test_msrp_chunk_assembler.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpChunkAssembler.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpMessageChunker.cpp
)
target_include_directories(test_msrp_chunk_assembler PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_msrp_chunk_assembler PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_msrp_chunk_assembler COMMAND test_msrp_chunk_assembler)

# ---- test_msrp_transaction_store (Task W100) -------------------------------
qt_add_executable(test_msrp_transaction_store
    test_msrp_transaction_store.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpTransactionStore.cpp
)
target_include_directories(test_msrp_transaction_store PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_msrp_transaction_store PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_msrp_transaction_store COMMAND test_msrp_transaction_store)

# ---- test_messaging_transport_policy (Task W100) ---------------------------
qt_add_executable(test_messaging_transport_policy
    test_messaging_transport_policy.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MessagingTransportPolicy.cpp
)
target_include_directories(test_messaging_transport_policy PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_messaging_transport_policy PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_messaging_transport_policy COMMAND test_messaging_transport_policy)

# ---- test_msrp_payload_dispatcher (Task W100) ------------------------------
# Reuses the existing CPIM/IMDN/is-composing parsers and MessageHistoryStore
# directly — no duplicated parsing (task requirement 8).
qt_add_executable(test_msrp_payload_dispatcher
    test_msrp_payload_dispatcher.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpPayloadDispatcher.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/MessagingContentKind.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/CpimParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/ImdnParser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/IsComposingParser.cpp
    ${SIP_MESSAGE_FOUNDATION_SOURCES}
    ${MESSAGE_HISTORY_SOURCES}
)
target_include_directories(test_msrp_payload_dispatcher PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_msrp_payload_dispatcher PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_msrp_payload_dispatcher COMMAND test_msrp_payload_dispatcher)

# ---- test_msrp_session_harness (Task W100, section U) ----------------------
# Local integration harness: two MsrpSession instances (active + passive)
# exchanging real MSRP frames over 127.0.0.1 with an OS-assigned ephemeral
# port — no dependency on any real MSRP relay/server.
qt_add_executable(test_msrp_session_harness
    test_msrp_session_harness.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpTransport.h
    ${MSRP_PROTOCOL_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpChunkAssembler.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpTransactionStore.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpSessionStore.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpDiagnosticsStore.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpTcpTransport.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpTlsTransport.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpMessageChunker.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpSession.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpFileSelector.cpp
)
target_include_directories(test_msrp_session_harness PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_msrp_session_harness PRIVATE Qt6::Core Qt6::Test Qt6::Network)
add_test(NAME test_msrp_session_harness COMMAND test_msrp_session_harness)

# ---- test_msrp_file_selector (Task W104) -----------------------------------
qt_add_executable(test_msrp_file_selector
    test_msrp_file_selector.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpFileSelector.cpp
)
target_include_directories(test_msrp_file_selector PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_msrp_file_selector PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_msrp_file_selector COMMAND test_msrp_file_selector)

# ---- test_msrp_file_receiver (Task W104) ------------------------------------
qt_add_executable(test_msrp_file_receiver
    test_msrp_file_receiver.cpp
    ${CMAKE_SOURCE_DIR}/src/msrp/MsrpFileReceiver.cpp
)
target_include_directories(test_msrp_file_receiver PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_msrp_file_receiver PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_msrp_file_receiver COMMAND test_msrp_file_receiver)

# ---- test_xcap_xml_validator (Task W099) -----------------------------------
qt_add_executable(test_xcap_xml_validator
    test_xcap_xml_validator.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/XcapXmlValidator.cpp
)

target_include_directories(test_xcap_xml_validator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_xcap_xml_validator PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_xcap_xml_validator COMMAND test_xcap_xml_validator)

# ---- test_xcap_request_builder (Task W099) ---------------------------------
# Pure request-model construction — no QNetworkAccessManager dependency.
qt_add_executable(test_xcap_request_builder
    test_xcap_request_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/XcapRequestBuilder.cpp
)

target_include_directories(test_xcap_request_builder PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_xcap_request_builder PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_xcap_request_builder COMMAND test_xcap_request_builder)

# ---- test_xcap_auth_header_builder (Task W099) -----------------------------
qt_add_executable(test_xcap_auth_header_builder
    test_xcap_auth_header_builder.cpp
)

target_include_directories(test_xcap_auth_header_builder PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_xcap_auth_header_builder PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_xcap_auth_header_builder COMMAND test_xcap_auth_header_builder)

# ---- test_xcap_diagnostics_store (Task W099) -------------------------------
# XcapDiagnosticsStore.cpp's constructor connects to XcapClient::instance(),
# which needs Qt6::Network + CredentialStore (+ its Logger dependency); the
# export test also links InteropTraceExporter.cpp, which needs the full
# Messaging/Presence diagnostics chain (all its called functions must
# resolve even though this test only exercises the xcapEvents section).
qt_add_executable(test_xcap_diagnostics_store
    test_xcap_diagnostics_store.cpp
    ${SIP_TRACE_SOURCES}
    ${MESSAGING_DIAGNOSTICS_SOURCES}
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
)

target_include_directories(test_xcap_diagnostics_store PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_xcap_diagnostics_store PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)
if(WIN32)
    target_link_libraries(test_xcap_diagnostics_store PRIVATE Advapi32)
endif()

add_test(NAME test_xcap_diagnostics_store COMMAND test_xcap_diagnostics_store)

# ---- test_sdp_msrp_diagnostics_parser ------------------------------------
qt_add_executable(test_sdp_msrp_diagnostics_parser
    test_sdp_msrp_diagnostics_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SdpMsrpDiagnosticsParser.cpp
)

target_include_directories(test_sdp_msrp_diagnostics_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sdp_msrp_diagnostics_parser PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_sdp_msrp_diagnostics_parser COMMAND test_sdp_msrp_diagnostics_parser)

# ---- test_deflate_decoder (Task W095) ------------------------------------
qt_add_executable(test_deflate_decoder
    test_deflate_decoder.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/DeflateDecoder.cpp
)

target_include_directories(test_deflate_decoder PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_deflate_decoder PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_deflate_decoder COMMAND test_deflate_decoder)

# ---- test_sip_body_extractor (Task W095) ----------------------------------
qt_add_executable(test_sip_body_extractor
    test_sip_body_extractor.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SipBodyExtractor.cpp
)

target_include_directories(test_sip_body_extractor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sip_body_extractor PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_sip_body_extractor COMMAND test_sip_body_extractor)

# ---- test_rcs_ft_http_parser (Task W095) -----------------------------------
qt_add_executable(test_rcs_ft_http_parser
    test_rcs_ft_http_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/RcsFtHttpParser.cpp
)

target_include_directories(test_rcs_ft_http_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_rcs_ft_http_parser PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_rcs_ft_http_parser COMMAND test_rcs_ft_http_parser)

# ---- test_url_redactor (Task W095) -----------------------------------------
qt_add_executable(test_url_redactor
    test_url_redactor.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/UrlRedactor.cpp
)

target_include_directories(test_url_redactor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_url_redactor PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_url_redactor COMMAND test_url_redactor)

# ---- test_messaging_diagnostics_store ------------------------------------
qt_add_executable(test_messaging_diagnostics_store
    test_messaging_diagnostics_store.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${SIP_TRACE_SOURCES}
    ${MESSAGING_DIAGNOSTICS_SOURCES}
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_messaging_diagnostics_store PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_messaging_diagnostics_store PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)
if(WIN32)
    target_link_libraries(test_messaging_diagnostics_store PRIVATE Advapi32)
endif()

add_test(NAME test_messaging_diagnostics_store COMMAND test_messaging_diagnostics_store)

# ---- test_messaging_event_store ------------------------------------------
qt_add_executable(test_messaging_event_store
    test_messaging_event_store.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${SIP_TRACE_SOURCES}
    ${MESSAGING_DIAGNOSTICS_SOURCES}
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_messaging_event_store PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_messaging_event_store PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)
if(WIN32)
    target_link_libraries(test_messaging_event_store PRIVATE Advapi32)
endif()

add_test(NAME test_messaging_event_store COMMAND test_messaging_event_store)

# ---- test_sip_message_foundation -----------------------------------------
qt_add_executable(test_sip_message_foundation
    test_sip_message_foundation.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${SIP_TRACE_SOURCES}
    ${MESSAGING_DIAGNOSTICS_SOURCES}
    ${SIP_MESSAGE_FOUNDATION_SOURCES}
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_sip_message_foundation PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_sip_message_foundation PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)
if(WIN32)
    target_link_libraries(test_sip_message_foundation PRIVATE Advapi32)
endif()

add_test(NAME test_sip_message_foundation COMMAND test_sip_message_foundation)

# ---- test_message_history -------------------------------------------------
qt_add_executable(test_message_history
    test_message_history.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/MessagingContentKind.cpp
    ${SIP_MESSAGE_FOUNDATION_SOURCES}
    ${MESSAGE_HISTORY_SOURCES}
)

target_include_directories(test_message_history PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_message_history PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_message_history COMMAND test_message_history)

# ---- test_windows_trace_json_export ---------------------------------------
qt_add_executable(test_windows_trace_json_export
    test_windows_trace_json_export.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${SIP_TRACE_SOURCES}
    ${MESSAGING_DIAGNOSTICS_SOURCES}
    ${SECURITY_SOURCES}
    ${SECURITY_HEADERS}
)

target_include_directories(test_windows_trace_json_export PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_windows_trace_json_export PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)
if(WIN32)
    target_link_libraries(test_windows_trace_json_export PRIVATE Advapi32)
endif()

add_test(NAME test_windows_trace_json_export COMMAND test_windows_trace_json_export)

# ---- test_trace_comparator (Task W102 Phase 11) ---------------------------
# TraceComparator.cpp has no dependencies beyond QtCore/QtJson — a minimal,
# self-contained target rather than pulling in the full messaging pipeline.
qt_add_executable(test_trace_comparator
    test_trace_comparator.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/TraceComparator.cpp
)

target_include_directories(test_trace_comparator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_trace_comparator PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_trace_comparator COMMAND test_trace_comparator)

# ---- test_lmpe_codec_unconfirmed (Task W103) -------------------------------
# UnconfirmedLmpeCodec is a header-only fail-closed stub with no dependencies
# beyond QtCore — guards against ever silently claiming LMPE compatibility
# before the real ETSI TS 103 698 format is confirmed. See
# docs/agent-results/W103-lmpe-foundation-result.md.
qt_add_executable(test_lmpe_codec_unconfirmed
    test_lmpe_codec_unconfirmed.cpp
)

target_include_directories(test_lmpe_codec_unconfirmed PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_lmpe_codec_unconfirmed PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_lmpe_codec_unconfirmed COMMAND test_lmpe_codec_unconfirmed)

# ---- test_msrp_sip_media_injector (Task W101, Phase 2) ---------------------
# Proves MsrpSipMediaInjector writes a real "m=message" section into an
# actual pjmedia_sdp_session and that pjmedia_sdp_print() emits it on the
# wire — not just an internal Qt model. Needs real pjmedia/pjlib, so it only
# exists when ENABLE_PJSIP=ON.
if(ENABLE_PJSIP AND PJSIP_FOUND)
    qt_add_executable(test_msrp_sip_media_injector
        test_msrp_sip_media_injector.cpp
        ${CMAKE_SOURCE_DIR}/src/msrp/MsrpSipMediaInjector.cpp
        ${CMAKE_SOURCE_DIR}/src/msrp/MsrpPath.cpp
    )
    target_include_directories(test_msrp_sip_media_injector PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(test_msrp_sip_media_injector PRIVATE PJMEDIA_HAS_VIDEO=1)
    target_link_libraries(test_msrp_sip_media_injector PRIVATE
        Qt6::Core
        Qt6::Test
        PJSIP::pjsua2
        "${CMAKE_SOURCE_DIR}/.deps/pjsip-msvc-install/bin/pjlib.lib"
    )
    if(WIN32)
        target_link_libraries(test_msrp_sip_media_injector PRIVATE ws2_32)
    endif()
    if(TARGET Pj::pjlib)
        target_link_libraries(test_msrp_sip_media_injector PRIVATE Pj::pjlib)
    endif()
    add_test(NAME test_msrp_sip_media_injector COMMAND test_msrp_sip_media_injector)
endif()

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
        test_diagnostics_bundle
    )
        if(TARGET ${_call_test})
            target_link_libraries(${_call_test} PRIVATE ${CALL_SOURCES_EXTRA_LIBS})
        endif()
    endforeach()
endif()

if(ENABLE_PJSIP AND PJSIP_FOUND)
    set(_PJSIP_TEST_BIN_DIR "${CMAKE_SOURCE_DIR}/.deps/pjsip-msvc-install/bin")
    set(_PJSIP_TEST_THIRD_PARTY_DIR "${_PJSIP_TEST_BIN_DIR}/pjproject/third_party")
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
        test_diagnostics_bundle
    )
        if(TARGET ${_pjsip_test_target})
            target_link_libraries(${_pjsip_test_target} PRIVATE PJSIP::pjsua2)
            target_compile_definitions(${_pjsip_test_target} PRIVATE PJMEDIA_HAS_VIDEO=1)
            target_link_libraries(${_pjsip_test_target} PRIVATE
                "${_PJSIP_TEST_BIN_DIR}/pjlib.lib"
                "${_PJSIP_TEST_THIRD_PARTY_DIR}/resample.lib"
                "${_PJSIP_TEST_THIRD_PARTY_DIR}/g7221.lib"
                "${_PJSIP_TEST_THIRD_PARTY_DIR}/gsm.lib"
                "${_PJSIP_TEST_THIRD_PARTY_DIR}/ilbc.lib"
                "${_PJSIP_TEST_THIRD_PARTY_DIR}/speex.lib"
                "${_PJSIP_TEST_THIRD_PARTY_DIR}/srtp.lib"
                "${_PJSIP_TEST_THIRD_PARTY_DIR}/webrtc.lib"
                "${_PJSIP_TEST_THIRD_PARTY_DIR}/webrtc_aec3.lib"
                "${_PJSIP_TEST_THIRD_PARTY_DIR}/yuv.lib"
            )
            if(TARGET Pj::pjlib)
                target_link_libraries(${_pjsip_test_target} PRIVATE Pj::pjlib)
            endif()
        endif()
    endforeach()
endif()

# ---- Emergency module sources (no PJSIP dependency) --------------------
set(EMERGENCY_SOURCES
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocationProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallController.cpp
)

# ---- test_emergency_state_machine --------------------------------------
qt_add_executable(test_emergency_state_machine
    test_emergency_state_machine.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallStateMachine.cpp
)

target_include_directories(test_emergency_state_machine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_emergency_state_machine PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_emergency_state_machine COMMAND test_emergency_state_machine)

# ---- test_emergency_profile --------------------------------------------
qt_add_executable(test_emergency_profile
    test_emergency_profile.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallProfile.cpp
)

target_include_directories(test_emergency_profile PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_emergency_profile PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_emergency_profile COMMAND test_emergency_profile)

# ---- test_emergency_location_provider ----------------------------------
qt_add_executable(test_emergency_location_provider
    test_emergency_location_provider.cpp
    ${EMERGENCY_SOURCES}
)

target_include_directories(test_emergency_location_provider PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_emergency_location_provider PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_emergency_location_provider COMMAND test_emergency_location_provider)

# ---- test_emergency_invite_builder -------------------------------------
qt_add_executable(test_emergency_invite_builder
    test_emergency_invite_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyInviteBuilder.cpp
)

target_include_directories(test_emergency_invite_builder PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_emergency_invite_builder PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_emergency_invite_builder COMMAND test_emergency_invite_builder)

# ---- Emergency module sources (location + PIDF-LO, no PJSIP) ---------------
set(EMERGENCY_LOCATION_SOURCES
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocation.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocationProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/PidfLoBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/StaticLocationProvider.cpp
)

# ---- test_emergency_location -------------------------------------------
qt_add_executable(test_emergency_location
    test_emergency_location.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocation.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocationProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/PidfLoBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/StaticLocationProvider.cpp
)

target_include_directories(test_emergency_location PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_emergency_location PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_emergency_location COMMAND test_emergency_location)

# ---- test_pidf_lo_builder ----------------------------------------------
qt_add_executable(test_pidf_lo_builder
    test_pidf_lo_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocation.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/PidfLoBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyInviteBuilder.cpp
)

target_include_directories(test_pidf_lo_builder PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_pidf_lo_builder PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_pidf_lo_builder COMMAND test_pidf_lo_builder)

# ---- test_emergency_sip_integration ------------------------------------
qt_add_executable(test_emergency_sip_integration
    test_emergency_sip_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyInviteBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocation.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/PidfLoBuilder.cpp
)

target_include_directories(test_emergency_sip_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_emergency_sip_integration PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_emergency_sip_integration COMMAND test_emergency_sip_integration)

# ---- test_emergency_multipart_builder ----------------------------------
qt_add_executable(test_emergency_multipart_builder
    test_emergency_multipart_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyMultipartBuilder.cpp
)

target_include_directories(test_emergency_multipart_builder PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_emergency_multipart_builder PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_emergency_multipart_builder COMMAND test_emergency_multipart_builder)

# ---- test_emergency_protocol_validation (Task 40) ----------------------
qt_add_executable(test_emergency_protocol_validation
    test_emergency_protocol_validation.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyInviteBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocation.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/PidfLoBuilder.cpp
)

target_include_directories(test_emergency_protocol_validation PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_emergency_protocol_validation PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_emergency_protocol_validation COMMAND test_emergency_protocol_validation)

# ---- test_emergency_gui_wiring (Task 42) -----------------------------------
qt_add_executable(test_emergency_gui_wiring
    test_emergency_gui_wiring.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AppSettings.h
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallController.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyInviteBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocation.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocationProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/PidfLoBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/StaticLocationProvider.cpp
)

target_include_directories(test_emergency_gui_wiring PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_emergency_gui_wiring PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_emergency_gui_wiring COMMAND test_emergency_gui_wiring)

# ---- test_emergency_location_update (Task 43) ------------------------------
qt_add_executable(test_emergency_location_update
    test_emergency_location_update.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallController.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyInviteBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocation.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocationProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/PidfLoBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/emergency/StaticLocationProvider.cpp
)
target_include_directories(test_emergency_location_update PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_emergency_location_update PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_emergency_location_update COMMAND test_emergency_location_update)

if(BUILD_LIVE_VALIDATION_TOOLS)
    if(NOT ENABLE_PJSIP OR NOT PJSIP_FOUND)
        message(FATAL_ERROR "BUILD_LIVE_VALIDATION_TOOLS requires ENABLE_PJSIP=ON and a real PJSIP backend")
    endif()

    qt_add_executable(live_registration_probe
        live_registration_probe.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipManager.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRetryPolicy.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRefreshConfig.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
        ${SIP_TRACE_SOURCES}
        ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
        ${CALL_SOURCES}
        ${CODEC_MANAGER_SOURCES}
        ${AUDIO_MEDIA_SOURCES}
        ${PJSIP_AUDIO_MAPPER_SOURCES}
        ${SECURITY_SOURCES}
        ${SECURITY_HEADERS}
        ${PJSIP_TRACE_MODULE_SOURCES}
    )

    target_include_directories(live_registration_probe PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(live_registration_probe PRIVATE
        Qt6::Core
        Qt6::Multimedia
        Qt6::Network
        PJSIP::pjsua2
        ${CALL_SOURCES_EXTRA_LIBS}
    )

    if(WIN32)
        target_link_libraries(live_registration_probe PRIVATE Advapi32)
    endif()

    # ---- live_xcap_probe (manual live validation) ---------------------------
    # Pure Qt/Network — no PJSIP dependency — grouped here with the other live
    # probes for consistency. Exercises XcapClient's real HTTP GET + Digest
    # auth path against a live XCAP server.
    qt_add_executable(live_xcap_probe
        live_xcap_probe.cpp
        ${XCAP_SOURCES}
        ${SECURITY_SOURCES}
        ${SECURITY_HEADERS}
        ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    )

    target_include_directories(live_xcap_probe PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(live_xcap_probe PRIVATE
        Qt6::Core
        Qt6::Network
    )

    if(WIN32)
        target_link_libraries(live_xcap_probe PRIVATE Advapi32)
    endif()

    qt_add_executable(live_audio_call_probe
        live_audio_call_probe.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipManager.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRetryPolicy.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRefreshConfig.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
        ${SIP_TRACE_SOURCES}
        ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
        ${CALL_SOURCES}
        ${CODEC_MANAGER_SOURCES}
        ${AUDIO_MEDIA_SOURCES}
        ${PJSIP_AUDIO_MAPPER_SOURCES}
        ${SECURITY_SOURCES}
        ${SECURITY_HEADERS}
        ${PJSIP_TRACE_MODULE_SOURCES}
    )

    target_include_directories(live_audio_call_probe PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(live_audio_call_probe PRIVATE
        Qt6::Core
        Qt6::Multimedia
        Qt6::Network
        PJSIP::pjsua2
        ${CALL_SOURCES_EXTRA_LIBS}
    )

    if(WIN32)
        target_link_libraries(live_audio_call_probe PRIVATE Advapi32)
    endif()

    # ---- live_emergency_call_probe (Task 40) --------------------------------
    qt_add_executable(live_emergency_call_probe
        live_emergency_call_probe.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipManager.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipAccount.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationStateMachine.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRetryPolicy.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/RegistrationRefreshConfig.cpp
        ${CMAKE_SOURCE_DIR}/src/sip/SipProfileManager.cpp
        ${SIP_TRACE_SOURCES}
        ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
        ${CALL_SOURCES}
        ${CODEC_MANAGER_SOURCES}
        ${AUDIO_MEDIA_SOURCES}
        ${PJSIP_AUDIO_MAPPER_SOURCES}
        ${SECURITY_SOURCES}
        ${SECURITY_HEADERS}
        ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallAdapter.cpp
        ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyCallProfile.cpp
        ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyInviteBuilder.cpp
        ${CMAKE_SOURCE_DIR}/src/emergency/EmergencyLocation.cpp
        ${CMAKE_SOURCE_DIR}/src/emergency/PidfLoBuilder.cpp
        ${PJSIP_TRACE_MODULE_SOURCES}
    )

    target_include_directories(live_emergency_call_probe PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(live_emergency_call_probe PRIVATE
        Qt6::Core
        Qt6::Multimedia
        Qt6::Network
        PJSIP::pjsua2
        ${CALL_SOURCES_EXTRA_LIBS}
    )

    if(WIN32)
        target_link_libraries(live_emergency_call_probe PRIVATE Advapi32)
    endif()
endif()
