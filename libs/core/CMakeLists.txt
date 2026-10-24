configure_file(src/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/flostudio/core/version.h" @ONLY)

flostudio_add_library(FloStudioCore Core
    SOURCES
        src/buildinfo.cpp
    HEADERS
        include/flostudio/core/buildinfo.h
        "${CMAKE_CURRENT_BINARY_DIR}/include/flostudio/core/version.h"
)

target_compile_definitions(FloStudioCore PRIVATE
    FLOSTUDIO_QT_STATIC_BUILD=$<BOOL:${FLOSTUDIO_QT_STATIC}>
    FLOSTUDIO_SHARED_LIBS_BUILD=$<BOOL:${FLOSTUDIO_SHARED_LIBS}>)
