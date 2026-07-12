cmake_minimum_required(VERSION 3.16)

# NMake + MSVC can generate malformed compiler dependency files in this
# environment. Fall back to CMake's legacy dependency discovery so the
# generated makefiles stay readable by nmake.
set(CMAKE_DEPENDS_USE_COMPILER FALSE CACHE BOOL
    "Use CMake dependency discovery instead of compiler-generated deps" FORCE)

project(SIPClient
    VERSION 0.1.0
    DESCRIPTION "Generic SIP Multimedia Client - Audio / Video / RTT"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# --- CMake modules ---
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# --- Find Qt6 ---
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
    Network
    Multimedia
    MultimediaWidgets
    Svg
)

# --- Optional: Qt private QZipWriter (Diagnostics Bundle real ZIP export) ---
# QZipWriter/QZipReader live in Qt6::CorePrivate (private, unstable API but
# widely relied on for exactly this — basic in-process ZIP writing without a
# new third-party dependency). If this component isn't available in a given
# Qt build, DiagnosticsBundleExporter falls back to a plain folder export.
find_package(Qt6CorePrivate QUIET)
if(TARGET Qt6::CorePrivate)
    set(HAVE_QT_ZIP_WRITER TRUE)
    message(STATUS "Qt6::CorePrivate found — Diagnostics Bundle export will produce real ZIP files")
else()
    message(STATUS "Qt6::CorePrivate NOT found — Diagnostics Bundle export will fall back to a folder")
endif()

# --- Optional: PJSIP/pjsua2 SIP stack ---
# Pass -DENABLE_PJSIP=ON and optionally -DPJSIP_DIR=<path> to enable.
# When disabled (default), a stub SIP backend is compiled in its place.
option(ENABLE_PJSIP "Enable PJSIP/pjsua2 SIP stack integration" OFF)

if(ENABLE_PJSIP)
    find_package(PJSIP REQUIRED)
    message(STATUS "PJSIP found — building with real SIP backend")
    add_compile_definitions(HAVE_PJSIP)
else()
    message(STATUS "ENABLE_PJSIP=OFF — building with stub SIP backend")
endif()

# --- Version header (Diagnostics Center: System tab) ---
# Best-effort short git commit hash; "unknown" outside a git checkout or
# when git is unavailable. Configured once at cmake-configure time.
find_package(Git QUIET)
set(APP_GIT_COMMIT_HASH "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _git_hash
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _git_result
    )
    if(_git_result EQUAL 0 AND _git_hash)
        set(APP_GIT_COMMIT_HASH "${_git_hash}")
    endif()
endif()
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/AppVersion.h.in"
    "${CMAKE_BINARY_DIR}/generated/AppVersion.h"
    @ONLY
)

# --- Sources ---
set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/gui/MainWindow.cpp
    src/gui/dashboard/DashboardPage.cpp
    src/gui/dashboard/DashboardHeader.cpp
    src/gui/dashboard/DashboardShortcutCard.cpp
    src/gui/dashboard/DashboardStatistics.cpp
    src/gui/dashboard/DashboardRecentEvents.cpp
    src/gui/panels/NavRail.cpp
    src/gui/panels/SidebarPanel.cpp
    src/gui/panels/ContactsPanel.cpp
    src/gui/panels/CallPanel.cpp
    src/gui/panels/CallHistoryPanel.cpp
    src/gui/panels/VideoPanel.cpp
    src/gui/CameraController.cpp
    src/gui/panels/RttPanel.cpp
    src/gui/panels/DiagnosticsPanel.cpp
    src/gui/panels/MediaPanel.cpp
    src/gui/panels/SettingsPanel.cpp
    src/gui/panels/SipLadderPage.cpp
    src/gui/SipLadderWidget.cpp
    src/gui/SipMessageDetailsDialog.cpp
    src/gui/panels/MessagingDiagnosticsPage.cpp
    src/gui/panels/PresencePage.cpp
    src/gui/panels/XcapPage.cpp
    src/gui/panels/MsrpPage.cpp
    src/gui/MessagingMessageDetailsDialog.cpp
    src/gui/theme/ThemeManager.cpp
    src/gui/widgets/FlowLayout.cpp
    src/gui/widgets/StatusCard.cpp
    src/gui/widgets/AudioLevelMeter.cpp
    src/sip/SipTraceLogger.cpp
    src/sip/SipRawMessageParser.cpp
    src/sip/PjsipTraceModule.cpp
    src/sip/MessagingContentKind.cpp
    src/sip/CpimParser.cpp
    src/sip/ImdnParser.cpp
    src/sip/ImdnGenerator.cpp
    src/sip/IsComposingParser.cpp
    src/sip/IsComposingGenerator.cpp
    src/sip/TypingIndicatorController.cpp
    src/sip/SdpMsrpDiagnosticsParser.cpp
    src/sip/SipBodyExtractor.cpp
    src/sip/DeflateDecoder.cpp
    src/sip/UrlRedactor.cpp
    src/sip/RcsFtHttpParser.cpp
    src/sip/MessagingDiagnosticsStore.cpp
    src/sip/MessagingEventStore.cpp
    src/sip/CpimBuilder.cpp
    src/sip/SipMessageComposer.cpp
    src/sip/MessageHistoryStore.cpp
    src/sip/InteropTraceExporter.cpp
    src/sip/PidfParser.cpp
    src/sip/PresenceStore.cpp
    src/sip/PresenceDiagnosticsStore.cpp
    src/sip/XcapUrlRedactor.cpp
    src/sip/XcapXmlValidator.cpp
    src/sip/XcapRequestBuilder.cpp
    src/sip/XcapClient.cpp
    src/sip/XcapDiagnosticsStore.cpp
    src/msrp/MsrpPath.cpp
    src/msrp/MsrpSdpNegotiator.cpp
    src/msrp/MsrpFrameParser.cpp
    src/msrp/MsrpFrameSerializer.cpp
    src/msrp/MsrpChunkAssembler.cpp
    src/msrp/MsrpTransactionStore.cpp
    src/msrp/MsrpSessionStore.cpp
    src/msrp/MsrpDiagnosticsStore.cpp
    src/msrp/MsrpTcpTransport.cpp
    src/msrp/MsrpTlsTransport.cpp
    src/msrp/MsrpMessageChunker.cpp
    src/msrp/MsrpSession.cpp
    src/msrp/MsrpPayloadDispatcher.cpp
    src/msrp/MessagingTransportPolicy.cpp
    src/msrp/MsrpSipIntegration.cpp
    src/msrp/MsrpSipMediaInjector.cpp
    src/gui/widgets/AppStatusBar.cpp
    src/gui/dialogs/SipProfileEditorDialog.cpp
    src/gui/dialogs/IncomingCallDialog.cpp
    src/gui/dialogs/MediaRequestDialog.cpp
    src/core/Logger.cpp
    src/core/ContactStore.cpp
    src/core/CallHistoryEntry.cpp
    src/core/CallHistoryStore.cpp
    src/core/CallHistoryRecorder.cpp
    src/core/CallHistoryListModel.cpp
    src/core/CallHistoryFilterProxyModel.cpp
    src/core/DiagnosticsSnapshot.cpp
    src/core/DiagnosticsCollector.cpp
    src/core/DiagnosticsBundleExporter.cpp
    src/core/DiagnosticsTimelineEntry.cpp
    src/core/DiagnosticsTimelineModel.cpp
    src/core/DiagnosticsTimelineFilterProxyModel.cpp
    src/core/DiagnosticsTimelineExporter.cpp
    src/core/DiagnosticsTimelineService.cpp
    src/gui/panels/DiagnosticsCenterPanel.cpp
    src/sip/SipManager.cpp
    src/sip/SipAccount.cpp
    src/sip/SipCall.cpp
    src/sip/SipUriNormalizer.cpp
    src/sip/CodecManager.cpp
    src/sip/PjsipAudioMapper.cpp
    src/sip/CallStateMachine.cpp
    src/sip/RegistrationStateMachine.cpp
    src/sip/RegistrationRetryPolicy.cpp
    src/sip/RegistrationRefreshConfig.cpp
    src/sip/SipProfileManager.cpp
    src/security/CredentialStore.cpp
    src/security/MemoryCredentialBackend.cpp
    src/media/AudioMediaManager.cpp
    src/media/VideoMediaManager.cpp
    src/media/MediaDeviceManager.cpp
    src/media/MediaDeviceSelectionModel.cpp
    src/media/QtMediaDeviceBackend.cpp
    src/media/PjsipGdiRenderer.cpp
    src/media/VideoSettings.cpp
    src/media/VideoQualityManager.cpp
    src/media/VideoStatistics.cpp
    src/media/VideoPipelineMonitor.cpp
    src/gui/panels/VideoSettingsPanel.cpp
    src/gui/panels/MediaSettingsPanel.cpp
    src/rtt/RttSession.cpp
    # Emergency module — no PJSIP dependency; wired to GUI in Task 42
    src/emergency/EmergencyCallAdapter.cpp
    src/emergency/EmergencyCallController.cpp
    src/emergency/EmergencyCallProfile.cpp
    src/emergency/EmergencyCallStateMachine.cpp
    src/emergency/EmergencyInviteBuilder.cpp
    src/emergency/EmergencyLocation.cpp
    src/emergency/EmergencyLocationProvider.cpp
    src/emergency/EmergencyMultipartBuilder.cpp
    src/emergency/PidfLoBuilder.cpp
    src/emergency/StaticLocationProvider.cpp
)

set(HEADERS
    src/app/Application.h
    src/gui/MainWindow.h
    src/gui/dashboard/DashboardPage.h
    src/gui/dashboard/DashboardHeader.h
    src/gui/dashboard/DashboardShortcutCard.h
    src/gui/dashboard/DashboardStatistics.h
    src/gui/dashboard/DashboardRecentEvents.h
    src/gui/panels/NavRail.h
    src/gui/panels/SidebarPanel.h
    src/gui/panels/ContactsPanel.h
    src/gui/panels/CallPanel.h
    src/gui/panels/CallHistoryPanel.h
    src/gui/panels/VideoPanel.h
    src/gui/CameraController.h
    src/gui/panels/RttPanel.h
    src/gui/panels/DiagnosticsPanel.h
    src/gui/panels/MediaPanel.h
    src/gui/panels/SettingsPanel.h
    src/gui/panels/SipLadderPage.h
    src/gui/SipLadderWidget.h
    src/gui/SipMessageDetailsDialog.h
    src/gui/panels/MessagingDiagnosticsPage.h
    src/gui/panels/PresencePage.h
    src/gui/panels/XcapPage.h
    src/gui/panels/MsrpPage.h
    src/gui/MessagingMessageDetailsDialog.h
    src/sip/SipMessageTrace.h
    src/sip/SipTraceLogger.h
    src/sip/SipRawMessageParser.h
    src/sip/PjsipTraceModule.h
    src/sip/MessagingContentKind.h
    src/sip/CpimInfo.h
    src/sip/CpimParser.h
    src/sip/ImdnInfo.h
    src/sip/ImdnParser.h
    src/sip/ImdnGenerator.h
    src/sip/IsComposingInfo.h
    src/sip/IsComposingParser.h
    src/sip/IsComposingGenerator.h
    src/sip/TypingIndicatorController.h
    src/sip/SdpMsrpInfo.h
    src/sip/SdpMsrpDiagnosticsParser.h
    src/sip/SipBodyExtractor.h
    src/sip/DeflateDecoder.h
    src/sip/UrlRedactor.h
    src/sip/RcsFtHttpInfo.h
    src/sip/RcsFtHttpParser.h
    src/sip/MessagingTraceEntry.h
    src/sip/MessagingDiagnosticsStore.h
    src/sip/MessagingEvent.h
    src/sip/MessagingEventStore.h
    src/sip/CpimBuilder.h
    src/sip/SipMessageComposer.h
    src/sip/MessageHistoryEntry.h
    src/sip/MessageHistoryStore.h
    src/sip/InteropTraceExporter.h
    src/sip/PresenceInfo.h
    src/sip/PidfParser.h
    src/sip/PresenceResubscribePolicy.h
    src/sip/PresenceStore.h
    src/sip/PresenceTraceEntry.h
    src/sip/PresenceDiagnosticsStore.h
    src/sip/XcapModels.h
    src/sip/XcapAuthHeaderBuilder.h
    src/sip/XcapUrlRedactor.h
    src/sip/XcapXmlValidator.h
    src/sip/XcapRequestBuilder.h
    src/sip/XcapClient.h
    src/sip/XcapDiagnosticsStore.h
    src/msrp/MsrpTypes.h
    src/msrp/MsrpSessionInfo.h
    src/msrp/MsrpPath.h
    src/msrp/MsrpSdpNegotiator.h
    src/msrp/MsrpFrame.h
    src/msrp/MsrpFrameParser.h
    src/msrp/MsrpFrameSerializer.h
    src/msrp/MsrpChunkAssembler.h
    src/msrp/MsrpTransaction.h
    src/msrp/MsrpTransactionStore.h
    src/msrp/MsrpSessionStore.h
    src/msrp/MsrpDiagnosticsEvent.h
    src/msrp/MsrpDiagnosticsStore.h
    src/msrp/MsrpTransport.h
    src/msrp/MsrpTcpTransport.h
    src/msrp/MsrpTlsTransport.h
    src/msrp/MsrpMessageChunker.h
    src/msrp/MsrpSession.h
    src/msrp/MsrpPayloadDispatcher.h
    src/msrp/MessagingTransportPolicy.h
    src/msrp/MsrpSipIntegration.h
    src/gui/theme/ThemeManager.h
    src/gui/widgets/FlowLayout.h
    src/gui/widgets/StatusCard.h
    src/gui/widgets/AppStatusBar.h
    src/gui/dialogs/SipProfileEditorDialog.h
    src/gui/dialogs/IncomingCallDialog.h
    src/gui/dialogs/MediaRequestDialog.h
    src/core/Logger.h
    src/core/AppSettings.h
    src/core/ContactStore.h
    src/core/CallHistoryEntry.h
    src/core/CallHistoryStore.h
    src/core/CallHistoryRecorder.h
    src/core/CallHistoryListModel.h
    src/core/CallHistoryFilterProxyModel.h
    src/core/DiagnosticsSnapshot.h
    src/core/DiagnosticsCollector.h
    src/core/DiagnosticsBundleExporter.h
    src/core/DiagnosticsTimelineEntry.h
    src/core/DiagnosticsTimelineModel.h
    src/core/DiagnosticsTimelineFilterProxyModel.h
    src/core/DiagnosticsTimelineExporter.h
    src/core/DiagnosticsTimelineService.h
    src/gui/panels/DiagnosticsCenterPanel.h
    src/sip/SipManager.h
    src/sip/SipAccount.h
    src/sip/SipCall.h
    src/sip/SipUriNormalizer.h
    src/sip/CodecManager.h
    src/sip/PjsipAudioMapper.h
    src/sip/CallStateMachine.h
    src/sip/CallMediaOptions.h
    src/sip/RegistrationStateMachine.h
    src/sip/RegistrationRetryPolicy.h
    src/sip/RegistrationRefreshConfig.h
    src/sip/SipProfile.h
    src/sip/SipProfileManager.h
    src/security/ICredentialBackend.h
    src/security/CredentialStore.h
    src/security/MemoryCredentialBackend.h
    src/media/AudioMediaManager.h
    src/media/VideoMediaManager.h
    src/media/MediaDeviceManager.h
    src/media/MediaDeviceSelectionModel.h
    src/media/MediaDevice.h
    src/media/IMediaDeviceBackend.h
    src/media/QtMediaDeviceBackend.h
    src/media/PjsipGdiRenderer.h
    src/media/VideoSettings.h
    src/media/VideoQualityManager.h
    src/media/VideoStatistics.h
    src/media/VideoPipelineMonitor.h
    src/gui/panels/VideoSettingsPanel.h
    src/gui/panels/MediaSettingsPanel.h
    src/rtt/RttSession.h
)

if(WIN32)
    list(APPEND SOURCES src/security/WindowsCredentialBackend.cpp)
    list(APPEND HEADERS src/security/WindowsCredentialBackend.h)
endif()

set(RESOURCES
    resources/resources.qrc
)

qt_add_executable(SIPClient
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_link_libraries(SIPClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Network
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Svg
)

if(HAVE_QT_ZIP_WRITER)
    target_link_libraries(SIPClient PRIVATE Qt6::CorePrivate)
    target_compile_definitions(SIPClient PRIVATE HAVE_QT_ZIP_WRITER)
endif()

if(WIN32)
    target_link_libraries(SIPClient PRIVATE Advapi32 Psapi)
endif()

if(ENABLE_PJSIP AND PJSIP_FOUND)
    set(_PJSIP_BIN_DIR "${CMAKE_SOURCE_DIR}/.deps/pjsip-msvc-install/bin")
    set(_PJSIP_THIRD_PARTY_DIR "${_PJSIP_BIN_DIR}/pjproject/third_party")

    target_compile_definitions(SIPClient PRIVATE PJMEDIA_HAS_VIDEO=1)

    target_link_libraries(SIPClient PRIVATE
        "${_PJSIP_BIN_DIR}/pjlib.lib"
        "${_PJSIP_THIRD_PARTY_DIR}/resample.lib"
        "${_PJSIP_THIRD_PARTY_DIR}/g7221.lib"
        "${_PJSIP_THIRD_PARTY_DIR}/gsm.lib"
        "${_PJSIP_THIRD_PARTY_DIR}/ilbc.lib"
        "${_PJSIP_THIRD_PARTY_DIR}/speex.lib"
        "${_PJSIP_THIRD_PARTY_DIR}/srtp.lib"
        "${_PJSIP_THIRD_PARTY_DIR}/webrtc.lib"
        "${_PJSIP_THIRD_PARTY_DIR}/webrtc_aec3.lib"
        "${_PJSIP_THIRD_PARTY_DIR}/yuv.lib"
    )
    target_link_libraries(SIPClient PRIVATE PJSIP::pjsua2)
    if(PJSIP_LIBRARIES)
        target_link_libraries(SIPClient PRIVATE ${PJSIP_LIBRARIES})
    endif()
    if(TARGET Pj::pjlib)
        target_link_libraries(SIPClient PRIVATE Pj::pjlib)
    endif()
endif()

target_include_directories(SIPClient PRIVATE src "${CMAKE_BINARY_DIR}/generated")

# --- Optional: tests ---
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Deploy Qt runtime (Windows) ---
# Runs windeployqt after every build so plugins (multimedia, platforms, etc.)
# are always present next to the executable. Without this, QMediaDevices returns
# empty device lists because windowsmediaplugin.dll is not found.
if(WIN32)
    find_program(WINDEPLOYQT windeployqt6 windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
        NO_DEFAULT_PATH)
    if(WINDEPLOYQT)
        add_custom_command(TARGET SIPClient POST_BUILD
            COMMAND "${WINDEPLOYQT}"
                --no-translations
                --no-system-d3d-compiler
                $<$<CONFIG:Debug>:--debug>$<$<NOT:$<CONFIG:Debug>>:--release>
                "$<TARGET_FILE:SIPClient>"
            COMMENT "Running windeployqt..."
            VERBATIM)
    endif()
endif()

# --- Install ---
install(TARGETS SIPClient
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
