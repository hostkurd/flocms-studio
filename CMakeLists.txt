cmake_minimum_required(VERSION 3.24)

project(FloStudio
    VERSION 0.0.1
    DESCRIPTION "FloCMS Studio: create, run and change FloCMS projects"
    HOMEPAGE_URL "https://github.com/hostkurd/flocms-studio"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Programs and DLLs in one folder, so Studio and the tests find the
# FloStudio DLLs in the shared-library build on Windows.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

# Until the final release, Studio ships with Qt and the three libraries as
# DLLs / shared libraries (decided 24 Oct: faster CI). Stage 9 turns this OFF
# and links everything, Qt included, into one program (design note D6).
option(FLOSTUDIO_SHARED_LIBS "Build FloStudioCore, FloStudioDatabase and FloStudioSecurity as shared libraries" ON)
option(FLOSTUDIO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (D11)" ON)
option(FLOSTUDIO_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer (D11)" OFF)
option(FLOSTUDIO_BUILD_TESTS "Build the Qt Test suites" ON)

# CI and releases use Qt 6.12. 6.4 is accepted so the project also builds
# with Ubuntu 24.04's own Qt packages.
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "Build in a separate folder, for example: cmake -S . -B build")
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)
qt_standard_project_setup()
# Qt 6.8+: also write a qmldir for QML files in subfolders (the future default).
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FloStudioTarget)

get_target_property(FLOSTUDIO_QT_TYPE Qt6::Core TYPE)
if(FLOSTUDIO_QT_TYPE STREQUAL "STATIC_LIBRARY")
    set(FLOSTUDIO_QT_STATIC ON)
else()
    set(FLOSTUDIO_QT_STATIC OFF)
endif()

# The static Qt for Windows releases is built with the static C++ runtime
# (/MT, see .github/scripts/build_static_qt.py), so Studio.exe needs no
# Visual C++ DLLs. Everything linked into it must use the same runtime.
if(MSVC AND FLOSTUDIO_QT_STATIC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(libs/core)
add_subdirectory(libs/database)
add_subdirectory(libs/security)
add_subdirectory(app)

if(FLOSTUDIO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
