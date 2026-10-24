# QtKeychain (BSD license, design note D5): the system's secure store for
# saved database passwords. Built here, static and without its own
# translations, from a pinned commit.
include(FetchContent)
set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE)
set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
set(BUILD_TEST_APPLICATION OFF CACHE BOOL "" FORCE)
# Linux: libsecret when it is there, else the Secret Service over D-Bus
if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FLOSTUDIO_LIBSECRET QUIET libsecret-1)
    endif()
    set(LIBSECRET_SUPPORT ${FLOSTUDIO_LIBSECRET_FOUND} CACHE BOOL "" FORCE)
endif()
set(_flostudio_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
FetchContent_Declare(qtkeychain
    GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
    GIT_TAG ad7344c45a86a4f66cbafc4b081b5f7b876cb0b7 # 0.15.0
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(qtkeychain)
set(BUILD_SHARED_LIBS ${_flostudio_shared})

flostudio_add_library(FloStudioSecurity Security
    SOURCES
        src/library.cpp
        src/secrets.cpp
    HEADERS
        include/flostudio/security/library.h
        include/flostudio/security/secrets.h
    DEPENDS
        FloStudio::Core
)
target_link_libraries(FloStudioSecurity PRIVATE qt6keychain)
