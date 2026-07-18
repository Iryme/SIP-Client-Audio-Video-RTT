if(NOT EXISTS "${INFO_PLIST}")
    message(FATAL_ERROR "Configured Info.plist is missing: ${INFO_PLIST}")
endif()
file(READ "${INFO_PLIST}" _plist)
foreach(_required IN ITEMS
        "<string>${EXPECTED_VERSION}</string>"
        "LSMinimumSystemVersion"
        "<string>${EXPECTED_DEPLOYMENT_TARGET}</string>"
        "NSMicrophoneUsageDescription"
        "Microphone access is required for audio calls."
        "NSCameraUsageDescription"
        "Camera access is required for video calls.")
    string(FIND "${_plist}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "Info.plist is missing required value: ${_required}")
    endif()
endforeach()
if(NOT EXISTS "${PACKAGING_SCRIPT}")
    message(FATAL_ERROR "macOS packaging script is missing")
endif()
file(READ "${PACKAGING_SCRIPT}" _script)
foreach(_required IN ITEMS "set -euo pipefail" "macdeployqt" "codesign" "version-info.json")
    string(FIND "${_script}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "Packaging script is missing required operation: ${_required}")
    endif()
endforeach()
