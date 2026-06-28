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
    src/gui/panels/VideoPanel.cpp
    src/gui/CameraController.cpp
    src/gui/panels/RttPanel.cpp
    src/gui/panels/DiagnosticsPanel.cpp
    src/gui/panels/MediaPanel.cpp
    src/gui/panels/SettingsPanel.cpp
    src/gui/panels/SipLadderPage.cpp
    src/gui/SipLadderWidget.cpp
    src/gui/SipMessageDetailsDialog.cpp
    src/gui/theme/ThemeManager.cpp
    src/gui/widgets/FlowLayout.cpp
    src/gui/widgets/StatusCard.cpp
    src/sip/SipTraceLogger.cpp
    src/gui/widgets/AppStatusBar.cpp
    src/gui/dialogs/SipProfileEditorDialog.cpp
    src/gui/dialogs/IncomingCallDialog.cpp
    src/core/Logger.cpp
    src/core/ContactStore.cpp
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
    src/gui/panels/VideoPanel.h
    src/gui/CameraController.h
    src/gui/panels/RttPanel.h
    src/gui/panels/DiagnosticsPanel.h
    src/gui/panels/MediaPanel.h
    src/gui/panels/SettingsPanel.h
    src/gui/panels/SipLadderPage.h
    src/gui/SipLadderWidget.h
    src/gui/SipMessageDetailsDialog.h
    src/sip/SipMessageTrace.h
    src/sip/SipTraceLogger.h
    src/gui/theme/ThemeManager.h
    src/gui/widgets/FlowLayout.h
    src/gui/widgets/StatusCard.h
    src/gui/widgets/AppStatusBar.h
    src/gui/dialogs/SipProfileEditorDialog.h
    src/gui/dialogs/IncomingCallDialog.h
    src/core/Logger.h
    src/core/AppSettings.h
    src/core/ContactStore.h
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

target_include_directories(SIPClient PRIVATE src)

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
