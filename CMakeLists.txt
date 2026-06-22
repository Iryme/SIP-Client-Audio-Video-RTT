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
    src/gui/widgets/AppStatusBar.cpp
    src/core/Logger.cpp
    src/diagnostics/DiagnosticsLogger.cpp
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
    src/gui/widgets/AppStatusBar.h
    src/core/Logger.h
    src/core/AppSettings.h
    src/diagnostics/LogLevel.h
    src/diagnostics/LogCategory.h
    src/diagnostics/LogEntry.h
    src/diagnostics/DiagnosticsLogger.h
)

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
