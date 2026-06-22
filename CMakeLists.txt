cmake_minimum_required(VERSION 3.16)

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

# --- Find Qt6 ---
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
    Multimedia
    MultimediaWidgets
)

# --- Sources ---
set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/gui/MainWindow.cpp
    src/gui/panels/NavRail.cpp
    src/gui/panels/SidebarPanel.cpp
    src/gui/panels/CallPanel.cpp
    src/gui/panels/VideoPanel.cpp
    src/gui/panels/RttPanel.cpp
    src/gui/panels/DiagnosticsPanel.cpp
    src/gui/panels/MediaPanel.cpp
    src/gui/widgets/AppStatusBar.cpp
    src/gui/dialogs/SipProfileEditorDialog.cpp
    src/core/Logger.cpp
    src/sip/SipProfileManager.cpp
    src/security/CredentialStore.cpp
    src/security/MemoryCredentialBackend.cpp
    src/media/MediaDeviceManager.cpp
    src/media/MediaDeviceSelectionModel.cpp
    src/media/QtMediaDeviceBackend.cpp
)

set(HEADERS
    src/app/Application.h
    src/gui/MainWindow.h
    src/gui/panels/NavRail.h
    src/gui/panels/SidebarPanel.h
    src/gui/panels/CallPanel.h
    src/gui/panels/VideoPanel.h
    src/gui/panels/RttPanel.h
    src/gui/panels/DiagnosticsPanel.h
    src/gui/panels/MediaPanel.h
    src/gui/widgets/AppStatusBar.h
    src/gui/dialogs/SipProfileEditorDialog.h
    src/core/Logger.h
    src/core/AppSettings.h
    src/sip/SipProfile.h
    src/sip/SipProfileManager.h
    src/security/ICredentialBackend.h
    src/security/CredentialStore.h
    src/security/MemoryCredentialBackend.h
    src/media/MediaDevice.h
    src/media/IMediaDeviceBackend.h
    src/media/QtMediaDeviceBackend.h
    src/media/MediaDeviceManager.h
    src/media/MediaDeviceSelectionModel.h
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
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

if(WIN32)
    target_link_libraries(SIPClient PRIVATE Advapi32)
endif()

target_include_directories(SIPClient PRIVATE src)

# --- Optional: tests ---
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Install ---
install(TARGETS SIPClient
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
