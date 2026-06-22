# FindPJSIP.cmake
# ---------------
# Locates a PJSIP/pjsua2 installation.
#
# Search order:
#   1. CMake variable PJSIP_DIR (pass -DPJSIP_DIR=<path> on the cmake command line)
#   2. Environment variable PJSIP_DIR
#   3. pkg-config (Linux / MSYS2)
#   4. Standard system paths
#
# Imported target:  PJSIP::pjsua2
#
# Cache variables set on success:
#   PJSIP_INCLUDE_DIRS   — directories containing pjsua2.hpp and friends
#   PJSIP_LIBRARIES      — all libraries required to link a pjsua2 application
#   PJSIP_FOUND          — TRUE when all required components are present
#
# Usage in CMakeLists.txt:
#   find_package(PJSIP)           # optional — no error if not found
#   find_package(PJSIP REQUIRED)  # fatal error if not found
#
# Building PJSIP from source:
#   See docs/build-windows.md and docs/build-linux.md for detailed instructions.
#   The recommended installation prefix is one that places headers under
#   <prefix>/include/pjsua2.hpp and libraries under <prefix>/lib/.

cmake_minimum_required(VERSION 3.16)

# --- Honour user-supplied hint ---
if(DEFINED PJSIP_DIR)
    set(_PJSIP_SEARCH_HINT "${PJSIP_DIR}")
elseif(DEFINED ENV{PJSIP_DIR})
    set(_PJSIP_SEARCH_HINT "$ENV{PJSIP_DIR}")
endif()

# --- pkg-config (Linux / MSYS2) ---
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(_PJSIP_PC QUIET libpjsua2 libpjsua libpjsip)
endif()

# --- Header search ---
find_path(PJSIP_INCLUDE_DIR
    NAMES pjsua2.hpp
    HINTS
        ${_PJSIP_SEARCH_HINT}/include
        ${_PJSIP_PC_INCLUDE_DIRS}
    PATHS
        /usr/local/include
        /usr/include
        "C:/pjproject/include"
        "C:/pjsip/include"
    DOC "Directory containing pjsua2.hpp"
)

# --- Library search ---
# pjsua2 is the primary C++ wrapper; pjsip-ua / pjsip / pjmedia / pjnath / pj are the rest
set(_PJSIP_REQUIRED_LIBS pjsua2 pjsua pjsip-ua pjsip-simple pjsip
                         pjmedia-audiodev pjmedia-codec pjmedia-videodev pjmedia
                         pjnath pjlib-util pj)

set(PJSIP_LIBRARIES "")
set(_PJSIP_ALL_FOUND TRUE)

foreach(_lib IN LISTS _PJSIP_REQUIRED_LIBS)
    find_library(_PJSIP_LIB_${_lib}
        NAMES ${_lib} lib${_lib}
        HINTS
            ${_PJSIP_SEARCH_HINT}/lib
            ${_PJSIP_PC_LIBRARY_DIRS}
        PATHS
            /usr/local/lib
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            "C:/pjproject/lib"
            "C:/pjsip/lib"
    )
    if(_PJSIP_LIB_${_lib})
        list(APPEND PJSIP_LIBRARIES "${_PJSIP_LIB_${_lib}}")
    else()
        # Only pjsua2 and pjsua are strictly required to detect the installation
        if(_lib STREQUAL "pjsua2" OR _lib STREQUAL "pjsua")
            set(_PJSIP_ALL_FOUND FALSE)
        endif()
    endif()
    mark_as_advanced(_PJSIP_LIB_${_lib})
endforeach()

# --- Set PJSIP_FOUND ---
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PJSIP
    REQUIRED_VARS PJSIP_INCLUDE_DIR
    FAIL_MESSAGE  "PJSIP not found. Pass -DPJSIP_DIR=<path> or set the PJSIP_DIR environment variable to your PJSIP installation root. See docs/build-windows.md for build instructions."
)

if(PJSIP_FOUND AND NOT _PJSIP_ALL_FOUND)
    message(WARNING "PJSIP headers found but some libraries are missing. Linking may fail.")
endif()

if(PJSIP_FOUND)
    set(PJSIP_INCLUDE_DIRS "${PJSIP_INCLUDE_DIR}")
    mark_as_advanced(PJSIP_INCLUDE_DIR PJSIP_LIBRARIES)

    if(NOT TARGET PJSIP::pjsua2)
        add_library(PJSIP::pjsua2 INTERFACE IMPORTED)
        set_target_properties(PJSIP::pjsua2 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${PJSIP_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES      "${PJSIP_LIBRARIES}"
        )
    endif()
endif()
